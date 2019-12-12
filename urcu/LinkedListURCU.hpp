#ifndef _LINKED_LIST_URCU_H_
#define _LINKED_LIST_URCU_H_

#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <vector>
#include "URCU.hpp"

#define MAX_THREAD_COUNT 40

template<typename T> 
class LinkedListURCU {

private:
    struct Node {
        T* key;
        std::atomic<Node*> next;

        Node(T* key) : key{key}, next{nullptr} { }
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    int max_threads;

    URCU urcu {max_threads};

    long retired_nodes_count[MAX_THREAD_COUNT];

public:

    LinkedListURCU(const int max_threads) : max_threads{max_threads} 
    {
        this->max_threads = max_threads;
        head.store(new Node(nullptr));
        tail.store(new Node(nullptr));
        head.load()->next.store(tail.load());

        for(int i=0; i<max_threads; i++)
        {
            retired_nodes_count[i] = 0;
        }

    }

    ~LinkedListURCU() {
        Node *pred = head.load();
        Node *node = pred->next.load();
        while (node != nullptr) {
            delete pred;
            pred = node;
            node = pred->next.load();
        }
        delete pred;
    }
    
    bool add(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        std::vector<Node*> retired;
        Node* newNode = new Node(key);
        urcu.readLock(tid);
        while (true) 
        {
            if (find(key, &pred, &curr, &next, retired, tid)) 
            {
                delete newNode;
                urcu.readUnlock(tid);
                deleteRetired(retired, tid);
                return false;
            }
            newNode->next.store(curr, std::memory_order_relaxed);
            Node *tmp = getUnmarked(curr);
            if (pred->compare_exchange_strong(tmp, newNode)) 
            { 
                urcu.readUnlock(tid);
                deleteRetired(retired, tid);
                return true;
            }
        }
    }

    bool remove(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        std::vector<Node*> retired;
        urcu.readLock(tid);
        while (true) 
        {
            
            if (!find(key, &pred, &curr, &next, retired, tid)) 
            {
                urcu.readUnlock(tid);
                deleteRetired(retired, tid);
                return false;
            }
            
            Node *tmp = getUnmarked(next);
            if (!curr->next.compare_exchange_strong(tmp, getMarked(next))) 
            {
                continue; 
            }

            tmp = getUnmarked(curr);
            if (pred->compare_exchange_strong(tmp, getUnmarked(next))) 
            {
                urcu.readUnlock(tid);
                urcu.synchronizeRCU();
                delete getUnmarked(curr);
            } else {
                urcu.readUnlock(tid);
            }
            deleteRetired(retired, tid);
            
            return true;
        }
    }

    bool contains(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        std::vector<Node*> retired;
        urcu.readLock(tid);
        bool isContains = find(key, &pred, &curr, &next, retired, tid);
        urcu.readUnlock(tid);
        deleteRetired(retired, tid);
        return isContains;
    }

private:
    bool find (T* key, std::atomic<Node*> **par_pred, Node **par_curr, Node **par_next, std::vector<Node*>& retired, const int tid)
    {
        std::atomic<Node*> *pred;
        Node *curr, *next;

     try_again:
        pred = &head;
        curr = pred->load();
        if (pred->load() != getUnmarked(curr)) goto try_again;
        while (true) {
            if (getUnmarked(curr) == nullptr) break;
            next = curr->next.load();
            if (getUnmarked(next) == tail.load()) break;
            if (getUnmarked(curr)->next.load() != next) goto try_again;
            if (pred->load() != getUnmarked(curr)) goto try_again;
            if (getUnmarked(next) == next) {
                 // !cmark in the paper
                if (getUnmarked(curr)->key != nullptr && !(*getUnmarked(curr)->key < *key)) 
                {
                    *par_curr = curr;
                    *par_pred = pred;
                    *par_next = next;
                    return (*getUnmarked(curr)->key == *key);
                }
                pred = &getUnmarked(curr)->next;
            } else 
            {    
                Node *tmp = getUnmarked(curr);
                if (!pred->compare_exchange_strong(tmp, getUnmarked(next))) 
                {
                    goto try_again;
                }
                
                retired.push_back(getUnmarked(curr));
                retired_nodes_count[tid]++;
            }
            curr = next;
        }
        *par_curr = curr;
        *par_pred = pred;
        *par_next = next;
        return false;
    }

    bool isMarked(Node * node) {
    	return ((size_t) node & 0x1);
    }

    Node * getMarked(Node * node) {
    	return (Node*)((size_t) node | 0x1);
    }

    Node * getUnmarked(Node * node) {
    	return (Node*)((size_t) node & (~0x1));
    }

    void deleteRetired(std::vector<Node*>& retired, const int tid) {
        if (retired.size() > 0) 
        {
            urcu.synchronizeRCU();
            for (auto retNode : retired) 
            {
                retired_nodes_count[tid]--;
                delete retNode;
            }
        }
    }

public:
    int getRetiredNodesCount(const int tid)
    {
        return retired_nodes_count[tid];
    }
};

#endif 
