#ifndef HASH_SET_REFINABLE_H
#define HASH_SET_REFINABLE_H

#include <cassert>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "src/hash_set_base.h"

template <typename T>
class HashSetRefinable : public HashSetBase<T> {
 public:
  explicit HashSetRefinable(size_t initial_capacity)
    : buckets_(std::max<size_t>(initial_capacity, 1)) {
      all_lock_arrays_.push_back(std::make_unique<LockArray>(initial_capacity));
      current_locks_.store(all_lock_arrays_.back().get());
    }

  bool Add(T elem) final {
    {
      std::unique_lock<std::mutex> lock = AcquireStripe(elem);

      auto& bucket = BucketFor(elem);
      if (VecContains(bucket, elem)) {
        return false;
      }
      bucket.push_back(std::move(elem));

      // relaxed ordering since mutexes handle our synchronisation
      size_.fetch_add(1, std::memory_order_relaxed);
      
      if (!Policy()) return true;
    }
    
    Resize();
    return true;
  }

  bool Remove(T elem) final {
    std::unique_lock<std::mutex> lock = AcquireStripe(elem);

    auto& bucket = BucketFor(elem);
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) {
      return false;
    }
    bucket.erase(it);

    // relaxed ordering since mutexes handle our synchronisation
    size_.fetch_sub(1, std::memory_order_relaxed);
    return true;
  }

  [[nodiscard]] bool Contains(T elem) final {
    // we don't want anyone to modify the buckets while we're checking for
    // elem inside of it
    std::unique_lock<std::mutex> lock = AcquireStripe(elem);
    return VecContains(BucketFor(elem), elem);
  }

  [[nodiscard]] size_t Size() const final {
  // relaxed since returned value can't be used for synchronisation anyway, its just an observation
    return size_.load(std::memory_order_relaxed);
  }

 private:
  using LockArray = std::vector<std::mutex>;

  std::unique_lock<std::mutex> AcquireStripe(const T& elem) {
    for (;;) {
      // relaxed since synchronisation is mostly handled by locks
      // do we even need this line?? it puts contention on resizing_ on every acquire
      // even though resizes are rare
      while (resizing_.load(std::memory_order_relaxed)) {}
      
      // acquire loads since doesn't resizing acquire provide the necessary synchronisation
      // (someone could start resizing after our loop above finishes)
      // so synchronise with the release store of current locks so we get a value in valid state
      LockArray* locks = current_locks_.load(std::memory_order_acquire);
      std::unique_lock<std::mutex> lock((*locks)[std::hash<T>()(elem) % locks->size()]);
      if (!resizing_.load(std::memory_order_acquire) && current_locks_.load() == locks) {
        return lock;
      }
    }
  }

  // relaxed ordering since mutexes handle our synchronisation
  // Policy being called inside resize assumes mutexes held?
  bool Policy() const { return size_.load(std::memory_order_relaxed) / buckets_.size() > 4; }

  void Resize() {
    bool expected = false;
    // relaxed since synchronisation is done by Quiesce
    if (!resizing_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
      return;
    }

    Quiesce();
    // we now have exclusive access to buckets_

    if (Policy()) {
      size_t new_cap = buckets_.size() * 2;
      std::vector<std::vector<T>> new_buckets(new_cap);
      for (auto& bucket : buckets_) {
        for (auto& elem : bucket) {
          new_buckets[std::hash<T>()(elem) % new_cap].push_back(
              std::move(elem));
        }
      }
      buckets_ = std::move(new_buckets);

      all_lock_arrays_.push_back(std::make_unique<LockArray>(new_cap));
      // release store to publicise state updates to all_lock_arrays_
      current_locks_.store(all_lock_arrays_.back().get(), std::memory_order_release);
    }

    // release order since we want new buckets to be visible to new Adds / Removals
    resizing_.store(false, std::memory_order_release);
  }

  bool VecContains(const std::vector<T>& vec, const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  std::vector<T>& BucketFor(const T& elem) {
    return buckets_[std::hash<T>()(elem) % buckets_.size()];
  }

  void Quiesce() {
    // relaxed order since synchronisation provided by acquiring resize
    for (auto& m : *current_locks_.load(std::memory_order_relaxed)) {
      m.lock();
      m.unlock();
    }
  }

  std::atomic<size_t> size_{0};
  std::vector<std::vector<T>> buckets_;
  // We don't need to track the "owning" thread, just whether someone is resizing
  // since our Resize method is private and doesn't support reentrancy
  std::atomic<bool> resizing_{false};
  // diverge from the book: the implementation exposes a race condition where a thread can access
  // the array of locks whilst it is in an invalid state, this pair of members
  // allows us to ensure AcquireStripe will only acquire a valid stripe
  std::atomic<LockArray*> current_locks_{nullptr};
  std::vector<std::unique_ptr<LockArray>> all_lock_arrays_;
};


#endif  // HASH_SET_REFINABLE_H
