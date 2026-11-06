#ifndef HASH_SET_STRIPED_H
#define HASH_SET_STRIPED_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "src/hash_set_base.h"

class ScopedVectorLock {
 public:
  ScopedVectorLock(std::vector<std::mutex>& stripes)
      : stripes_{stripes} {
    for (auto& stripe : stripes_) {
      stripe.lock();
    }
  }

  ~ScopedVectorLock() {
    for (auto& stripe : stripes_) {
      stripe.unlock();
    }
  }

 private:
  std::vector<std::mutex>& stripes_;
};

template <typename T>
class HashSetStriped : public HashSetBase<T> {
 public:
  explicit HashSetStriped(size_t initial_capacity)
      : buckets_(std::max<size_t>(initial_capacity, 1)), stripes_(std::max<size_t>(initial_capacity, 1)) {}

  bool Add(T elem) final {
    std::unique_lock<std::mutex> lock(Stripe(elem));

    auto& bucket = BucketFor(elem);
    if (VecContains(bucket, elem)) {
      return false;
    }

    bucket.push_back(std::move(elem));
    // relaxed ordering since mutexes handle our synchronisation
    size_.fetch_add(1, std::memory_order_relaxed);

    if (!Policy()) return true;
    lock.unlock();

    ScopedVectorLock global_lock(stripes_);
    if (Policy()) {
      Resize();
    }
    
    return true;
  }

  bool Remove(T elem) final {
    // acquire the lock fo the element's stripe, acquire semantics
    // on lock, release on unlock (don't expose inconsitent state)
    std::scoped_lock<std::mutex> lock(Stripe(elem));

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
    std::scoped_lock<std::mutex> lock(Stripe(elem));

    return VecContains(BucketFor(elem), elem);
  }

  // relaxed since returned value can't be used for synchronisation anyway, its just an observation
  [[nodiscard]] size_t Size() const final { return size_.load(std::memory_order_relaxed); }

 private:
  bool VecContains(const std::vector<T>& vec, const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  bool Policy() const { 
    // relaxed ordering since mutexes handle our synchronisation
    // accessing buckets_.size is not a race condition since we only change bucket.size()
    // during resizing when global lock is held, nobody can access size when this happens
    return size_.load(std::memory_order_relaxed) / buckets_.size() > 4; 
  }

  void Resize() {
    size_t new_cap = buckets_.size() * 2;
    std::vector<std::vector<T>> new_buckets(new_cap);

    for (auto& bucket : buckets_) {
      for (auto& elem : bucket) {
        new_buckets[std::hash<T>()(elem) % new_cap].push_back(std::move(elem));
      }
    }

    buckets_ = std::move(new_buckets);
  }

  std::vector<T>& BucketFor(const T& elem) {
    return buckets_[std::hash<T>()(elem) % buckets_.size()];
  }


  std::mutex& Stripe(const T& elem) {
    return stripes_[std::hash<T>()(elem) % stripes_.size()];
  }

  std::atomic<size_t> size_{0};
  std::vector<std::vector<T>> buckets_;
  std::vector<std::mutex> stripes_;
};

#endif  // HASH_SET_STRIPED_H
