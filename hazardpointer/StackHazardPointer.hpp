#ifndef STACK_HAZARD_POINTER_HPP
#define STACK_HAZARD_POINTER_HPP

#include<atomic>
#include<iostream>
#include "hazardpointer.hpp"

template<typename T>
class Stack_HazardPointer
{

private:
    struct Node {
        T* item;
        std::atomic<Node*> next;

        Node(T* item)
        {
            item = item;
            next.store(nullptr);
        }
    };

    std::atomic<Node*> top;
    const int numOfThreads;
    hazardPointers<Node> hpStack{numOfThreads};

public:

    Stack_HazardPointer(int numOfThreads) : numOfThreads{numOfThreads}
    {
        Node* sentinel = new Node(nullptr);
        top.store(sentinel, std::memory_order_relaxed);
    }

    ~Stack_HazardPointer()
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
        Node* node = new Node(item);
        while(true)
        {
            Node* temp = hpStack.storeHazardPtr(0,top,threadID);
            if(temp == top.load())
            {
                node->next.store(temp, std::memory_order_relaxed);
                if(top.compare_exchange_strong(temp,node))
                {
                    hpStack.clear(threadID);
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
            temp = hpStack.protect(0, top, threadID);
            if(temp == nullptr)
            {
                hpStack.clear(threadID);
                return nullptr;
            }
            if(top.load() != temp)
            {
                continue;
            }
            next = temp->next.load();
            if(top.compare_exchange_strong(temp, next))
            {
                break;
            }
        }
        ret_data = temp->item;
        hpStack.retireNode(temp,threadID);
        hpStack.clear(threadID);
        return ret_data;
    }
};

#endif