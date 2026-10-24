#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <cassert>

#include "src/hash_set_base.h"

template <typename T>
class HashSetSequential : public HashSetBase<T> {
 public:
  explicit HashSetSequential(size_t initial_capacity)
      : capacity_(initial_capacity), table_(initialBuckets) {}

  // if elem in respective bucket, return false; else pushback to vector for
  // that bucket
  // Also checks if resize is needed
  bool Add(T elem) final {
    std::vector<T>& bucket = BucketFor(elem);
    // Bucket found, because iterator didn't reach end
    if (std::find(bucket.begin(), bucket.end(), elem) != bucket.end = ()) {
      return false;
    }
    bucket.push_back(elem);
    size_ += 1;

    if (ResizeRule()) {
      ReSize();
    }
    return true;
  }

  // if elem in respective bucket, remove it and return true; else return false
  bool Remove(T elem) final {
    std::vector<T>& bucket = BucketFor(elem);
    auto elemPos = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) {
      return false;
    }
    bucket.erase(it);
    size_ -= 1;
    return true;
  }

  // if elem in respective bucket, return true, else false
  [[nodiscard]] bool Contains(T /*elem*/) final {
    if (std::find(bucket.begin(), bucket.end(), elem) != bucket.end = ()) {
      return false;
    }
    return true;
  }

  // return size - also remember to update size everywhere else
  [[nodiscard]] size_t Size() const final { return size_; }

 private:
  const size_t initialBuckets = 4;
  size_t capacity_;
  size_t size_ = 0;
  std::vector<std::vector<T>> table_;

  // Uses the hash to return pointer to the appropriate bucket
  std::vector<T>& BucketFor(T& /*elem*/) {}

  // ReSize Rule
  bool ResizeRule() {}

  // double the capacity and rehash every element.
  void Resize() {}
};

#endif  // HASH_SET_SEQUENTIAL_H