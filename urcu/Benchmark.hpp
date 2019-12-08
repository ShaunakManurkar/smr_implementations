#ifndef _BENCHMARK_H_
#define _BENCHMARK_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>

#include "LinkedListURCU.hpp"
#include "QueueURCU.hpp"
#include "StackURCU.hpp"

using namespace std;
using namespace chrono;

class Benchmarks {

private:
    struct UserData  {
        long long seq;
        int tid;
        UserData(long long lseq, int ltid) {
            this->seq = lseq;
            this->tid = ltid;
        }
        UserData() {
            this->seq = -2;
            this->tid = -2;
        }
        UserData(const UserData &other) : seq(other.seq), tid(other.tid) { }

        bool operator < (const UserData& other) const {
            return seq < other.seq;
        }
        bool operator == (const UserData& other) const {
            return seq == other.seq && tid == other.tid;
        }
    };

    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result() { }

        Result(const Result &other) {
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator < (const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
    };


    static const long long NSEC_IN_SEC = 1000000000LL;

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
        UserData* udarray[total_elements];
        int elements[total_elements];
        for (int i = 0; i < total_elements; i++) 
        {
            udarray[i] = new UserData(i, 0);
            elements[i] = i;
        }

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&update_ratio,&quit,&startFlag,&queue,&udarray,&total_elements](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t seed = tid+1234567890123456781ULL;
            while (!startFlag.load()) { } // spin
            while (!quit.load()) {
                seed = randomLong(seed);
                auto ix = (unsigned int)(seed%total_elements);
                seed = randomLong(seed);
                auto ratio = seed%10000;  // Ratios are in per-10k units
                if (ratio < update_ratio) {
                    // Writer threads
                    queue->enqueue(udarray[ix], tid);
                } else {
                    queue->dequeue(tid);
                    // cout<<"dequeue\n";
                    // Reader threads
                    // list->contains(udarray[ix], tid);
                    seed = randomLong(seed);
                    ix = (unsigned int)(seed%total_elements);
                    // list->contains(udarray[ix], tid);
                }
                numOps+=2;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < total_runs; irun++) {
            // list = new L(numThreads);
            queue = new Q(numThreads);
            // Add all the items to the list
            for (int i = 0; i < total_elements; i++) 
            {
                // list->insert(udarray[i], 0);
                queue->enqueue(udarray[i], 0);
                // list->push(udarray[i], 0);
            }

            if (irun == 0) 
            {
                // cout << "##### " << list->className() << " #####  \n";
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

        for (int i = 0; i < total_elements; i++) delete udarray[i];

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

    template<typename S>
    long long benchmarkStacks(int update_ratio, seconds test_length, int total_runs, int total_elements) 
    {
        long long ops[numThreads][total_runs];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        S* stack = nullptr;

        // Create all the objects in the list
        UserData* udarray[total_elements];
        int elements[total_elements];
        for (int i = 0; i < total_elements; i++) 
        {
            udarray[i] = new UserData(i, 0);
            elements[i] = i;
        }

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&update_ratio,&quit,&startFlag,&stack,&udarray,&total_elements](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t seed = tid+1234567890123456781ULL;
            while (!startFlag.load()) { } // spin
            while (!quit.load()) {
                seed = randomLong(seed);
                auto ix = (unsigned int)(seed%total_elements);
                seed = randomLong(seed);
                auto ratio = seed%10000;  // Ratios are in per-10k units
                if (ratio < update_ratio) {
                    // Writer threads
                    // if (list->remove(udarray[ix], tid)) list->insert(udarray[ix], tid);
                    stack->push(udarray[ix], tid);
                    // list->enqueue(udarray[ix], tid);
                    // cout<<"enqueue\n";
                } else {
                    stack->pop(tid);
                    // list->dequeue(tid);
                    // cout<<"dequeue\n";
                    // Reader threads
                    // list->contains(udarray[ix], tid);
                    seed = randomLong(seed);
                    ix = (unsigned int)(seed%total_elements);
                    // list->contains(udarray[ix], tid);
                }
                numOps+=2;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < total_runs; irun++) {
            stack = new S(numThreads);
            // Add all the items to the list
            for (int i = 0; i < total_elements; i++) 
            {
                // list->insert(udarray[i], 0);
                // list->enqueue(udarray[i], 0);
                stack->push(udarray[i], 0);
            }

            if (irun == 0) 
            {
                // cout << "##### " << list->className() << " #####  \n";
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

        for (int i = 0; i < total_elements; i++) delete udarray[i];

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
    long long benchmarkLinkedList(const int updateRatio, const seconds testLengthSeconds, const int numRuns, const int numElements) {
        long long ops[numThreads][numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        L* list = nullptr;

        // Create all the objects in the list
        UserData* udarray[numElements];
        int elements[numElements];
        for (int i = 0; i < numElements; i++) 
        {
            udarray[i] = new UserData(i, 0);
            elements[i] = i;
        }

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&updateRatio,&quit,&startFlag,&list,&udarray,&numElements](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t seed = tid+1234567890123456781ULL;
            while (!startFlag.load()) { } // spin
            while (!quit.load()) {
                seed = randomLong(seed);
                auto ix = (unsigned int)(seed%numElements);
                seed = randomLong(seed);
                auto ratio = seed%10000;  // Ratios are in per-10k units
                if (ratio < updateRatio) {
                    // Writer threads
                    if (list->remove(udarray[ix], tid)) list->insert(udarray[ix], tid);
                    // list->push(udarray[ix], tid);
                    // list->enqueue(udarray[ix], tid);
                    // cout<<"enqueue\n";
                } else {
                    // list->pop(tid);
                    // list->dequeue(tid);
                    // cout<<"dequeue\n";
                    // Reader threads
                    list->contains(udarray[ix], tid);
                    seed = randomLong(seed);
                    ix = (unsigned int)(seed%numElements);
                    list->contains(udarray[ix], tid);
                }
                numOps+=2;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            list = new L(numThreads);
            // Add all the items to the list
            for (int i = 0; i < numElements; i++) 
            {
                list->insert(udarray[i], 0);
                // list->enqueue(udarray[i], 0);
                // list->push(udarray[i], 0);
            }

            if (irun == 0) 
            {
                // cout << "##### " << list->className() << " #####  \n";
                cout<<"----- Benchmarking Linked List ------\n";
            }
            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid] = thread(rw_lambda, &ops[tid][irun], tid);
            }

            startFlag.store(true);
            // Sleep for 100 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) 
            {
                rwThreads[tid].join();
            }

            quit.store(false);
            startFlag.store(false);
            delete list;
        }

        for (int i = 0; i < numElements; i++) delete udarray[i];

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            agg[irun] = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
        }

        // Compute the median, max and min. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));

        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "   delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        return medianops;
    }

    uint64_t randomLong(uint64_t x) {
        x ^= x >> 12; // a
        x ^= x << 25; // b
        x ^= x >> 27; // c
        return x * 2685821657736338717LL;
    }



public:

    static void allThroughputTests() {
        // vector<int> threadList = { 1, 2, 4, 8 /*, 16, 20, 24, 28, 32, 34, 36, 48, 64*/ }; // Number of threads for Opteron
        // vector<int> threadList = {1};         // Number of threads for the laptop
        // vector<int> ratioList = { 1000 /*10000, 1000, 100, 0*/ }; // per-10k ratio: 100%, 10%, 1%, 0%
        // const int numRuns = 5;                           // 5 runs for the paper
        // const seconds testLength = 10s;                  // 20s for the paper
        // vector<int> elemsList = { /*100, 1000,*/ 10000 };    // Number of keys in the set: 100, 1k, 10k

        vector<int> total_threads = {4, 8, 16};
        vector<int> ratio = {0, 1000, 5000, 10000}; // per-10k ratio: 100%, 10%, 1%, 0%
        int total_runs = 5;
        const seconds test_length = 10s;
        int total_elements = 10000;

        // Save results
        // [class][ratio][threads]
        // const int LNO = 0;
        // const int LHP = 1;
        // const int LHE = 2;
        // const int LUR = 3;
        // const int LUD = 4;
        // const int LLB = 5;
        // const int LHR = 6;
        // const int LWF = 7;
        // long long ops[7][ratioList.size()][threadList.size()];

   
        for(int thread_index=0; thread_index < total_threads.size(); thread_index++)
        {
            for(int ratio_index=0; ratio_index < ratio.size(); ratio_index++)
            {
                Benchmarks bench(total_threads[thread_index]);
                std::cout << "\n-----  Benchmarks   numElements=" << total_elements << "   ratio=" << ratio[ratio_index]/100 << "%   numThreads=" << total_threads[thread_index] << "   numRuns=" << total_runs << "   length=" << test_length.count() << "s -----\n";
                // ops[LUR][iratio][ithread] = 
                // bench.benchmarkLinkedList<LinkedListURCU<UserData>>(ratio[ratio_index], test_length, total_runs, total_elements);
                bench.benchmarkQueues<QueueURCU<UserData>>(ratio[ratio_index], test_length, total_runs, total_elements);
                // bench.benchmarkStacks<StackURCU<UserData>>(ratio[ratio_index], test_length, total_runs, total_elements);
            }
        }
    }
};

#endif
