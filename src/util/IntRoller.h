//
// Created by Lukas Tenbrink on 20.01.20.
//

#ifndef LED_FAN_INTROLLER_H
#define LED_FAN_INTROLLER_H


#include "RollerIterator.h"

class IntRoller {
public:
    const unsigned int count;
    int *data;
    unsigned int head = 0;

    IntRoller(int c);

    int& operator[](int index);

    void push(int value);

    void fill(int value);

    int last() {
        return (*this)[-1];
    }

    int min();
    int max();

    int sum();
    float mean();

    RollerIterator<int> begin() { return {data, head, count}; }
    RollerIterator<int> end() { return {}; }

    int countOccurrences(int v);
};

#endif //LED_FAN_INTROLLER_H
