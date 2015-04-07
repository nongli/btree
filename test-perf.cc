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
  printf("Testing %s\n", name);
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
  printf("  Transactions: %ld\n", transactions);
  printf("  TPS: %0.3f K\n", transactions / seconds.count() / 1000.);
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
// Testing btree
//   Transactions: 25000000
//   TPS: 9737.326 K
// Testing std unordered map
//   Transactions: 25000000
//   TPS: 38972.620 K
int main(int argc, char** argv) {
  srand(0);
  const int num_iters = 5;

  string mode = "st";
  if (argc == 2) mode = argv[1];
  if (mode == "st") {
    printf("Running single threaded benchmark.\n");
    vector<TestOp> ops = GenerateOps(5000000L, 50000, 70, 20);
    int64_t btree_finds = TestPerf<BTree>("btree", ops, num_iters);
    int64_t map_finds = TestPerf<StdUnorderedMap>("std unordered map", ops, num_iters);
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
