#ifndef LINKED_LIST_HAZARD_POINTER_HPP
#define LINKED_LIST_HAZARD_POINTER_HPP

#include <atomic>
#include <iostream>
#include "hazardpointer.hpp"

template<typename T> 
class LinkedList_HazardPointer {

private:
    struct Node {
        T* item;
        std::atomic<Node*> next;

        Node(T* item) 
        {
            this->item = item;
            this->next.store(nullptr);
        }
    };

    bool isMarked(Node * node) {
    	return ((size_t) node & 0x1);
    }

    Node * getMarked(Node * node) {
    	return (Node*)((size_t) node | 0x1);
    }

    Node * getUnmarked(Node * node) {
    	return (Node*)((size_t) node & (~0x1));
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    int numofThreads;
    hazardPointers<Node> hpList {numofThreads};

public:

    LinkedList_HazardPointer(int numThreads) 
    {
        numofThreads = numThreads;
        Node* sentinel = new Node(nullptr);
        head.store(sentinel);
        tail.store(sentinel);
        head.load()->next.store(tail.load());
    }

    bool add(T* item, const int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        Node* node = new Node(item);
        while (true) 
        {
            if (find(item, &pred, &curr, &next, threadID)) 
            {
                delete node;
                hpList.clear(threadID);
                return false;
            }
            node->next.store(curr, std::memory_order_relaxed);
            Node *temp = getUnmarked(curr);
            if (pred->compare_exchange_strong(temp, node)) 
            {
                hpList.clear(threadID);
                return true;
            }
        }
    }


    bool remove(T* item, const int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        while (true) 
        {
            if (!find(item, &pred, &curr, &next, threadID)) {
                hpList.clear(threadID);
                return false;
            }
            Node *temp = getUnmarked(next);
            if (!curr->next.compare_exchange_strong(temp, getMarked(next))) {
                continue;
            }
            temp = getUnmarked(curr);
            if (pred->compare_exchange_strong(temp, getUnmarked(next))) 
            {
                hpList.clear(threadID);
                hpList.retireNode(getUnmarked(curr), threadID);
            } else {
                hpList.clear(threadID);
            }
            return true;
        }
    }

    bool contains (T* item, const int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        return find(item, &pred, &curr, &next, threadID);
    }

    uint64_t getRetiredCountLinkedList(int threadID){
        return hpList.getRetiredCount(threadID);
    }


private:

    bool find (T* item, std::atomic<Node*> **par_pred, Node **par_curr, Node **par_next, const int threadID)
    {
        std::atomic<Node*> *pred;
        Node *curr, *next;

     try_again:
        pred = &head;
        curr = pred->load();
        hpList.storeHazardPtr(1, curr, threadID);
        while (true) {
            if (getUnmarked(curr) == nullptr) 
            {
                break;
            }
            next = curr->next.load();
            hpList.storeHazardPtr(0, getUnmarked(next), threadID);
            if (getUnmarked(curr)->next.load() != next) 
            {
                goto try_again;
            }
            if (getUnmarked(next) == tail.load()) 
            {
                break;
            }
            if (pred->load() != getUnmarked(curr)) 
            {
                goto try_again;
            }
            if (getUnmarked(next) == next)
            { 
                if (getUnmarked(curr)->item != nullptr && !(*getUnmarked(curr)->item < *item)) 
                {
                    *par_curr = curr;
                    *par_pred = pred;
                    *par_next = next;
                    return (*getUnmarked(curr)->item == *item);
                }
                pred = &getUnmarked(curr)->next;
                hpList.storeHazardPtr(2, getUnmarked(curr), threadID);
            } 
            else 
            {
                Node *temp = getUnmarked(curr);
                if (!pred->compare_exchange_strong(temp, getUnmarked(next))) 
                {
                    goto try_again;
                }
                hpList.retireNode(getUnmarked(curr), threadID);
            }
            curr = next;
            hpList.storeHazardPtr(1, getUnmarked(next), threadID);
        }
        *par_curr = curr;
        *par_pred = pred;
        *par_next = next;
        return false;
    }
};

#endif
