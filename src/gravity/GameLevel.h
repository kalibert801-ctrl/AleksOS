#pragma once
#include <cstdint>
#include <vector>
#include "GDFileStream.h"

class GameCanvas;

class GameLevel {
    int minX = 0, maxX = 0;
    int field_264 = 0, field_265 = 0, field_266 = 0, field_277 = 0;

public:
    int startPosX = 0, startPosY = 0;
    int finishPosX = 13107200, finishPosY = 0;
    int startFlagPoint = 0, finishFlagPoint = 0;
    int pointsCount = 0;
    int field_274 = 0;
    std::vector<std::vector<int>> pointPositions;

    GameLevel();
    ~GameLevel() {}
    void init();
    void method_174(int v1, int v2, int v3, int v4);
    int method_181(int v1);
    void setMinMaxX(int minX, int maxX);
    void method_183(int v1, int v2);
    void method_184(int v1, int v2, int v3);
    void renderShadow(GameCanvas* gc, int v2, int v3);
    void renderLevel3D(GameCanvas* gc, int xF16, int yF16);
    void renderTrackNearestGreenLine(GameCanvas* gc);
    void addPointSimple(int v1, int v2);
    void addPoint(int x, int y);
    void load(FileStream* inStream);
};
