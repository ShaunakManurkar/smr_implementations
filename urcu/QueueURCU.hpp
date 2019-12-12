#ifndef _QUEUE_URCU_H_
#define _QUEUE_URCU_H_

#include <atomic>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include "URCU.hpp"

#define MAX_THREAD_COUNT 40

template<typename T>
class QueueURCU
{

private:
    struct Node{
        T* item;
        std:: atomic<Node*> next;

        Node(T* userItem) : item{userItem}, next{nullptr} { }
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    int max_threads;
    URCU urcu {max_threads};
    long retired_nodes_count[MAX_THREAD_COUNT];

public:

    QueueURCU(int max_threads) : max_threads{max_threads} {
        Node* sentinel_node = new Node(nullptr);
        head.store(sentinel_node, std::memory_order_relaxed);
        tail.store(sentinel_node, std::memory_order_relaxed);

        for(int i=0; i<max_threads; i++)
        {
            retired_nodes_count[i] = 0;
        }
    }

    ~QueueURCU() 
    {
        while (dequeue(0) != nullptr);
        delete head.load();            
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
        retired_nodes_count[thread_id] += 1;
        urcu.readUnlock(thread_id);
        deleteRetiredNodes(retired, thread_id);
        return ret_data;
    }

    long getRetiredNodesCount(int thread_id)
    {
        return retired_nodes_count[thread_id];
    }
    
    void deleteRetiredNodes(std::vector<Node*>& retired_nodes, int thread_id)
    {
        if(retired_nodes.size() > 0)
        {
            urcu.synchronizeRCU();
            for(auto del_node : retired_nodes)
            {
                retired_nodes_count[thread_id] -= 1;
                delete del_node;
            }
        }
    }
};

#endif