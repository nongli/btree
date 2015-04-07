all:
	clang++ -std=c++11 -g -Wall test-correctness.cc -o test-correctness
	clang++ -std=c++11 -O3 -DNDEBUG -g -Wall test-perf.cc -o test-perf
