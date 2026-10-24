#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "src/hash_set_base.h"

template <typename T>
class HashSetSequential : public HashSetBase<T> {
 public:
  explicit HashSetSequential(size_t initial_capacity)
      : table_(std::max<size_t>(initial_capacity, initialBuckets)) {}

  // if elem in respective bucket, return false; else pushback to vector for
  // that bucket
  // Also checks if resize is needed
  bool Add(T elem) final {
    std::vector<T>& bucket = BucketFor(elem);
    // Bucket found, because iterator didn't reach end
    if (std::find(bucket.begin(), bucket.end(), elem) != bucket.end()) {
      return false;
    }
    bucket.push_back(std::move(elem));
    size_ += 1;

    if (ResizeRule()) {
      Resize();
    }
    return true;
  }

  // if elem in respective bucket, remove it and return true; else return false
  bool Remove(T elem) final {
    std::vector<T>& bucket = BucketFor(elem);
    auto elem_pos = std::find(bucket.begin(), bucket.end(), elem);
    if (elem_pos == bucket.end()) {
      return false;
    }
    bucket.erase(elem_pos);
    size_ -= 1;
    return true;
  }

  // if elem in respective bucket, return true, else false
  [[nodiscard]] bool Contains(T elem) final {
    std::vector<T>& bucket = BucketFor(elem);
    if (std::find(bucket.begin(), bucket.end(), elem) != bucket.end()) {
      return true;
    }
    return false;
  }

  // return size - also remember to update size everywhere else
  [[nodiscard]] size_t Size() const final { return size_; }

 private:
  const float_t resizeRatio = 4;
  const size_t initialBuckets = 4;
  size_t size_ = 0;
  std::vector<std::vector<T>> table_;

  // Uses the hash to return pointer to the appropriate bucket
  std::vector<T>& BucketFor(T& elem) {
    return table_[std::hash<T>()(elem) % table_.size()];
  }

  // Resize Rule
  bool ResizeRule() { return size_ / table_.size() > 4; }

  // double the capacity and rehash every element.
  void Resize() {
    std::vector<std::vector<T>> new_table(table_.size() * 2);
    for (std::vector<T>& bucket : table_) {
      for (T& elem : bucket) {
        new_table[std::hash<T>()(elem) % new_table.size()].push_back(
            std::move(elem));
      }
    }
    table_ = std::move(new_table);
  }
};

#endif  // HASH_SET_SEQUENTIAL_H
