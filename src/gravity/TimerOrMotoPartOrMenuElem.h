#pragma once

class TimerOrMotoPartOrMenuElem {
public:
    int xF16 = 0;
    int yF16 = 0;
    int angleF16 = 0;
    int field_382 = 0;
    int field_383 = 0;
    int field_384 = 0;
    int field_385 = 0;
    int field_386 = 0;
    int field_387 = 0;
    int timerNo = 0;

    TimerOrMotoPartOrMenuElem() { setToZeros(); }

    void setToZeros() {
        xF16 = yF16 = angleF16 = 0;
        field_382 = field_383 = field_384 = 0;
        field_385 = field_386 = field_387 = 0;
    }
};
