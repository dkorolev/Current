/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef CURRENT_STORAGE_CONTAINER_DICTIONARY_H
#define CURRENT_STORAGE_CONTAINER_DICTIONARY_H

#include "common.h"
#include "sfinae.h"

#include "../base.h"

#include "../../TypeSystem/optional.h"

namespace current {
namespace storage {
namespace container {

template <typename T, typename ADDER, typename DELETER, template <typename...> class MAP = Unordered>
class Dictionary {
 public:
  using T_KEY = sfinae::ENTRY_KEY_TYPE<T>;
  using T_MAP = MAP<T_KEY, T>;

  explicit Dictionary(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  ImmutableOptional<T> operator[](sfinae::CF<T_KEY> key) const {
    const auto iterator = map_.find(key);
    if (iterator != map_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), &iterator->second);
    } else {
      return nullptr;
    }
  }

  void Add(const T& object) {
    const auto key = sfinae::GetKey(object);
    const auto iterator = map_.find(key);
    if (iterator != map_.end()) {
      const T previous_object = iterator->second;
      journal_.LogMutation(ADDER(object), [this, key, previous_object]() { map_[key] = previous_object; });
    } else {
      journal_.LogMutation(ADDER(object), [this, key]() { map_.erase(key); });
    }
    map_[key] = object;
  }

  void Erase(sfinae::CF<T_KEY> key) {
    const auto iterator = map_.find(key);
    if (iterator != map_.end()) {
      const T previous_object = iterator->second;
      journal_.LogMutation(DELETER(previous_object),
                           [this, key, previous_object]() { map_[key] = previous_object; });
      map_.erase(key);
    }
  }

  void operator()(const ADDER& object) { map_[sfinae::GetKey(object)] = object; }

  void operator()(const DELETER& object) { map_.erase(sfinae::GetKey(object)); }

  struct Iterator final {
    using T_ITERATOR = typename T_MAP::const_iterator;
    T_ITERATOR iterator;
    explicit Iterator(T_ITERATOR iterator) : iterator(std::move(iterator)) {}
    void operator++() { ++iterator; }
    bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    sfinae::CF<T_KEY> key() const { return iterator->first; }
    const T& operator*() const { return iterator->second; }
    const T* operator->() const { return &iterator->second; }
  };

  Iterator begin() const { return Iterator(map_.cbegin()); }
  Iterator end() const { return Iterator(map_.cend()); }

 private:
  T_MAP map_;
  MutationJournal& journal_;
};

}  // namespace container
}  // namespace storage
}  // namespace current

using current::storage::container::Dictionary;

#endif  // CURRENT_STORAGE_CONTAINER_DICTIONARY_H