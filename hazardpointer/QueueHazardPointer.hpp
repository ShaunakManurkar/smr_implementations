#ifndef QUEUE_HAZARD_POINTER_HPP
#define QUEUE_HAZARD_POINTER_HPP

#include <atomic>
#include <iostream>
#include "hazardpointer.hpp"

template<typename T>
class Queue_HazardPointer {

private:
    struct Node {
        T* item;
        std::atomic<Node*> next;

        Node(T* item)
        {
            item = item;
            next.store(nullptr);
        }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    const int numOfThreads;
    hazardPointers<Node> hpQueue {numOfThreads};

public:
    Queue_HazardPointer(int numOfThreads) : numOfThreads{numOfThreads} {
        Node* sentinel = new Node(nullptr);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~Queue_HazardPointer() {
        while (dequeue(0) != nullptr);
        delete head.load();
    }

    bool enqueue(T* item, int threadID) 
    {
        if (item == nullptr)
        {
            return false;
        }
        Node* node = new Node(item);
        while (true) 
        {
            Node* temp = hpQueue.storeHazardPtr(0, tail, threadID);
            if (temp == tail.load()) 
            {
                Node* next = temp->next.load();
                if (next == nullptr) 
                {
                    if (temp->casNext(nullptr, node)) 
                    {
                        tail.compare_exchange_strong(temp, node);
                        hpQueue.clear(threadID);
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


    T* dequeue(const int threadID) 
    {
        Node* node = hpQueue.protect(0, head, threadID);
        while (node != tail.load()) 
        {
            Node* next = hpQueue.protect(1, node->next, threadID);
            if (head.compare_exchange_strong(node, next)) 
            {
                T* item = next->item;
                hpQueue.clear(threadID);
                hpQueue.retireNode(node, threadID);
                return item;
            }
            node = hpQueue.protect(0, head, threadID);
        }
        hpQueue.clear(threadID);
        return nullptr;
    }
};

#endif
