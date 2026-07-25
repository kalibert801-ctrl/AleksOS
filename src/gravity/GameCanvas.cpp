#include "GameCanvas.h"
#include "GamePhysics.h"
#include "MathF16.h"
#include "GDMicro.h"

GameCanvas::GameCanvas() {
    width = height = height2 = 240; // overridden by init
    dx = 0; dy = height2;
}

void GameCanvas::init(GamePhysics* gp) {
    gamePhysics = gp;
    width  = 320; height = height2 = 240;
    dx = 0; dy = height2;
    gamePhysics->setMinimalScreenWH(width < height2 ? width : height2);
}

void GameCanvas::setViewPosition(int ndx, int ndy) {
    dx = ndx; dy = ndy;
    gamePhysics->setRenderMinMaxX(-dx, -dx + width);
}

void GameCanvas::drawLine(int x, int y, int x2, int y2) {
    graphics->drawLine(addDx(x), addDy(y), addDx(x2), addDy(y2));
}

void GameCanvas::drawLineF16(int x, int y, int x2, int y2) {
    graphics->drawLine(addDx(x<<2>>16), addDy(y<<2>>16), addDx(x2<<2>>16), addDy(y2<<2>>16));
}

void GameCanvas::renderBodyPart(int x1F16, int y1F16, int x2F16, int y2F16, int bpNo) {
    // sprites not loaded — no-op
}
void GameCanvas::renderBodyPart(int x1F16, int y1F16, int x2F16, int y2F16, int bpNo, int tF16) {
    // sprites not loaded — no-op
}

void GameCanvas::method_142(int v1, int v2, int v3, int v4) {
    ++v3;
    int cx = addDx(v1 - v3), cy = addDy(v2 + v3);
    int d = v3 << 1;
    if ((v4 = -((int)(((int64_t)((int)((int64_t)v4 * 11796480L >> 16)) << 32) / 205887L >> 16))) < 0) v4 += 360;
    graphics->drawArc(cx, cy, d, d, (v4 >> 16) + 170, 90);
}

void GameCanvas::drawCircle(int x, int y, int size) {
    int r = size / 2;
    graphics->drawArc(addDx(x - r), addDy(y + r), size, size, 0, 360);
}

void GameCanvas::fillCircle(int x, int y, int r) {
    graphics->fillCircle(addDx(x), addDy(y), r);
}

void GameCanvas::fillRect(int x, int y, int w, int h) {
    graphics->fillRect(addDx(x), addDy(y), w, h);
}

void GameCanvas::drawForthSpriteByCenter(int cx, int cy) {
    // sprite 4 is a small dot — draw a filled rect instead
    graphics->fillRect(addDx(cx - 2), addDy(cy + 2), 4, 4);
}

void GameCanvas::drawHelmet(int x, int y, int angleF16) { /* sprites off */ }
void GameCanvas::drawWheelTires(int x, int y, int thin)  { /* sprites off */ }
void GameCanvas::renderEngine(int x, int y, int angleF16) { /* sprites off */ }
void GameCanvas::renderFender(int x, int y, int angleF16) { /* sprites off */ }

int GameCanvas::calcSpriteNo(int angleF16, int v2, int v3, int v4, bool v5) {
    for (angleF16 += v2; angleF16 < 0; angleF16 += v3) {}
    while (angleF16 >= v3) angleF16 -= v3;
    if (v5) angleF16 = v3 - angleF16;
    int r = (int)((int64_t)((int)(((int64_t)angleF16 << 32) / (int64_t)v3 >> 16)) * (int64_t)(v4 << 16) >> 16);
    return r >> 16 < v4 - 1 ? r >> 16 : v4 - 1;
}

