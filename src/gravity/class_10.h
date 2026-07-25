#pragma once
#include <vector>
#include <memory>
#include "TimerOrMotoPartOrMenuElem.h"

class class_10 {
public:
    bool unusedBool = true;
    int field_257 = 0;
    int field_258 = 0;
    int field_259 = 0;
    int field_260 = 0;
    std::vector<std::unique_ptr<TimerOrMotoPartOrMenuElem>> motoComponents
        = std::vector<std::unique_ptr<TimerOrMotoPartOrMenuElem>>(6);

    class_10();
    ~class_10() {}
    void reset();
};
