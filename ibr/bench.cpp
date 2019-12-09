#include <thread>

// #include "BenchmarkLists.hpp"
#include "Benchmark.hpp"

char LINKED_LIST = 'L';
char QUEUE = 'Q';
char STACK = 'S';

int main(void) {
    // BenchmarkLists::allThroughputTests();
    Benchmarks::allThroughputTests();
    return 0;
}