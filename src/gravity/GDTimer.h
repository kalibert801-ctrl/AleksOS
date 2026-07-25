#pragma once
#include <Arduino.h>

class Timer {
    int _id;
    uint32_t _dueMs;
public:
    Timer(int id, int64_t timeoutMs)
        : _id(id), _dueMs(millis() + (uint32_t)timeoutMs) {}

    bool ready()  { return millis() >= _dueMs; }
    int  getId()  { return _id; }
};
