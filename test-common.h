#ifndef TEST_COMMON_H
#define TEST_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <algorithm>

#include "btree.h"
#include "stdtree.h"

using namespace std;

template<typename T>
bool Insert(T* tree, int64_t key) {
  return !tree->Insert(key, NULL).AtEnd();
}

template<typename T>
bool Upsert(T* tree, int64_t key) {
  return !tree->Upsert(key, NULL).AtEnd();
}

template<typename T>
bool Find(T* tree, int64_t key) {
  return !tree->Find(key).AtEnd();
}

template <typename T>
void PrintAllKeys(const T* tree, bool backwards = false) {
  std::vector<int64_t> keys;
  tree->CollectAllKeys(&keys, backwards);
  printf("Printing all values %s\n", backwards ? "backwards" : "");
  for (size_t i = 0; i < keys.size(); ++i) {
    printf("%ld ", keys[i]);
    if (i != 0) {
      if (backwards) {
        assert(keys[i - 1] > keys[i]);
      } else {
        assert(keys[i - 1] < keys[i]);
      }
    }
  }
  printf("\n");
}

enum Op {
  FIND,
  INSERT,
  UPDATE,
  UPSERT,
  REMOVE,
  MAX_OP
};

#endif

