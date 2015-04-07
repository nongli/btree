#ifndef STD_TREE_H
#define STD_TREE_H

#include <stdio.h>
#include <cstdint>

#include <map>
#include <unordered_map>

template<typename T>
class StdTree {
 public:
  class Iterator {
   public:
    bool AtEnd() { return parent_ == NULL; }

   private:
    friend class StdTree;
    Iterator(const StdTree* parent = NULL, void* value = NULL)
      : parent_(parent), value_(value) {
    }

    const StdTree* parent_;
    void* value_;
  };

  Iterator Find(int64_t key) const {
    typename T::const_iterator it = map_.find(key);
    if (it == map_.end()) return End();
    return Iterator(this, it->second);
  }

  bool Update(int64_t key, void* value) {
    typename T::const_iterator it = map_.find(key);
    if (it == map_.end()) return false;
    map_[key] = value;
    return true;
  }

  Iterator Insert(int64_t key, void* value) {
    typename T::const_iterator it = map_.find(key);
    if (it != map_.end()) return End();
    map_[key] = value;
    return Iterator(this, value);
  }

  Iterator Upsert(int64_t key, void* value) {
    map_[key] = value;
    return Iterator(this, value);
  }

  bool Remove(int64_t key) {
    typename T::iterator it = map_.find(key);
    if (it == map_.end()) return false;
    map_.erase(it);
    return true;
  }

  int64_t size() const {
    return map_.size();
  }

  void DebugPrint() const {}
  void CollectAllKeys(std::vector<int64_t>* keys, bool backwards = false) const {
    keys->clear();
    for (auto& v: map_) {
      keys->push_back(v.first);
    }
    if (backwards) std::reverse(keys->begin(), keys->end());
  }

  Iterator End() const { return Iterator(); }

 private:
  T map_;
};

typedef StdTree<std::map<int64_t, void*>> StdMap;
typedef StdTree<std::unordered_map<int64_t, void*>> StdUnorderedMap;

#endif
