#include "test-common.h"

#include <chrono>

void GenerateOps(vector<int64_t>* keys, vector<Op>* ops, int64_t num_ops, int64_t max_key) {
  for (int64_t i = 0; i < num_ops; ++i) {
    keys->push_back(rand() % max_key);
    ops->push_back((Op)(rand() % MAX_OP));
  }
}

struct TestOp {
  Op op;
  int64_t key;
};

template<typename Tree>
int64_t TestPerf(const char* name, const vector<TestOp>& ops, int num_iters) {
  printf("Testing %s", name);
  int64_t num_finds = 0;

  auto start = chrono::high_resolution_clock::now();
  for (int i = 0; i < num_iters; ++i) {
    Tree tree;
    for (int64_t j = 0; j < ops.size(); ++j) {
      switch (ops[j].op) {
        case FIND:
          if (Find(&tree, ops[j].key)) ++num_finds;
          break;
        case UPDATE:
          tree.Update(ops[j].key, NULL);
          break;
        case INSERT:
          Insert(&tree, ops[j].key);
          break;
        case UPSERT:
          Upsert(&tree, ops[j].key);
          break;
        case REMOVE:
          tree.Remove(ops[j].key);
          break;
        default:
          printf("Unknown op\n");
          exit(1);
      }
    }
  }
  auto end = chrono::high_resolution_clock::now();
  auto seconds = chrono::duration_cast<chrono::duration<float>>(end - start);
  int64_t transactions = ops.size() * num_iters;
  printf(": %0.3f kTPS\n", transactions / seconds.count() / 1000.);
  return num_finds;
}

vector<TestOp> GenerateOps(int64_t num_ops, int64_t max_key,
    int percentFind, int percentInsert) {
  if (percentFind + percentInsert > 100) {
    printf("percentFind and percentInsert sum to > 100.\n");
    exit(1);
  }

  vector<TestOp> ops;
  for (auto i = 0; i < num_ops; ++i) {
    int rand_op = rand() % 100;
    int64_t key = rand() % max_key;
    if (rand_op < percentFind) {
      ops.push_back(TestOp{FIND, key});
    } else if (rand_op < percentFind + percentInsert) {
      ops.push_back(TestOp{INSERT, key});
    } else {
      ops.push_back(TestOp{REMOVE, key});
    }
  }
  return ops;
}

// Running single threaded benchmark.
//   Find: 70%   Insert: 20%   Remove: 10%
// Testing btree: 9810.000 kTPS
// Testing btree_v1: 9619.339 kTPS
// Testing std map: 5870.611 kTPS
// Testing std unordered map: 38524.896 kTPS
int main(int argc, char** argv) {
  srand(0);
  const int num_iters = 5;

  const int percent_find = 70;
  const int percent_insert = 20;

  string mode = "st";
  if (argc == 2) mode = argv[1];
  if (mode == "st") {
    printf("Running single threaded benchmark.\n");
    vector<TestOp> ops = GenerateOps(5000000L, 50000, percent_find, percent_insert);
    printf("  Find: %d%%   Insert: %d%%   Remove: %d%%\n",
        percent_find, percent_insert, 100 - percent_find - percent_insert);
    int64_t btree_finds = TestPerf<BTree>("btree", ops, num_iters);
    TestPerf<BTreeV1>("btree_v1", ops, num_iters);
    int64_t map_finds = TestPerf<StdMap>("std map", ops, num_iters);
    TestPerf<StdUnorderedMap>("std unordered map", ops, num_iters);
    if (btree_finds != map_finds) {
      printf("Incorrect results: %ld != %ld\n", btree_finds, map_finds);
      exit(1);
    }
  } else {
    printf("Unknown mode.\n");
    exit(1);
  }
  printf("Done.\n");
  return 0;
}
