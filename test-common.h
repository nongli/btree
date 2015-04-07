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

enum Op {
  FIND,
  INSERT,
  UPDATE,
  UPSERT,
  REMOVE,
  MAX_OP
};

#endif

