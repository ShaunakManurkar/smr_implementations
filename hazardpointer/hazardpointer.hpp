
#ifndef HAZARD_POINTER_HPP
#define HAZARD_POINTER_HPP

#include <iostream>
#include <atomic>
#include <vector>

#define MAX_NUMBER_OF_HAZARD_POINTERS_PER_THREAD 5
#define MAX_NUMBER_OF_THREADS 40
#define RETIRED_NODES_THRESHOLD 0
#define TOTAL_NUMBER_OF_HAZARD_POINTERS MAX_NUMBER_OF_THREADS*MAX_NUMBER_OF_HAZARD_POINTERS_PER_THREAD

template<typename T>
class hazardPointers {

private:
    int threadCount;
    std::atomic<T*>* hazardPointerList[MAX_NUMBER_OF_THREADS];
    std::vector<T*>  retiredPointerList[MAX_NUMBER_OF_THREADS];

public:
    hazardPointers(int numThreads){
        threadCount = numThreads;
        for (int i = 0; i < MAX_NUMBER_OF_THREADS; i++) {
            hazardPointerList[i] = new std::atomic<T*>[MAX_NUMBER_OF_HAZARD_POINTERS_PER_THREAD];
            //retiredPointerList[i].reserve(TOTAL_NUMBER_OF_HAZARD_POINTERS);
            for (int j = 0; j < MAX_NUMBER_OF_HAZARD_POINTERS_PER_THREAD; j++) {
                hazardPointerList[i][j].store(nullptr, std::memory_order_relaxed);
            }
        }
    }

    ~hazardPointers() {
        for (int i = 0; i < MAX_NUMBER_OF_THREADS; i++) {
            delete[] hazardPointerList[i];
            for (int j = 0; j < retiredPointerList[i].size(); j++) {
                delete retiredPointerList[i][j];
            }
        }
    }

    void clear(const int threadID) {
        for (int i = 0; i < MAX_NUMBER_OF_HAZARD_POINTERS_PER_THREAD; i++) {
            hazardPointerList[threadID][i].store(nullptr, std::memory_order_release);
        }
    }

	
    T* storeHazardPtr(int hazardPointerIndex, T* ptr, const int threadID) {
        hazardPointerList[threadID][hazardPointerIndex].store(ptr/*, std::memory_order_release*/);
        return ptr;
    }

    T* protect(int index, const std::atomic<T*>& atom, const int tid) {
        T* n = nullptr;
        T* ret;
		while ((ret = atom.load()) != n) {
			hazardPointerList[tid][index].store(ret);
			n = ret;
		}
		return ret;
    }

    void retireNode(T* ptr, const int threadID) {
        bool deleteNode = true;
        retiredPointerList[threadID].push_back(ptr);
        if (retiredPointerList[threadID].size() < RETIRED_NODES_THRESHOLD) 
        {  
            return;
        }
        for (int i = 0; i < retiredPointerList[threadID].size();i++) 
        {
            T* stptr = retiredPointerList[threadID][i];
            for (int k = 0; k < threadCount; k++) 
            {
                for (int j = 0; j < MAX_NUMBER_OF_HAZARD_POINTERS_PER_THREAD; j++) 
                {
                    if (hazardPointerList[k][j].load() == stptr) 
                    {
                        deleteNode = false;
                        break;
                    }
                }
            }
            if (deleteNode) 
            {
                retiredPointerList[threadID].erase(retiredPointerList[threadID].begin() + i);
                delete stptr;
                continue;
            }
        }
    }
};

#endif