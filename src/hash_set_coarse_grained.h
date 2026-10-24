#ifndef HASH_SET_COARSE_GRAINED_H
#define HASH_SET_COARSE_GRAINED_H

#include <atomic>
#include <cassert>
#include <mutex>
#include <vector>

#include "src/hash_set_base.h"

template <typename T>
class HashSetCoarseGrained : public HashSetBase<T> {
 public:
  explicit HashSetCoarseGrained(size_t initial_capacity)
      : cap_{initial_capacity} {
    buckets_.resize(cap_);
  }

  bool Add(T elem) final {
    std::scoped_lock lock(mtx_);

    auto& bucket = buckets_[bucket_index(elem)];
    if (vec_contains(bucket, elem)) return false;

    bucket.push_back(std::move(elem));
    size_++;

    if (policy()) resize();
    return true;
  }

  bool Remove(T elem) final {
    std::scoped_lock<std::mutex> lock(mtx_);

    auto& bucket = buckets_[bucket_index(elem)];
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) return false;
    size_t i = it - bucket.begin();

    std::swap(bucket[i], bucket.back());
    bucket.pop_back();
    size_--;
    return true;
  }

  [[nodiscard]] bool Contains(T elem) final {
    std::scoped_lock<std::mutex> lock(mtx_);

    return vec_contains(buckets_[bucket_index(elem)], elem);
  }

  [[nodiscard]] size_t Size() const final {
    std::scoped_lock<std::mutex> lock(mtx_);

    return size_;
  }

 private:
  bool vec_contains(const std::vector<T>& vec, const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  bool policy() const {
    return static_cast<double>(size_) > 0.7 * static_cast<double>(cap_);
  }

  void resize() {
    cap_ *= 2;
    std::vector<std::vector<T>> new_buckets(cap_);

    for (const auto& bucket : buckets_) {
      for (const auto& elem : bucket) {
        new_buckets[bucket_index(elem)].push_back(std::move(elem));
      }
    }

    buckets_ = std::move(new_buckets);
  }

  size_t bucket_index(const T& elem) const {
    return std::hash<T>()(elem) % cap_;
  }

  std::mutex mtx_;
  size_t size_;
  size_t cap_;
  std::vector<std::vector<T>> buckets_;
};

#endif  // HASH_SET_COARSE_GRAINED_H