void GameCanvas::drawTime(int64_t time10Ms) {
    int secs = (int)(time10Ms / 100L);
    int ms10 = (int)(time10Ms % 100L);
    if (timeInSeconds != secs || stringWithTime.empty()) {
        std::string pad = (secs % 60 >= 10) ? "" : "0";
        stringWithTime = std::to_string(secs / 60) + ":" + pad + std::to_string(secs % 60) + ".";
        timeInSeconds = secs;
    }
    if (time10MsToStringCache[ms10].empty()) {
        std::string pad = (ms10 >= 10) ? "" : "0";
        time10MsToStringCache[ms10] = pad + std::to_string(ms10);
    }
    setColor(0, 0, 0);
    if (time10Ms > 3600000L) {
        graphics->drawString("0:00.00", width - 50, height2 - 5, GDGfx::RIGHT | GDGfx::BOTTOM);
    } else {
        graphics->drawString(stringWithTime, width - 25, height2 - 5, GDGfx::RIGHT | GDGfx::BOTTOM);
        graphics->drawString(time10MsToStringCache[ms10], width - 25, height2 - 5, GDGfx::LEFT | GDGfx::BOTTOM);
    }
}

void GameCanvas::method_150(int v1) {
    if (timerId == v1) timerTriggered = true;
}

void GameCanvas::method_151() {
    field_226 += 655;
    int sv = MathF16::sinF16(field_226);
    int v0 = 32768 + ((sv < 0 ? -sv : sv) >> 1);
    flagAnimationTime += (int)(6553L * (int64_t)v0 >> 16);
}

void GameCanvas::renderStartFlag(int x, int y) {
    if (flagAnimationTime > 229376) flagAnimationTime = 0;
    setColor(0, 0, 0);
    drawLine(x, y, x, y + 32);
    // Draw simple flag triangle instead of sprite
    setColor(255, 255, 255);
    graphics->fillRect(addDx(x), addDy(y + 32) - 8, 12, 8);
    setColor(0, 0, 0);
}

void GameCanvas::renderFinishFlag(int x, int y) {
    if (flagAnimationTime > 229376) flagAnimationTime = 0;
    setColor(0, 0, 0);
    drawLine(x, y, x, y + 32);
    // Draw checkerboard finish flag
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 4; ++j)
            if ((i + j) % 2 == 0) {
                graphics->fillRect(addDx(x + i*6), addDy(y + 32 - j*4) - 4, 6, 4);
            }
}

void GameCanvas::clearScreenWithWhite() {
    graphics->setColor(255, 255, 255);
    graphics->fillRect(0, 0, width, height2);
}

void GameCanvas::setColor(int r, int g, int b) {
    // isInGameMenu = false always
    graphics->setColor(r, g, b);
}

void GameCanvas::method_161(int v1, bool mode) {
    int h = mode ? height : height2;
    setColor(0, 0, 0);
    graphics->fillRect(1, h - 4, width - 2, 3);
    setColor(255, 255, 255);
    graphics->fillRect(2, h - 3, (int)((int64_t)((width - 4) << 16) * (int64_t)v1 >> 16) >> 16, 1);
}

void GameCanvas::scheduleGameTimerTask(const std::string& msg, int delayMs) {
    timerTriggered = false;
    ++timerId;
    timerMessage = msg;
    timers.push_back(Timer(timerId, delayMs));
}

void GameCanvas::processTimers() {
    for (auto i = timers.begin(); i != timers.end(); ) {
        if (i->ready()) { method_150(i->getId()); i = timers.erase(i); }
        else ++i;
    }
}

void GameCanvas::drawGame(GDGfx* g) {
    graphics = g;
    processTimers();

    gamePhysics->setMotoComponents();
    setViewPosition(-gamePhysics->getCamPosX() + field_178 + width / 2,
                     gamePhysics->getCamPosY() + field_179 + height2 / 2);
    gamePhysics->renderGame(this);

    if (isDrawingTime) drawTime(gameTimeMs / 10L);

    if (!timerMessage.empty()) {
        setColor(0, 0, 0);
        graphics->drawString(timerMessage, width / 2, height2 / 4,
                             GDGfx::HCENTER | GDGfx::VCENTER);
        if (timerTriggered) { timerTriggered = false; timerMessage = ""; }
    }

    int prog = gamePhysics->method_52();
    method_161(prog, false);

    graphics = nullptr;
}
