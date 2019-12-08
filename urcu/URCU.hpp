#ifndef _URCU_H_
#define _URCU_H_

#include <atomic>

class URCU {
    static const uint64_t NOT_READING = 0xFFFFFFFFFFFFFFFE;
    static const uint64_t UNASSIGNED =  0xFFFFFFFFFFFFFFFD;

    const int max_threads;
    std::atomic<uint64_t> updaterVersion { 0 };
    std::atomic<uint64_t>* readersVersion;

public:
    URCU(const int max_threads = 128) : max_threads{max_threads} 
    {
        readersVersion = new std::atomic<uint64_t>[max_threads];
        for (int i=0; i < max_threads; i++) 
        {
            readersVersion[i].store(UNASSIGNED, std::memory_order_relaxed);
        }
    }

    ~URCU() {
        delete[] readersVersion;
    }

    // int registerThread() 
    // {
    //     for (int i=0; i < max_threads; i++) {
    //         if (readersVersion[i].load() != UNASSIGNED) continue;
    //         uint64_t curr = UNASSIGNED;
    //         if (readersVersion[i].compare_exchange_strong(curr, NOT_READING)) {
    //              return i;
    //         }
    //     }
    //     std::cout << "Error: Max threads reached\n";
    // }

    // void unregisterThread(int thread_id)
    // {
    //     if (readersVersion[thread_id].load() == UNASSIGNED) {
    //         std::cout << "Error: Thread Id is invalid\n";
    //         return;
    //     }
    //     readersVersion[thread_id].store(UNASSIGNED);
    // }

    void readLock(const int thread_id) noexcept 
    {
        const uint64_t rv = updaterVersion.load();
        readersVersion[thread_id].store(rv);
        const uint64_t nrv = updaterVersion.load();
        if (rv != nrv) 
        {
            readersVersion[thread_id].store(nrv, std::memory_order_relaxed);
        }
    }


    void readUnlock(const int thread_id) noexcept 
    {
        readersVersion[thread_id].store(NOT_READING, std::memory_order_release);
    }

    void synchronizeRCU() noexcept 
    {
        const uint64_t waitForVersion = updaterVersion.load();
        auto tmp = waitForVersion;
        updaterVersion.compare_exchange_strong(tmp, waitForVersion+1);
        for (int i=0; i < max_threads; i++) 
        {
            while (readersVersion[i].load() <= waitForVersion) { } // spin
        }
    }
};

#endif
