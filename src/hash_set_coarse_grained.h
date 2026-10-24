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
      : cap_{initial_capacity}, buckets_(cap_) {}

  bool Add(T elem) final {
    std::unique_lock<std::mutex> lock(mtx_);

    auto& bucket = buckets_[bucket_index(elem)];
    if (vec_contains(bucket, elem)) {
      return false;
    }

    bucket.push_back(std::move(elem));
    size_++;

    if (policy()) {
      resize();
    }
    return true;
  }

  bool Remove(T elem) final {
    std::unique_lock<std::mutex> lock(mtx_);

    auto& bucket = buckets_[bucket_index(elem)];
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) {
      return false;
    }

    std::iter_swap(it, std::prev(bucket.end()));
    bucket.pop_back();
    size_--;
    return true;
  }

  [[nodiscard]] bool Contains(T elem) final {
    std::unique_lock<std::mutex> lock(mtx_);

    return vec_contains(buckets_[bucket_index(elem)], elem);
  }

  [[nodiscard]] size_t Size() const final {
    std::unique_lock<std::mutex> lock(mtx_);

    return size_;
  }

 private:
  bool vec_contains(const std::vector<T>& vec, const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  bool policy() const { return (size_ / cap_) > 4; }

  void resize() {
    size_t new_cap = cap_ * 2;
    std::vector<std::vector<T>> new_buckets(new_cap);

    for (auto& bucket : buckets_) {
      for (auto& elem : bucket) {
        new_buckets[std::hash<T>()(elem) % new_cap].push_back(std::move(elem));
      }
    }

    buckets_ = std::move(new_buckets);
    cap_ = new_cap;
  }

  size_t bucket_index(const T& elem) const {
    return std::hash<T>()(elem) % cap_;
  }

  mutable std::mutex mtx_;
  size_t size_{0};
  size_t cap_{0};
  std::vector<std::vector<T>> buckets_;
};

#endif  // HASH_SET_COARSE_GRAINED_H
