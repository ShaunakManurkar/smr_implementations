#ifndef _BENCHMARK_H_
#define _BENCHMARK_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <time.h>

#include "LinkedListURCU.hpp"
#include "QueueURCU.hpp"
#include "StackURCU.hpp"

using namespace std;
using namespace chrono;

class Benchmarks {

private:
    int numThreads;

public:
    Benchmarks(int numThreads) {
        this->numThreads = numThreads;
    }

    template<typename Q>
    long long benchmarkQueues(int update_ratio, seconds test_length, int total_runs, int total_elements) 
    {
        long long ops[numThreads][total_runs];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        Q* queue = nullptr;

        // Create all the objects in the list
        int* elements[total_elements];
        for (int i = 0; i < total_elements; i++) 
        {
            elements[i] = new int(i);
        }

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&update_ratio,&quit,&startFlag,&queue,&total_elements, &elements](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t seed = tid;
            srand(time(NULL));
            while (!startFlag.load()) { } // spin
            while (!quit.load()) {
                // seed = randomLong(seed);
                seed = rand()*total_elements + 1;
                auto ix = (unsigned int)(seed%total_elements);
                // seed = randomLong(seed);
                auto ratio = seed%10000;  // Ratios are in per-10k units
                if (ratio < update_ratio) {
                    if(queue->dequeue(tid) != NULL)
                    {
                        queue->enqueue(elements[ix], tid);
                        numOps+=1;
                    }
                } else {
                    queue->dequeue(tid);
                }
                numOps+=1;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < total_runs; irun++) {
            queue = new Q(numThreads);
            
            for (int i = 0; i < total_elements; i++) 
            {
                queue->enqueue(elements[i], 0);
            }

            if (irun == 0) 
            {
                cout<<"----- Benchmarking Queue ------\n";
            }

            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid] = thread(rw_lambda, &ops[tid][irun], tid);
            }

