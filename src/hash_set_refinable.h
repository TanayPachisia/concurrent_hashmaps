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
    : cap_(initial_capacity), buckets_(cap_) {
      all_locks_arrays_.push_back(std::make_unique<LockArray>(cap_));
      current_lock_.store(all_locks_arrays_.back().get());
    }

  bool Add(T elem) final {
    size_t hash = std::hash<T>()(elem);
    bool need_resize;
    {
      std::unique_lock<std::mutex> lock = AcquireStripe(hash);
      auto& bucket = buckets_[hash % cap_];
      if (VecContains(bucket, elem)) {
        return false;
      }
      bucket.push_back(std::move(elem));
      size_.fetch_add(1);
      need_resize = Policy();
    }
    if (need_resize) {
      Resize();
    }
    return true;
  }

  bool Remove(T elem) final {
    size_t hash = std::hash<T>()(elem);
    std::unique_lock<std::mutex> lock = AcquireStripe(hash);
    auto& bucket = buckets_[hash % cap_];
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) {
      return false;
    }
    std::iter_swap(it, std::prev(bucket.end()));
    bucket.pop_back();
    size_.fetch_sub(1);
    return true;
  }

  [[nodiscard]] bool Contains(T elem) final {
    size_t hash = std::hash<T>()(elem);
    std::unique_lock<std::mutex> lock = AcquireStripe(hash);
    return VecContains(buckets_[hash % cap_], elem);

  }

  [[nodiscard]] size_t Size() const final {
    return size_.load();
  }

 private:
  using LockArray = std::vector<std::mutex>;

  std::unique_lock<std::mutex> AcquireStripe(size_t hash) {
    for (;;) {
      resizing_.wait(true);
      LockArray* locks = current_locks_.load();
      std::unique_lock<std::mutex> lock((*locks)[hash % locks->size()]);
      if (!resizing_.load() && current_locks_.load() == locks) {
        return lock;
      }
    }
  }

  [[nodiscard]] bool VecContains(const std::vector<T>& vec,
                                 const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  [[nodiscard]] bool Policy() const { return size_.load() / cap_ > 4; }

  void Resize() {
    bool expected = false;
    if (!resizing_.compare_exchange_strong(expected, true)) {
      return;
    }

    for (std::mutex& m : *current_locks_.load()) {
      m.lock();
      m.unlock();
    }

    if (Policy()) {
      size_t new_cap = cap_ * 2;
      std::vector<std::vector<T>> new_buckets(new_cap);
      for (auto& bucket : buckets_) {
        for (auto& elem : bucket) {
          new_buckets[std::hash<T>()(elem) % new_cap].push_back(
              std::move(elem));
        }
      }
      buckets_ = std::move(new_buckets);
      cap_ = new_cap;

      all_lock_arrays_.push_back(std::make_unique<LockArray>(new_cap));
      current_locks_.store(all_lock_arrays_.back().get());
    }

    resizing_.store(false);
    resizing_.notify_all();
  }

  std::atomic<size_t> size_{0};
  size_t cap_;
  std::vector<std::vector<T>> buckets_;
  std::atomic<bool> resizing_{false};
  std::atomic<LockArray*> current_locks_{nullptr};
  std::vector<std::unique_ptr<LockArray>> all_lock_arrays_;
};


#endif  // HASH_SET_REFINABLE_H
