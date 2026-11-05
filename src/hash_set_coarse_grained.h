#ifndef HASH_SET_COARSE_GRAINED_H
#define HASH_SET_COARSE_GRAINED_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "src/hash_set_base.h"

template <typename T>
class HashSetCoarseGrained : public HashSetBase<T> {
 public:
  explicit HashSetCoarseGrained(size_t initial_capacity)
      : buckets_(std::max<size_t>(initial_capacity, 1)) {}

  bool Add(T elem) final {
    // using RAII wrapper for lock management, this diverges from
    // the book extract as it is more idiomatic in c++
    std::unique_lock<std::mutex> lock(mtx_);

    auto& bucket = BucketFor(elem);
    if (VecContains(bucket, elem)) {
      return false;
    }

    bucket.push_back(std::move(elem));
    size_++;

    // Don't want to release the lock at this point,
    // the book permits a race condition in its implementation
    // the access to 'table.length' without the lock is racy in fig 13.3
    if (Policy()) {
      Resize();
    }
    return true;
  }

  bool Remove(T elem) final {
    std::unique_lock<std::mutex> lock(mtx_);

    auto& bucket = BucketFor(elem);
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) {
      return false;
    }

    bucket.erase(it);
    size_--;
    return true;
  }

  [[nodiscard]] bool Contains(T elem) final {
    std::unique_lock<std::mutex> lock(mtx_);

    return VecContains(BucketFor(elem), elem);
  }

  [[nodiscard]] size_t Size() const final {
    // acquire global lock to prevent races on size_
    std::unique_lock<std::mutex> lock(mtx_);

    return size_;
  }

 private:
  bool VecContains(const std::vector<T>& vec, const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  bool Policy() const { return size_ / buckets_.size() > 4; }

  void Resize() {
    size_t old_cap = buckets_.size();
    std::vector<std::vector<T>> new_buckets(old_cap * 2);

    for (auto& bucket : buckets_) {
      for (auto& elem : bucket) {
        new_buckets[std::hash<T>()(elem) % new_buckets.size()].push_back(std::move(elem));
      }
    }

    buckets_ = std::move(new_buckets);
  }

  std::vector<T>& BucketFor(const T& elem) {
    return buckets_[std::hash<T>()(elem) % buckets_.size()];
  }

  mutable std::mutex mtx_;
  size_t size_{0};
  std::vector<std::vector<T>> buckets_;
};

#endif  // HASH_SET_COARSE_GRAINED_H
