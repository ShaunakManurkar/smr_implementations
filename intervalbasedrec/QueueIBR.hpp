#ifndef QUEUE_IBR_HPP
#define QUEUE_IBR_HPP

#include <atomic>
#include <iostream>
#include "IntervalBasedReclamation.hpp"

using namespace std;

#define QUEUE_EPOCH_FREQUENCY 150
#define QUEUE_RECLAIM_FREQUENCY 30

template<typename T>
class Queue_IBR
{

private:
    struct Node{
        T* item;
        uint64_t birth_epoch;
        uint64_t retire_epoch;
        std:: atomic<Node*> next;

        Node(T* item) 
        {
            item = item;
            next.store(nullptr);
            birth_epoch = 0;
            retire_epoch = 0;
        }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    const int numOfThreads;
    IntervalBasedReclamation<Node> ibrQueue{numOfThreads,QUEUE_EPOCH_FREQUENCY,QUEUE_RECLAIM_FREQUENCY};

public:

    Queue_IBR(int numOfThreads) : numOfThreads{numOfThreads} {
        Node* sentinel = new Node(nullptr);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~Queue_IBR(){
        while(dequeue(0) != nullptr);
        delete head.load();
    }

    bool enqueue(T* item, int threadID)
    {
        if(item == nullptr)
        {
            return false;
        }
        Node* node = new Node(item);
        node = ibrQueue.allocNode(threadID,node);
        ibrQueue.start_op(threadID);
        while(true)
        {
            Node* temp = tail.load();
            if (temp == tail.load()) 
            {
                Node* next  = temp->next.load();
                if (next == nullptr) 
                {
                    if (temp->casNext(nullptr, node)) 
                    {
                        tail.compare_exchange_strong(temp, node);
                        ibrQueue.end_op(threadID);
                        return true;
                    }
                } 
                else 
                {
                    tail.compare_exchange_strong(temp, next);
                }
            }
        }
    }

    T* dequeue(int threadID)
    {
        ibrQueue.start_op(threadID);
        Node* node = head.load();
        while (node != tail.load()) 
        {
            Node* next = node->next.load();
            if (head.compare_exchange_strong(node, next)) 
            {
                T* item = next->item;
                ibrQueue.retireNode(node, threadID);
                ibrQueue.end_op(threadID);
                return item;
            }
            node = head.load();
        }
        ibrQueue.end_op(threadID);
        return nullptr;
    }

    uint64_t getRetiredCountQueue(int threadID)
    {
        return ibrQueue.getRetiredNodeCount(threadID);
    }
};

#endif