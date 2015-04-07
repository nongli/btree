#include "test-common.h"

template<typename T>
void TestBasicCorrectness(const char* name, int64_t num_keys) {
  printf("Testing %s with %ld keys.\n", name, num_keys);
  vector<int64_t> keys;
  for (int i = 0; i < num_keys; ++i) {
    keys.push_back(i);
  }

  T tree;

  random_shuffle(keys.begin(), keys.end());
  for (int64_t i = 0; i < num_keys; ++i) {
    assert(Insert(&tree, keys[i]));
    //tree.DebugPrint();
    for (int j = 0; j <= i; ++j) {
      assert(Find(&tree, keys[j]));
    }
  }
  assert(tree.size() == num_keys);

  random_shuffle(keys.begin(), keys.end());
  for (int64_t i = 0; i < num_keys; ++i) {
    assert(Find(&tree, keys[i]));
  }

  for (int64_t i = num_keys; i < num_keys + 10; ++i) {
    assert(!Find(&tree, i));
  }

  //tree.DebugPrint();

  // Verify keys are in order.
  vector<int64_t> collected_keys;
  tree.CollectAllKeys(&collected_keys);
  assert(collected_keys.size() == num_keys);
  for (int i = 0; i < num_keys; ++i) {
    assert(collected_keys[i] == i);
  }
  tree.CollectAllKeys(&collected_keys, true);
  assert(collected_keys.size() == num_keys);
  for (int i = 0; i < num_keys; ++i) {
    assert(collected_keys[i] == num_keys - 1 - i);
  }

  random_shuffle(keys.begin(), keys.end());
  for (int64_t i = 0; i < num_keys; ++i) {
    assert(tree.Remove(keys[i]));
    //tree.DebugPrint();

    for (int64_t j = 0; j <= i; ++j) {
      assert(!Find(&tree, keys[j]));
    }
    for (int64_t j = i + 1; j < num_keys; ++j) {
      assert(Find(&tree, keys[j]));
    }
  }

  for (int64_t i = 0; i < num_keys + 1; ++i) {
    assert(!tree.Remove(i));
  }
  assert(tree.size() == 0);
  //tree.DebugPrint();
}

void GenerateOps(vector<int64_t>* keys, vector<Op>* ops, int64_t num_ops, int64_t max_key) {
  for (int64_t i = 0; i < num_ops; ++i) {
    keys->push_back(rand() % max_key);
    ops->push_back((Op)(rand() % MAX_OP));
  }
}

template<typename Tree>
void TestAgainstStl(int64_t num_ops, int64_t max_key) {
  printf("Testing against STL for %ld ops.\n", num_ops);

  vector<int64_t> keys;
  vector<Op> ops;
  GenerateOps(&keys, &ops, num_ops, max_key);

  Tree tree;
  StdMap reference;

  for (int64_t i = 0; i < ops.size(); ++i) {
    switch (ops[i]) {
      case FIND:
        assert(Find(&tree, keys[i]) == Find(&reference, keys[i]));
        break;
      case UPDATE:
        assert(tree.Update(keys[i], NULL) == reference.Update(keys[i], NULL));
        break;
      case INSERT:
        assert(Insert(&tree, keys[i]) == Insert(&reference, keys[i]));
        break;
      case UPSERT:
        assert(Upsert(&tree, keys[i]) == Upsert(&reference, keys[i]));
        break;
      case REMOVE:
        assert(tree.Remove(keys[i]) == reference.Remove(keys[i]));
        break;
      default:
        assert(false);
    }
    assert(tree.size() == reference.size());
  }
}

int main(int argc, char** argv) {
  srand(0);
  string mode = argc == 1 ? "" : argv[1];
  if (mode == "basic") {
    TestBasicCorrectness<StdMap>("std map", 1000);
    TestBasicCorrectness<StdUnorderedMap>("std unordered map", 1000);
    for (int i = 0; i <= 1000; i += 10) {
      //TestBasicCorrectness<BTreeV1>("btree_v1", i);
      TestBasicCorrectness<BTree>("btree", i);
    }
  } else if (mode == "stl") {
    for (int i = 0; i < 10; ++i) {
      TestAgainstStl<BTreeV1>(100000, 100000);
    }
  } else {
    printf("Usage: test-correctness [basic|stl]\n");
    return -1;
  }
  printf("Done.\n");
  return 0;
}
