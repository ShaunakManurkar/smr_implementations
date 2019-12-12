#ifndef QUEUE_HAZARD_ERAS_HPP
#define QUEUE_HAZARD_ERAS_HPP

#include <atomic>
#include <iostream>
#include <vector>
#include "hazarderas.hpp"

using namespace std;

template<typename T>
class Queue_HazardEras {

private:
    struct Node {
        T* item;
        uint64_t newEra;
        uint64_t delEra;
        std:: atomic<Node*> next;

        Node(T* item, uint64_t birthEra) 
        {
            item = item;
            next.store(nullptr);
            newEra = birthEra;
            delEra = 0;
        }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    const int numOfThreads;
    hazardEras<Node> heQueue{numOfThreads};

public:
    Queue_HazardEras(int numOfThreads) : numOfThreads{numOfThreads} {
        Node* sentinel = new Node(nullptr,1);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~Queue_HazardEras(){
        while(dequeue(0) != nullptr);
        delete head.load();
    }

    bool enqueue(T* item, int threadID) 
    {
        if (item == nullptr)
        {
            return false;
        }
        Node* node = new Node(item, heQueue.getEra());
        while (true) 
        {
            Node* temp = heQueue.get_protected(0, tail, threadID);
            if (temp == tail.load()) 
            {
                Node* next  = temp->next.load();
                if (next == nullptr) 
                {
                    if (temp->casNext(nullptr, node)) 
                    {
                        tail.compare_exchange_strong(temp, node);
                        heQueue.clear(threadID);
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
        Node* node = heQueue.get_protected(0, head, threadID);
        while (node != tail.load()) 
        {
            Node* next = heQueue.get_protected(1, node->next, threadID);
            if (head.compare_exchange_strong(node, next)) 
            {
                T* item = next->item;
                heQueue.retireNode(node, threadID);
                heQueue.clear(threadID);
                return item;
            }
            node = heQueue.get_protected(0, head, threadID);
        }
        heQueue.clear(threadID);
        return nullptr;
    }

    uint64_t getRetiredCountQueue(int threadID){
        return heQueue.getRetiredNodeCount(threadID);
    }
};

#endif