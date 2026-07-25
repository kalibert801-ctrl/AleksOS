#pragma once
#include <cstdint>

class MathF16 {
public:
    static const int PiF16     = 205887;
    static const int PiHalfF16 = 102944;

    static int sinF16(int angle);
    static int cosF16(int angle);
    static int atan2F16(int dx, int dy);
};
