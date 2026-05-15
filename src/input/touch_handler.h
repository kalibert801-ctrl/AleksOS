#pragma once
#include <Arduino.h>

enum TapType { TAP_NONE, TAP_SINGLE, TAP_DOUBLE };

class TouchHandler {
public:
    void init();
    bool isTouched();           // edge: true only on new press
    bool rawTouched();          // level: true WHILE finger is held down (for NES controls)
    void getXY(int &x, int &y);
    TapType checkDoubleTap(int x, int y, int row);
private:
    int      _px = 0, _py = 0;
    bool     _pressed = false;
    uint32_t _pressTime = 0;
    uint32_t _lastTapTime = 0;
    int      _lastTapRow  = -1;
};

extern TouchHandler touch;
