#include "MathF16.h"
#include <cmath>

int MathF16::sinF16(int angle) {
    return (int)roundf(sinf(angle / 65536.0f) * 65536.0f);
}

int MathF16::cosF16(int angle) {
    return sinF16(PiHalfF16 - angle);
}

int MathF16::atan2F16(int dx, int dy) {
    return (int)roundf(atan2f((float)dx, (float)dy) * 65536.0f);
}
