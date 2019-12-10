#ifndef _STACK_URCU_H_
#define _STACK_URCU_H_

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "URCU.hpp"

template<typename T>
class StackURCU
{

private:
    struct Node {
        T* item;
        std::atomic<Node*> next;

        Node(T* userItem) : item{userItem}, next{nullptr} { }
    };

    std::atomic<Node*> top;
    int max_threads;
    URCU urcu {max_threads};

public:

    StackURCU(int max_threads=128) : max_threads{max_threads} 
    {
        Node* sentinel_node = new Node(nullptr);
        top.store(sentinel_node, std::memory_order_relaxed);
    }

    ~StackURCU()
    {
        while(pop(0) != nullptr);
        delete top.load();
    }
    
    bool push(T* item, int threadId)
    {
        // std::cout<<"Inside push\n";
        Node* node = new Node(item);
        urcu.readLock(threadId);
        while(true)
        {
            Node* temp = top.load();
            node->next.store(temp, std::memory_order_relaxed);
            if(top.compare_exchange_strong(temp,node))
            {
                // hpStack.clear(threadID);
                urcu.readUnlock(threadId);
                return true;
            }
        }   
    }

    T* pop(int threadId)
    {
        // std::cout<<"Inside pop\n";
        Node *temp, *next;
        T* ret_data;
        std::vector<Node*> retired;

        while(true)
        {
            urcu.readLock(threadId);
            temp = top.load();
            if(temp == nullptr)
            {
                urcu.readUnlock(threadId);
                return nullptr;
            }
            if(top.load() != temp)
            {
                urcu.readUnlock(threadId);
                continue;
            }
            next = temp->next.load();
            if(top.compare_exchange_strong(temp, next))
            {
                break;
            }
        }
        ret_data = temp->item;
        retired.push_back((Node*)temp);
        urcu.readUnlock(threadId);
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