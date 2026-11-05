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
  ScopedVectorLock(std::vector<std::mutex>& mtxs)
      : mtxs_{mtxs} {
    for (auto& mtx : mtxs_) {
      mtx.lock();
    }
  }

  ~ScopedVectorLock() {
    for (auto& mtx : mtxs_) {
      mtx.unlock();
    }
  }

 private:
  std::vector<std::mutex>& mtxs_;
};

template <typename T>
class HashSetStriped : public HashSetBase<T> {
 public:
  explicit HashSetStriped(size_t initial_capacity)
      : cap_{initial_capacity}, buckets_(initial_capacity), mtxs_(initial_capacity) {}

  bool Add(T elem) final {
    std::unique_lock<std::mutex> lock(mtx(elem));

    auto& bucket = BucketFor(elem);
    if (VecContains(bucket, elem)) {
      return false;
    }

    bucket.push_back(std::move(elem));
    size_.fetch_add(1);
    bool need_resize = Policy();
    lock.unlock();

    if (need_resize) {
      ScopedVectorLock global_lock(mtxs_);
      if (Policy()) {
        Resize();
      }
    }

    return true;
  }

  bool Remove(T elem) final {
    std::scoped_lock<std::mutex> lock(mtx(elem));

    auto& bucket = BucketFor(elem);
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
    std::scoped_lock<std::mutex> lock(mtx(elem));

    return VecContains(BucketFor(elem), elem);
  }

  [[nodiscard]] size_t Size() const final { return size_.load(); }

 private:
  bool VecContains(const std::vector<T>& vec, const T& target) const {
    return std::find(vec.begin(), vec.end(), target) != vec.end();
  }

  bool Policy() const { return (size_.load() / cap_) > 4; }

  void Resize() {
    size_t new_cap = buckets_.size() * 2;


    std::vector<std::vector<T>> new_buckets(new_cap);

    for (auto& bucket : buckets_) {
      for (auto& elem : bucket) {
        new_buckets[std::hash<T>()(elem) % new_cap].push_back(std::move(elem));
      }
    }

    buckets_ = std::move(new_buckets);
    cap_ = new_cap;
  }

  std::vector<T>& BucketFor(const T& elem) {
    return buckets_[std::hash<T>()(elem) % buckets_.size()];
  }


  std::mutex& mtx(const T& elem) {
    return mtxs_[std::hash<T>()(elem) % mtxs_.size()];
  }

  std::atomic<size_t> size_{0};
  std::atomic<size_t> cap_{0};
  std::vector<std::vector<T>> buckets_;
  mutable std::vector<std::mutex> mtxs_;
};

#endif  // HASH_SET_STRIPED_H
