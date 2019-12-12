#ifndef HAZARD_ERAS_HPP
#define HAZARD_ERAS_HPP

#include <atomic>
#include <vector>
#include <iostream>

#define MAX_NUMBER_OF_HAZARD_ERAS_PER_THREAD 5
#define HE_MAX_NUMBER_OF_THREADS 40
#define HE_TOTAL_NUMBER_HAZARD_ERAS  MAX_NUMBER_OF_HAZARD_ERAS_PER_THREAD*HE_MAX_NUMBER_OF_THREADS

template<typename T>
class hazardEras
{
private:
    int threadCount;
    uint64_t emptyEra = 0;
    std::atomic<uint64_t> globalEraClock{1};
    std::atomic<uint64_t>* hazardErasList[HE_MAX_NUMBER_OF_THREADS];
    std::vector<T*> retiredPtrList[HE_MAX_NUMBER_OF_THREADS];
    uint64_t retiredNodesCount[HE_MAX_NUMBER_OF_THREADS];

public:
    hazardEras(int numThreads){
        threadCount = numThreads;
        for (int i = 0; i < HE_MAX_NUMBER_OF_THREADS; i++) {
            hazardErasList[i] = new std::atomic<uint64_t>[MAX_NUMBER_OF_HAZARD_ERAS_PER_THREAD];
            retiredNodesCount[i] = 0;
            retiredPtrList[i].reserve(HE_TOTAL_NUMBER_HAZARD_ERAS);
            for (int j = 0; j < MAX_NUMBER_OF_HAZARD_ERAS_PER_THREAD; j++) {
                hazardErasList[i][j].store(emptyEra, std::memory_order_relaxed);
            }
        }
    }

    ~hazardEras() {
        for (int i = 0; i < HE_MAX_NUMBER_OF_THREADS; i++) {
            delete[] hazardErasList[i];
            for (int j = 0; j < retiredPtrList[i].size(); j++) {
                delete retiredPtrList[i][j];
            }
        }
    }

    uint64_t getEra()
    {
        return globalEraClock.load();
    }

    void clear(const int threadID) 
    {
        for (int i = 0; i < MAX_NUMBER_OF_HAZARD_ERAS_PER_THREAD; i++) 
        {
            hazardErasList[threadID][i].store(emptyEra, std::memory_order_release);
        }
    }

    T* get_protected(int eraIndex, const std::atomic<T*>& item, const int threadID) {
        auto prevEra = hazardErasList[threadID][eraIndex].load(std::memory_order_relaxed);
		while (true) {
		    T* temp = item.load();
		    auto currEra = globalEraClock.load(std::memory_order_acquire);
		    if (currEra == prevEra)
            {
                return temp;
            }
            hazardErasList[threadID][eraIndex].store(currEra);
            prevEra = currEra;
		}
    }

    void protectEraRelease(int eraIndex, int other, const int threadID) 
    {
        auto era = hazardErasList[threadID][other].load(std::memory_order_relaxed);
        if (hazardErasList[threadID][eraIndex].load(std::memory_order_relaxed) == era) 
        {
            return;
        }
        hazardErasList[threadID][eraIndex].store(era, std::memory_order_release);
    }

    bool canRemoveNode(T* item, const int threadID) {
        for (int i = 0; i < HE_MAX_NUMBER_OF_THREADS; i++) 
        {
            for (int j = 0; j < MAX_NUMBER_OF_HAZARD_ERAS_PER_THREAD; j++) 
            {
                uint64_t era = hazardErasList[threadID][j].load(std::memory_order_acquire);
                if (era == emptyEra || era < item->newEra || era > item->delEra) 
                {
                    continue;
                }
                return false;
            }
        }
        return true;
    }

    void retireNode(T* item, int threadID) {
        auto currEra = globalEraClock.load();
        item->delEra = currEra;
        retiredPtrList[threadID].push_back(item);
        retiredNodesCount[threadID] += 1;
        if (globalEraClock == currEra) 
        {
            globalEraClock.fetch_add(1);
        }
        for (int i = 0; i < retiredPtrList[threadID].size();i++) 
        {
            auto stptr = retiredPtrList[threadID][i];
            if (canRemoveNode(stptr, threadID)) 
            {
                retiredPtrList[threadID].erase(retiredPtrList[threadID].begin() + i);
                delete stptr;
                retiredNodesCount[threadID] -= 1;
                continue;
            }
        }
    }

    uint64_t getRetiredNodeCount(int threadID){
        return retiredNodesCount[threadID];
    }
};
#endif