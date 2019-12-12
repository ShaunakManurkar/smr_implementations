#ifndef INTERVAL_BASED_RECLAMATION_HPP
#define INTERVAL_BASED_RECLAMATION_HPP

#include <iostream>
#include <atomic>
#include <vector>

using namespace std;

#define MAX_NUMBER_OF_THREADS_IBR 40
#define RETIRED_LIST_SIZE 10

template<typename T>
class IntervalBasedReclamation{

private:
    std::atomic<uint64_t> globalEpoch{0};
    int numThreads;
    int epochFreq;
    int emptyFreq;
    std::atomic<uint64_t> reservations[MAX_NUMBER_OF_THREADS_IBR];
    uint64_t allocStat[MAX_NUMBER_OF_THREADS_IBR];
    uint64_t retireStat[MAX_NUMBER_OF_THREADS_IBR];
    std::vector<T*> retiredList[MAX_NUMBER_OF_THREADS_IBR];
    uint64_t retiredNodesCount[MAX_NUMBER_OF_THREADS_IBR];

public:
    IntervalBasedReclamation(int threadCount, int epf, int emf)
    {
        numThreads = threadCount;
        epochFreq = epf;
        emptyFreq = emf;
        for(int i = 0; i < numThreads; i++)
        {
            reservations[i].store(UINT64_MAX, std::memory_order_release);
            retiredList[i].reserve(RETIRED_LIST_SIZE);
            allocStat[i] = 0;
            retireStat[i] = 0;
            retiredNodesCount[i] = 0;
        }
    }

    uint64_t getGlobalEpoch()
    {
        return globalEpoch.load(std::memory_order_release);
    }

    void start_op(int threadID){
		uint64_t e = globalEpoch.load(std::memory_order_acquire);
		reservations[threadID].store(e,std::memory_order_seq_cst);
	}
	void end_op(int threadID){
		reservations[threadID].store(UINT64_MAX,std::memory_order_seq_cst);	
	}    

    T* allocNode(int threadID, T* obj)
    {
		allocStat[threadID] += 1;
		if(allocStat[threadID]% epochFreq == 0)
        {
			globalEpoch.fetch_add(1,std::memory_order_acq_rel);
		}
		obj->birth_epoch = getGlobalEpoch();
		return obj;
	}

    void retireNode(T* obj, int threadID)
    {
		if(obj==NULL)
        {
            return;
        }
		uint64_t re = globalEpoch.load(std::memory_order_acquire);
        obj->retire_epoch = re;
		retiredList[threadID].push_back(obj);
        retiredNodesCount[threadID] += 1;
        //cout << "Incremented ctr to " << retiredNodesCount[threadID] << endl;	
		if(retireStat[threadID]%emptyFreq == 0)
        {
			emptyRetireList(threadID);
		}
		retireStat[threadID] += 1;
	}

    bool conflict(uint64_t* reservEpoch, uint64_t birth_epoch, uint64_t retire_epoch){
		for (int i = 0; i < numThreads; i++){
			if (reservEpoch[i] >= birth_epoch && reservEpoch[i] <= retire_epoch){
				return true;
			}
		}
		return false;
	}
	
	void emptyRetireList(int threadID)
    {
		uint64_t reservEpoch[numThreads];
		for (int i = 0; i < numThreads; i++)
        {
			reservEpoch[i] = reservations[i].load(std::memory_order_acquire);
		}
		for (int i = 0;i < retiredList[threadID].size();i++)
        {
            auto temp = retiredList[threadID][i];
			if(!conflict(reservEpoch, temp->birth_epoch, temp->retire_epoch))
            {
				retiredList[threadID].erase(retiredList[threadID].begin() + i);
				delete temp;
				retireStat[threadID] -= 1;
                retiredNodesCount[threadID] -= 1;
                //cout << "Decremented ctr to " << retiredNodesCount[threadID] << endl;
			}
		}
	}

    uint64_t getRetiredNodeCount(int threadID)
    {
        return retiredNodesCount[threadID];
    }

};
#endif