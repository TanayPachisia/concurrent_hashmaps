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
  ScopedVectorLock(std::vector<std::unique_ptr<std::mutex>>& mtxs)
      : mtxs_{mtxs} {
    for (auto& mtx : mtxs_) {
      mtx->lock();
    }
  }

  ~ScopedVectorLock() {
    for (auto& mtx : mtxs_) {
      mtx->unlock();
    }
  }

 private:
  std::vector<std::unique_ptr<std::mutex>>& mtxs_;
};

template <typename T>
class HashSetStriped : public HashSetBase<T> {
 public:
  explicit HashSetStriped(size_t initial_capacity)
      : cap_{initial_capacity}, buckets_(cap_), mtxs_(cap_) {
    for (size_t i = 0; i < mtxs_.size(); i++) {
      mtxs_[i] = std::make_unique<std::mutex>();
    }
  }

  bool Add(T elem) final {
    std::unique_lock<std::mutex> lock(mtx(elem));

    auto& bucket = buckets_[bucket_index(elem)];
    if (vec_contains(bucket, elem)) {
      return false;
    }

    bucket.push_back(std::move(elem));
    lock.unlock();

    ScopedVectorLock global_lock(mtxs_);
    if (policy()) {
      resize();
    }

    return true;
  }

  bool Remove(T elem) final {
    std::unique_lock<std::mutex> lock(mtx(elem));

    auto& bucket = buckets_[bucket_index(elem)];
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) {
      return false;
    }

    std::iter_swap(it, std::prev(bucket.end()));
    bucket.pop_back();
    return true;
  }

  [[nodiscard]] bool Contains(T elem) final {
    std::unique_lock<std::mutex> lock(mtx(elem));

    return vec_contains(buckets_[bucket_index(elem)], elem);
  }

  [[nodiscard]] size_t Size() const final {
    ScopedVectorLock lock(mtxs_);

    return get_size();
  }

 private:
  size_t get_size() const {
    size_t size = 0;

    for (auto& bucket : buckets_) {
      size += bucket.size();
    }

    return size;
  }

  bool vec_contains(const std::vector<T>& vec, const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  bool policy() const { return (get_size() / cap_) > 4; }

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

  std::mutex& mtx(const T& elem) {
    return *(mtxs_[std::hash<T>()(elem) % mtxs_.size()]);
  }

  size_t cap_{0};
  std::vector<std::vector<T>> buckets_;
  mutable std::vector<std::unique_ptr<std::mutex>> mtxs_;
};

#endif  // HASH_SET_STRIPED_H
