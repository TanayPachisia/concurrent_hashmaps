#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "src/hash_set_base.h"

constexpr float_t resizeRatio = 4;

// hash set with no thread safety
template <typename T>
class HashSetSequential : public HashSetBase<T> {
 public:
  explicit HashSetSequential(size_t initial_capacity)
        // min needs to have at least 1 elem otherwise doubling size will have no effect (also 0 would result in divide by zero in bucket calculation)
      : table_(std::max<size_t>(initial_capacity, 1)) {}

  bool Add(T elem) final {
    std::vector<T>& bucket = BucketFor(elem);
    // if bucket found (iterator does not reach end), do nothing
    if (std::find(bucket.begin(), bucket.end(), elem) != bucket.end()) {
      return false;
    }

    // move so we don't copy the elem
    bucket.push_back(std::move(elem));
    size_ += 1;

    if (Policy()) {
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

    return std::find(bucket.begin(), bucket.end(), elem) != bucket.end();
  }

  // return size - also remember to update size everywhere else
  [[nodiscard]] size_t Size() const final { return size_; }

 private:
  //resize once the average bucket length exceeds 4 - as per textbook
  static constexpr size_t kMaxAverageBucketLength = 4;
  size_t size_ = 0;
  std::vector<std::vector<T>> table_;

  // Uses the hash to return pointer to the appropriate bucket
  std::vector<T>& BucketFor(T& elem) {
    return table_[std::hash<T>()(elem) % table_.size()];
  }

  // Resize Rule 
  bool Policy() { return size_ / table_.size() > kMaxAverageBucketLength; }

  // Double the capacity and rehash every element, since an element's bucket depends on the number of buckets.
  // No calls to existing public methods
  void Resize() {
    std::vector<std::vector<T>> new_table(table_.size() * 2);
    for (auto& bucket : table_) {
      for (T& elem : bucket) {
        new_table[std::hash<T>()(elem) % new_table.size()].push_back(
            std::move(elem));
      }
    }
    table_ = std::move(new_table);
  }
};

#endif  // HASH_SET_SEQUENTIAL_H
