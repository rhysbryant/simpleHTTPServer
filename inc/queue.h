/*
 *  Copyright (c) 2023 Rhys Bryant
 *  Author Rhys Bryant
 *
 *	This file is part of SimpleHTTP
 *
 *   SimpleHTTP is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   any later version.
 *
 *   SimpleHTTP is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with SimpleHTTP.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <iostream>

template <typename T>
class LinkedListQueue {
private:
    struct Node {
        T data;
        Node* next;
        Node(T val) : data(val), next(nullptr) {}
    };

    Node* head;
    Node* tail;
    size_t _size;

    int count;

public:
    LinkedListQueue() : head(nullptr), tail(nullptr), _size(0),count(0) {}

    ~LinkedListQueue() {
        while (!empty()) {
            pop();
        }
    }

    bool empty() const {
        return _size == 0;
    }

    size_t size() const {
        return _size;
    }

    void push(const T& value) {
        Node* newNode = new Node(value);
        if (tail) {
            tail->next = newNode;
        } else {
            head = newNode;
        }
        tail = newNode;
        ++_size;
    }

    void pop() {
        if (empty()) {
            return;  // No operation if the queue is empty
        }

        Node* oldHead = head;
        head = head->next;
        if (head == nullptr) {
            tail = nullptr;
        }
        delete oldHead;
        --_size;
    }

    T& front() {
        // Return a reference to the front element, or a default value if empty
        static T default_value{};  // Default-constructed value of T
        return head ? head->data : default_value;
    }

    const T& front() const {
        // Return a reference to the front element, or a default value if empty
        static T default_value{};  // Default-constructed value of T
        return head ? head->data : default_value;
    }

    T& back() {
        // Return a reference to the back element, or a default value if empty
        static T default_value{};  // Default-constructed value of T
        return tail ? tail->data : default_value;
    }

    const T& back() const {
        // Return a reference to the back element, or a default value if empty
        static T default_value{};  // Default-constructed value of T
        return tail ? tail->data : default_value;
    }
};
