#ifndef LINKED_LIST_IBR_HPP
#define LINKED_LIST_IBR_HPP

#include <atomic>
#include <iostream>
#include "IntervalBasedReclamation.hpp"

using namespace std;

#define EPOCH_FREQUENCY 150
#define RECLAIM_FREQUENCY 30

template<typename T>
class LinkedList_IBR {

private:
    struct Node {
        T* item;
        uint64_t birth_epoch;
        uint64_t retire_epoch;
        std::atomic<Node*> next;

        Node(T* item) 
        { 
            this->item = item;
            this->next.store(nullptr);
            this->birth_epoch = 0;
            this->retire_epoch = 0;
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
    IntervalBasedReclamation<Node> ibrList {numThreads,EPOCH_FREQUENCY,RECLAIM_FREQUENCY};

public:

    LinkedList_IBR(int numThreads) 
    {
        numThreads = numThreads;
        Node* sentinel = new Node(nullptr);
        head.store(sentinel);
        tail.store(sentinel);
        head.load()->next.store(tail.load());
    }

    bool add(T* item, int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        Node* node = new Node(item);
        node = ibrList.allocNode(threadID, node);
        ibrList.start_op(threadID);
        while (true) {
            if (find(item, &pred, &curr, &next, threadID)) 
            {
                delete node;
                ibrList.end_op(threadID);
                return false;
            }
            node->next.store(curr, std::memory_order_relaxed);
            Node *temp = getUnmarked(curr);
            if (pred->compare_exchange_strong(temp, node)) 
            {
                ibrList.end_op(threadID);
                return true;
            }
        }
    }

    bool remove(T* item, int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        ibrList.start_op(threadID);
        while (true) 
        {
            if (!find(item, &pred, &curr, &next, threadID)) {
                ibrList.end_op(threadID);
                return false;
            }
            Node *temp = getUnmarked(next);
            if (!curr->next.compare_exchange_strong(temp, getMarked(next))) {
                continue;
            }
            temp = getUnmarked(curr);
            if (pred->compare_exchange_strong(temp, getUnmarked(next))) 
            {
                ibrList.retireNode(getUnmarked(curr), threadID);
            }
            ibrList.end_op(threadID);
            return true;
        }
    }

    bool contains (T* item, int threadID)
    {
        Node *curr, *next;
        std::atomic<Node*> *pred;
        return find(item, &pred, &curr, &next, threadID);
    }

    uint64_t getRetiredCountLinkedList(int threadID)
    {
        return ibrList.getRetiredNodeCount(threadID);
    }


private:

    bool find (T* item, std::atomic<Node*> **par_pred, Node **par_curr, Node **par_next, const int threadID)
    {
        std::atomic<Node*> *pred;
        Node *curr, *next;
        ibrList.start_op(threadID);
     try_again:
        pred = &head;
        curr = pred->load();
        while (true) {
            if (getUnmarked(curr) == nullptr) 
            {
                break;
            }
            next = curr->next.load();
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
                    ibrList.end_op(threadID);
                    return (*getUnmarked(curr)->item == *item);
                }
                pred = &getUnmarked(curr)->next;
            } 
            else 
            {
                Node *temp = getUnmarked(curr);
                if (!pred->compare_exchange_strong(temp, getUnmarked(next))) 
                {
                    goto try_again;
                }
                ibrList.retireNode(getUnmarked(curr), threadID);
            }
            curr = next;
        }
        *par_curr = curr;
        *par_pred = pred;
        *par_next = next;
        ibrList.end_op(threadID);
        return false;
    }
};

#endif