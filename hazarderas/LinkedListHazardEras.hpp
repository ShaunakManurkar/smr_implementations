#ifndef LINKED_LIST_HAZARD_ERAS_HPP
#define LINKED_LIST_HAZARD_ERAS_HPP

#include <atomic>
#include <thread>
#include <forward_list>
#include <set>
#include <iostream>
#include <string>
#include "hazarderas.hpp"

template<typename T>
class LinkedList_HazardEras {

private:
    struct Node {
        T* item;
        uint64_t newEra;
        uint64_t delEra;
        std::atomic<Node*> next;

        Node(T* item, uint64_t newEra) 
        { 
            this->item = item;
            this->next.store(nullptr);
            this->newEra = newEra;
            this->delEra = 0;
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
    int numThreads;
    hazardEras<Node> heList {numThreads};

public:

    LinkedList_HazardEras(int numThreads) 
    {
        numThreads = numThreads;
        Node* sentinel = new Node(nullptr, 1);
        head.store(sentinel);
        tail.store(sentinel);
        head.load()->next.store(tail.load());
    }

    bool add(T* item, const int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        Node* node = new Node(item, heList.getEra());
        while (true) {
            if (find(item, &pred, &curr, &next, threadID)) 
            {
                delete node;
                heList.clear(threadID);
                return false;
            }
            node->next.store(curr, std::memory_order_relaxed);
            Node *temp = getUnmarked(curr);
            if (pred->compare_exchange_strong(temp, node)) 
            {
                heList.clear(threadID);
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
                heList.clear(threadID);
                return false;
            }
            Node *temp = getUnmarked(next);
            if (!curr->next.compare_exchange_strong(temp, getMarked(next))) {
                continue;
            }
            temp = getUnmarked(curr);
            if (pred->compare_exchange_strong(temp, getUnmarked(next))) {
                heList.clear(threadID);
                heList.retireNode(getUnmarked(curr), threadID);
            } else {
                heList.clear(threadID);
            }
            return true;
        }
    }

    bool contains (T* item, const int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        return find(item, &pred, &curr, &next, threadID);
        //heList.clear(threadID);
    }

    uint64_t getRetiredCountLinkedList(int threadID){
        return heList.getRetiredNodeCount(threadID);
    }

private:

    bool find (T* item, std::atomic<Node*> **par_pred, Node **par_curr, Node **par_next, const int threadID)
    {
        std::atomic<Node*> *pred;
        Node *curr, *next;

     try_again:
        pred = &head;
        curr = heList.get_protected(1, *pred, threadID);
        while (true) {
            if (getUnmarked(curr) == nullptr) 
            {
                break;
            }
            next = heList.get_protected(0, curr->next, threadID);
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
                if (getUnmarked(curr)->item != nullptr && !(*getUnmarked(curr)->item < *item)) { // Check for null to handle head and tail
                    *par_curr = curr;
                    *par_pred = pred;
                    *par_next = next;
                    return (*getUnmarked(curr)->item == *item);
                }
                pred = &getUnmarked(curr)->next;
                heList.protectEraRelease(2, 1, threadID);
            } 
            else 
            {
                Node *temp = getUnmarked(curr);
                if (!pred->compare_exchange_strong(temp, getUnmarked(next))) 
                {
                    goto try_again;
                }
                heList.retireNode(getUnmarked(curr), threadID);
            }
            curr = next;
            heList.protectEraRelease(1, 0, threadID);
        }
        *par_curr = curr;
        *par_pred = pred;
        *par_next = next;
        return false;
    }
};

#endif
