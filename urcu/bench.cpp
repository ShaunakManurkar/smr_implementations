#include <thread>

#include "Benchmark.hpp"

using namespace std;

int main(int argc, char* argv[]) 
{
    int total_threads;
    cout<<"Total arguments: "<<argc<<"\n\n";
    if(argc <= 1)
    {
        cout<<"Please provide proper arguments\n";        
    }
    else
    {
        char* ds_type = argv[1];
        if(argc > 2)
        {
            total_threads = atoi(argv[2]);
        }
        else
        {
            total_threads = -1;
        }
        Benchmarks::allThroughputTests(ds_type, total_threads);
    }

    return 0;
}