            startFlag.store(true);
            // Sleep for 100 seconds
            this_thread::sleep_for(test_length);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid].join();
            }

            quit.store(false);
            startFlag.store(false);
            delete queue;
        }

        for (int i = 0; i < total_elements; i++) 
        {
            delete elements[i];
        }

        // Calculating throughput
        vector<long long> agg(total_runs);
        for (int irun = 0; irun < total_runs; irun++) {
            agg[irun] = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
        }

        // Compute the median, max and min. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[total_runs-1];
        auto minops = agg[0];
        auto medianops = agg[total_runs/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));

        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "   delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        return medianops;
    }

    template<typename S>
    long long benchmarkStacks(int update_ratio, seconds test_length, int total_runs, int total_elements) 
    {
        long long ops[numThreads][total_runs];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        S* stack = nullptr;

        // Create all the objects in the list
        int* elements[total_elements];
        for (int i = 0; i < total_elements; i++) 
        {
            elements[i] = new int(i);
        }

        auto rw_lambda = [this,&update_ratio,&quit,&startFlag,&stack,&total_elements, &elements](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t seed = tid;
            srand(time(NULL));
            while (!startFlag.load()) { } // spin
            while (!quit.load()) {
                seed = rand()*total_elements + 1;
                auto ix = (unsigned int)(seed%total_elements);
                auto ratio = seed%10000;  // Ratios are in per-10k units
                if (ratio < update_ratio) {
                    stack->push(elements[ix], tid);
                } else {
                    if(stack->pop(tid) != NULL)
                    {
                        stack->push(elements[ix], tid);
                        numOps+=1;
                    }
                }
                numOps+=1;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < total_runs; irun++) {
            stack = new S(numThreads);
            // Add all the items to the list
            for (int i = 0; i < total_elements; i++) 
            {
                stack->push(elements[i], 0);
            }

            if (irun == 0) 
            {
                cout<<"----- Benchmarking Stack ------\n";
            }
            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid] = thread(rw_lambda, &ops[tid][irun], tid);
            }

            startFlag.store(true);
            // Sleep for 100 seconds
            this_thread::sleep_for(test_length);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid].join();
            }

            quit.store(false);
            startFlag.store(false);
            delete stack;
        }

        for (int i = 0; i < total_elements; i++) 
        {
            // delete udarray[i];
            delete elements[i];
        }

        // Accounting
        vector<long long> agg(total_runs);
        for (int irun = 0; irun < total_runs; irun++) {
            agg[irun] = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
        }

        // Compute the median, max and min. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[total_runs-1];
        auto minops = agg[0];
        auto medianops = agg[total_runs/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));

        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "   delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        // return medianops;
    }

    template<typename L>
    long long benchmarkLinkedList(const int update_ratio, const seconds test_length, const int total_runs, const int total_elements) {
        long long ops[total_elements][total_runs];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        L* list = nullptr;

        int* elements[total_elements];
        for (int i = 0; i < total_elements; i++) 
        {
            elements[i] = new int(i);
        }

        // Creating threads using lambda functions
        auto rw_lambda = [this,&update_ratio,&quit,&startFlag,&list,&total_elements, &elements](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t seed = tid;
            
            srand (time(NULL));

            while (!startFlag.load()) { } // spin
            while (!quit.load()) 
            {
                seed = rand()*total_elements + 1;
                unsigned int ix = (unsigned int)(seed%total_elements);
                int ratio = seed%10000;  // Ratios are in per-10k units
                if (ratio < update_ratio) 
                {
                    if (list->remove(elements[ix], tid)) 
                    {
                        list->insert(elements[ix], tid);
                        numOps+=1;
                    }
                } else {
                    list->contains(elements[ix], tid);
                }
                numOps+=1;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < total_runs; irun++) {
            list = new L(numThreads);
            // Add all the items to the list
            for (int i = 0; i < total_elements; i++) 
            {
                list->insert(elements[i], 0);
            }

            if (irun == 0) 
            {
                cout<<"----- Benchmarking Linked List ------\n";
            }

            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid] = thread(rw_lambda, &ops[tid][irun], tid);
            }

            startFlag.store(true);
            // Sleep for 10 seconds
            this_thread::sleep_for(test_length);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid].join();
            }

            quit.store(false);
            startFlag.store(false);
            delete list;
        }

        for (int i = 0; i < total_elements; i++) 
        {
            delete elements[i];
        }

        // Accounting
        vector<long long> agg(total_runs);
        for (int irun = 0; irun < total_runs; irun++) {
            agg[irun] = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
        }

        // Compute the median, max and min. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[total_runs-1];
        auto minops = agg[0];
        auto medianops = agg[total_runs/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));

        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "   delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        return medianops;
    }

public:

    static void allThroughputTests() 
    {

        vector<int> total_threads = {4, 8, 16};
        vector<int> ratio = {5000}; // per-10k ratio: 100%, 10%, 1%, 0%
        int total_runs = 5;
        const seconds test_length = 10s;
        int total_elements = 10000;

   
        for(int thread_index=0; thread_index < total_threads.size(); thread_index++)
        {
            for(int ratio_index=0; ratio_index < ratio.size(); ratio_index++)
            {
                Benchmarks bench(total_threads[thread_index]);
                std::cout << "\n-----  Benchmarks   numElements=" << total_elements << "   ratio=" << ratio[ratio_index]/100 << "%   numThreads=" << total_threads[thread_index] << "   numRuns=" << total_runs << "   length=" << test_length.count() << "s -----\n";
                // bench.benchmarkLinkedList<LinkedListURCU<int>>(ratio[ratio_index], test_length, total_runs, total_elements);
                bench.benchmarkQueues<QueueURCU<int>>(ratio[ratio_index], test_length, total_runs, total_elements);
                // bench.benchmarkStacks<StackURCU<int>>(ratio[ratio_index], test_length, total_runs, total_elements);
            }
        }
    }
};

#endif