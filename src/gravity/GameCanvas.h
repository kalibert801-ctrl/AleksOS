#pragma once
#include <string>
#include <vector>
#include <memory>
#include "GDGfx.h"
#include "GDTimer.h"

class GamePhysics;

class GameCanvas {
    void processTimers();

    GDGfx* graphics = nullptr;
    int dx = 0, dy = 0;
    GamePhysics* gamePhysics = nullptr;
    int field_178 = 0, field_179 = 0;
    bool timerTriggered = false;
    int  field_184 = 0;
    std::string timerMessage;
    int  timerId = 0;
    std::vector<Timer> timers;
    std::vector<std::string> time10MsToStringCache = std::vector<std::string>(100);
    int  timeInSeconds = -1;
    inline static std::string stringWithTime;
    inline static int flagAnimationTime = 0;
    inline static int field_226 = 0;

public:
    int width = 320, height = 240, height2 = 240;
    bool isDrawingTime = true;
    int64_t gameTimeMs = 0;

    // Sprite stubs (no sprites in this port)
    inline static const int spriteOffsetX[18]={0,0,15,15,15,0,6,12,18,18,25,25,25,37,37,37,15,32};
    inline static const int spriteOffsetY[18]={10,25,16,20,10,0,0,0,8,0,0,6,12,0,6,12,29,18};
    inline static const int spriteSizeX[18]={15,15,8,8,3,6,6,6,7,7,12,12,12,12,12,12,16,17};
    inline static const int spriteSizeY[18]={15,15,4,4,3,10,10,10,8,8,6,6,6,6,6,6,11,22};

    GameCanvas();
    void init(GamePhysics* gp);
    void setViewPosition(int dx, int dy);
    int  getDx()           { return dx; }
    int  addDx(int x)      { return x + dx; }
    int  addDy(int y)      { return -y + dy; }
    void drawLine(int x, int y, int x2, int y2);
    void drawLineF16(int x, int y, int x2, int y2);
    void method_142(int v1, int v2, int v3, int v4);
    void drawCircle(int x, int y, int size);
    void fillCircle(int x, int y, int r);
    void fillRect(int x, int y, int w, int h);
    void drawForthSpriteByCenter(int cx, int cy);
    void drawHelmet(int x, int y, int angleF16);
    void renderBodyPart(int x1F16, int y1F16, int x2F16, int y2F16, int bpNo);
    void renderBodyPart(int x1F16, int y1F16, int x2F16, int y2F16, int bpNo, int tF16);
    void drawWheelTires(int x, int y, int thin);
    void renderEngine(int x, int y, int angleF16);
    void renderFender(int x, int y, int angleF16);
    void drawTime(int64_t time10Ms);
    void method_150(int v1);
    static void method_151();
    void renderStartFlag(int x, int y);
    void renderFinishFlag(int x, int y);
    void clearScreenWithWhite();
    void setColor(int r, int g, int b);
    void method_161(int v1, bool mode);
    void scheduleGameTimerTask(const std::string& msg, int delayMs);
    void drawGame(GDGfx* g);

    // Entry point used by gravity.cpp
    void drawSprite(GDGfx* g, int spriteNo, int x, int y) { /* no sprites */ }
    int  calcSpriteNo(int angleF16, int v2, int v3, int v4, bool v5);
};
