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
            this->item = item;
            this->next.store(nullptr);
            this->birth_epoch = 0;
            this->retire_epoch = 0;
        }
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    int numofThreads;
    IntervalBasedReclamation<Node> ibrQueue{numofThreads,QUEUE_EPOCH_FREQUENCY,QUEUE_RECLAIM_FREQUENCY};

public:

    Queue_IBR(int numThreads)
    {
        numofThreads = numThreads;
        Node* sentinel = new Node(nullptr);
        head.store(sentinel);
        tail.store(sentinel);
    }

    bool enqueue(T* item, int threadID)
    {
        Node *next, *temp;
        Node* ptrnull = nullptr;
        if(item == nullptr)
        {
            return false;
        }
        Node* node = new Node(item);
        node = ibrQueue.allocNode(threadID,node);
        ibrQueue.start_op(threadID);
        while(true)
        {
            temp = tail.load();
            if(temp != tail.load())
            {
                continue;
            }
            next = temp->next.load();
            if(tail.load() != temp)
            {
                continue;
            }
            if(next != nullptr)
            {
                tail.compare_exchange_strong(temp, next);
                continue;
            }
            if(temp->next.compare_exchange_strong(ptrnull,node))
            {
                break;
            }
        }
        tail.compare_exchange_strong(temp,node);
        ibrQueue.end_op(threadID);
        cout << "Inserted item" << endl;
        return true;
    }

    T* dequeue(int threadID)
    {
        Node *temp1, *temp2, *next;
        T* ret_data;
        ibrQueue.start_op(threadID);
        while(true)
        {
            temp1 = head.load();
            if(temp1 != head.load())
            {
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
                ibrQueue.end_op(threadID);
                return nullptr;
            }
            if(temp1 == temp2)
            {
                tail.compare_exchange_strong(temp2,next);
                continue;
            }
            if(head.compare_exchange_strong(temp1,next))
            {
                ret_data = next->item;
                break;
            }
        }
        ibrQueue.retireNode(temp1,threadID);
        ibrQueue.end_op(threadID);
        cout << "Removed item" << endl;
        return ret_data;
    }
};

#endif