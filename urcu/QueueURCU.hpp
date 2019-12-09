#ifndef _QUEUE_URCU_H_
#define _QUEUE_URCU_H_

#include <atomic>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include "URCU.hpp"
// #include "URCUGraceVersion.hpp"
// using namespace std;
// using namespace chrono;

template<typename T>
class QueueURCU
{

private:
    struct Node{
        T* item;
        std:: atomic<Node*> next;

        Node(T* userItem) : item{userItem}, next{nullptr} { }
        // Node(T* item) 
        // {
        //     this->item = item;
        //     this->next.store(nullptr);
        // }
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    int max_threads;
    URCU urcu {max_threads};

public:

    // MichaelScottQueue(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
    //     Node* sentinelNode = new Node(nullptr);
    //     head.store(sentinelNode, std::memory_order_relaxed);
    //     tail.store(sentinelNode, std::memory_order_relaxed);
    // }

    QueueURCU(int max_threads=128) : max_threads{max_threads} {
        Node* sentinel_node = new Node(nullptr);
        head.store(sentinel_node, std::memory_order_relaxed);
        tail.store(sentinel_node, std::memory_order_relaxed);
    }

    ~QueueURCU() 
    {
        while (dequeue(0) != nullptr); // Drain the queue
        delete head.load();            // Delete the last node
    }

    bool enqueue(T* item, int thread_id)
    {
        // std::cout<<"Inside enqueue\n";
        Node *next, *temp;
        Node* ptrnull = nullptr;
        if(item == nullptr)
        {
            return false;
        }
        Node* node = new Node(item);
        while(true)
        {
            // std::cout<<"Inside while enqueue\n";
            temp = tail.load();
            // hpQueue.storeHazardPtr(0, temp, threadID);
            urcu.readLock(thread_id);

            if(temp != tail.load())
            {
                // hpQueue.clear(threadID);
                urcu.readUnlock(thread_id);
                continue;
            }
            next = temp->next.load();
            if(tail.load() != temp)
            {
                // hpQueue.clear(threadID);
                urcu.readUnlock(thread_id);
                continue;
            }
            if(next != nullptr)
            {
                tail.compare_exchange_strong(temp, next);
                // hpQueue.clear(threadID);
                urcu.readUnlock(thread_id);
                continue;
            }
            if(temp->next.compare_exchange_strong(ptrnull,node))
            {
                // std::cout<<"-----------------------------Item enqueued\n\n";
                break;
            }
        }
        tail.compare_exchange_strong(temp,node);
        // hpQueue.clear(threadID);
        urcu.readUnlock(thread_id);
        urcu.synchronizeRCU();
        return true;
    }

    T* dequeue(int thread_id)
    {
        // std::cout<<"Inside dequeue\n";
        Node *temp1, *temp2, *next;
        T* ret_data;
        std::vector<Node*> retired;
        
        while(true)
        {
            // std::cout<<"Inside while dequeue\n";
            temp1 = head.load();
            // hpQueue.storeHazardPtr(0, temp1, threadID);
            urcu.readLock(thread_id);
            if(temp1 != head.load())
            {
                // hpQueue.clear(threadID);
                urcu.readUnlock(thread_id);
                continue;
            }
            temp2 = tail.load();
            next = temp1->next.load();
            if(head.load() != temp1)
            {
                continue;
            }
            if(next == nullptr)
            {
                return nullptr;
            }
            if(temp1 == temp2)
            {
                tail.compare_exchange_strong(temp2,next);
                continue;
            }
            if(head.compare_exchange_strong(temp1,next))
            {
                // std::cout<<"----------------Item dequeued\n\n";
                ret_data = next->item;
                break;
            }
        }
        // hpQueue.retireNode(temp1,threadID);
        retired.push_back((Node*)temp1);
        urcu.readUnlock(thread_id);
        deleteRetiredNodes(retired);
        return ret_data;
    }

    void deleteRetiredNodes(std::vector<Node*>& retired_nodes)
    {
        if(retired_nodes.size() > 0)
        {
            urcu.synchronizeRCU();
            for(auto del_node : retired_nodes)
            {
                delete del_node;
            }
        }
    }
};

#endif