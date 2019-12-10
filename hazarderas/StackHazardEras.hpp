#ifndef STACK_HAZARD_ERAS_HPP
#define STACK_HAZARD_ERAS_HPP

#include <atomic>
#include <iostream>
#include "hazardEras.hpp"

template<typename T>
class Stack_HazardEras {

private:
    struct Node {
        T* item;
        uint64_t newEra;
        uint64_t delEra;
        std::atomic<Node*> next;

        Node(T* item, uint64_t birthEra) 
        { 
            item = item;
            next.store(nullptr);
            newEra = birthEra;
            delEra = 0;
        }
    };

    std::atomic<Node*> top;
    const int numOfThreads;
    hazardEras<Node> heStack{numOfThreads};

public:

    Stack_HazardEras(int numOfThreads) : numOfThreads{numOfThreads}
    {
        Node* sentinel = new Node(nullptr,1);
        top.store(sentinel, std::memory_order_relaxed);
    }

    ~Stack_HazardEras()
    {
        while(pop(0) != nullptr);
        delete top.load();
    }

    bool push(T* item, int threadID)
    {
        if (item == nullptr)
        {
            return false;
        }
        Node* node = new Node(item, heStack.getEra());
        while(true)
        {
            Node* temp = heStack.get_protected(0, top, threadID);
            if(temp == top.load())
            {
                node->next.store(temp, std::memory_order_relaxed);
                if(top.compare_exchange_strong(temp,node))
                {
                    heStack.clear(threadID);
                    return true;
                }
            }
        }   
    }

    T* pop(int threadID)
    {
        Node *temp, *next;
        T* ret_data;
        while(true)
        {
            temp = heStack.get_protected(0, top, threadID);
            if(temp == nullptr)
            {
                heStack.clear(threadID);
                return nullptr;
            }
            if(top.load() != temp)
            {
                continue;
            }
            next = temp->next.load();
            if(top.compare_exchange_strong(temp, next))
            {
                ret_data = temp->item;
                heStack.retireNode(temp,threadID);
                heStack.clear(threadID);
                return ret_data;
            }
        }
    }
};

#endif