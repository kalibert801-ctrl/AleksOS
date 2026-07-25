#include "GameLevel.h"
#include "GameCanvas.h"
#include "GamePhysics.h"
#include "LevelLoader.h"

GameLevel::GameLevel() { init(); }

void GameLevel::init() {
    startPosX = 0; startPosY = 0;
    finishPosX = 13107200;
    pointsCount = 0; field_274 = 0;
    startFlagPoint = 0; finishFlagPoint = 0;
}

void GameLevel::method_174(int v1, int v2, int v3, int v4) {
    startPosX  = v1 << 16 >> 3;
    startPosY  = v2 << 16 >> 3;
    finishPosX = v3 << 16 >> 3;
    finishPosY = v4 << 16 >> 3;
}

int GameLevel::method_181(int v1) {
    int v2 = v1 - pointPositions[startFlagPoint][0];
    int v3;
    return ((v3 = pointPositions[finishFlagPoint][0] - pointPositions[startFlagPoint][0]) < 0 ? -v3 : v3) >= 3 && v2 <= v3
        ? (int)(((int64_t)v2 << 32) / (int64_t)v3 >> 16) : 65536;
}

void GameLevel::setMinMaxX(int mn, int mx) {
    minX = mn << 16 >> 3;
    maxX = mx << 16 >> 3;
}

void GameLevel::method_183(int v1, int v2) {
    field_264 = v1 >> 1;
    field_265 = v2 >> 1;
}

void GameLevel::method_184(int v1, int v2, int v3) {
    field_264 = v1; field_265 = v2; field_266 = v3;
}

void GameLevel::renderShadow(GameCanvas* gc, int v2, int v3) {
    if (v3 <= pointsCount - 1) {
        int v4 = field_266 - ((pointPositions[v2][1] + pointPositions[v3 + 1][1]) >> 1) < 0
            ? 0 : field_266 - ((pointPositions[v2][1] + pointPositions[v3 + 1][1]) >> 1);
        if (field_266 <= pointPositions[v2][1] || field_266 <= pointPositions[v3 + 1][1])
            v4 = v4 < 327680 ? v4 : 327680;

        field_277 = (int)((int64_t)field_277 * 49152L >> 16) + (int)((int64_t)v4 * 16384L >> 16);
        if (field_277 <= 557056) {
            int v5 = (int)(1638400L * (int64_t)field_277 >> 16) >> 16;
            gc->setColor(v5, v5, v5);
            int v6 = pointPositions[v2][0] - pointPositions[v2 + 1][0];
            if (v6 == 0) return;
            int v8 = (int)(((int64_t)(pointPositions[v2][1] - pointPositions[v2 + 1][1]) << 32) / (int64_t)v6 >> 16);
            int v9 = pointPositions[v2][1] - (int)((int64_t)pointPositions[v2][0] * (int64_t)v8 >> 16);
            int v10 = (int)((int64_t)field_264 * (int64_t)v8 >> 16) + v9;
            v6 = pointPositions[v3][0] - pointPositions[v3 + 1][0];
            if (v6 == 0) return;
            v8 = (int)(((int64_t)(pointPositions[v3][1] - pointPositions[v3 + 1][1]) << 32) / (int64_t)v6 >> 16);
            v9 = pointPositions[v3][1] - (int)((int64_t)pointPositions[v3][0] * (int64_t)v8 >> 16);
            int v11 = (int)((int64_t)field_265 * (int64_t)v8 >> 16) + v9;
            if (v2 == v3) {
                gc->drawLine(field_264 << 3 >> 16, (v10 + 65536) << 3 >> 16,
                             field_265 << 3 >> 16, (v11 + 65536) << 3 >> 16);
                return;
            }
            gc->drawLine(field_264 << 3 >> 16, (v10 + 65536) << 3 >> 16,
                         pointPositions[v2 + 1][0] << 3 >> 16,
                         (pointPositions[v2 + 1][1] + 65536) << 3 >> 16);
            for (int i = v2 + 1; i < v3; ++i)
                gc->drawLine(pointPositions[i][0] << 3 >> 16, (pointPositions[i][1] + 65536) << 3 >> 16,
                             pointPositions[i+1][0] << 3 >> 16, (pointPositions[i+1][1] + 65536) << 3 >> 16);
            gc->drawLine(pointPositions[v3][0] << 3 >> 16, (pointPositions[v3][1] + 65536) << 3 >> 16,
                         field_265 << 3 >> 16, (v11 + 65536) << 3 >> 16);
        }
    }
}

void GameLevel::renderLevel3D(GameCanvas* gc, int xF16, int yF16) {
    int v7 = 0, v8 = 0, lineNo;
    for (lineNo = 0; lineNo < pointsCount - 1 && pointPositions[lineNo][0] <= minX; ++lineNo) {}
    if (lineNo > 0) --lineNo;

    int v9  = xF16 - pointPositions[lineNo][0];
    int v10 = yF16 + 3276800 - pointPositions[lineNo][1];
    int v11 = GamePhysics::getSmthLikeMaxAbs(v9, v10);
    if (v11 < 4) { v9 = 0; v10 = 0; } else {
        v9  = (int)(((int64_t)v9  << 32) / (int64_t)(v11 >> 2) >> 16);
        v10 = (int)(((int64_t)v10 << 32) / (int64_t)(v11 >> 2) >> 16);
    }
    gc->setColor(0, 170, 0);

    while (lineNo < pointsCount - 1) {
        int v4 = v9, v5 = v10;
        v9  = xF16 - pointPositions[lineNo + 1][0];
        v10 = yF16 + 3276800 - pointPositions[lineNo + 1][1];
        v11 = GamePhysics::getSmthLikeMaxAbs(v9, v10);
        if (v11 < 4) { v9 = 0; v10 = 0; } else {
            v9  = (int)(((int64_t)v9  << 32) / (int64_t)(v11 >> 2) >> 16);
            v10 = (int)(((int64_t)v10 << 32) / (int64_t)(v11 >> 2) >> 16);
        }
        gc->drawLine((pointPositions[lineNo][0] + v4) << 3 >> 16,
                     (pointPositions[lineNo][1] + v5) << 3 >> 16,
                     (pointPositions[lineNo+1][0] + v9) << 3 >> 16,
                     (pointPositions[lineNo+1][1] + v10) << 3 >> 16);
        gc->drawLine(pointPositions[lineNo][0] << 3 >> 16, pointPositions[lineNo][1] << 3 >> 16,
                     (pointPositions[lineNo][0] + v4) << 3 >> 16,
                     (pointPositions[lineNo][1] + v5) << 3 >> 16);
        if (lineNo > 1) {
            if (pointPositions[lineNo][0] > field_264 && v7 == 0) v7 = lineNo - 1;
            if (pointPositions[lineNo][0] > field_265 && v8 == 0) v8 = lineNo - 1;
        }
        if (startFlagPoint  == lineNo) { gc->renderStartFlag ((pointPositions[startFlagPoint ][0]+v4)<<3>>16,(pointPositions[startFlagPoint ][1]+v5)<<3>>16); gc->setColor(0,170,0); }
        if (finishFlagPoint == lineNo) { gc->renderFinishFlag((pointPositions[finishFlagPoint][0]+v4)<<3>>16,(pointPositions[finishFlagPoint][1]+v5)<<3>>16); gc->setColor(0,170,0); }
        if (pointPositions[lineNo][0] > maxX) break;
        ++lineNo;
    }
    gc->drawLine(pointPositions[pointsCount-1][0] << 3 >> 16,
                 pointPositions[pointsCount-1][1] << 3 >> 16,
                 (pointPositions[pointsCount-1][0]+v9) << 3 >> 16,
                 (pointPositions[pointsCount-1][1]+v10) << 3 >> 16);
    if (LevelLoader::isEnabledShadows) renderShadow(gc, v7, v8);
}

void GameLevel::renderTrackNearestGreenLine(GameCanvas* gc) {
    int pointNo;
    for (pointNo = 0; pointNo < pointsCount - 1 && pointPositions[pointNo][0] <= minX; ++pointNo) {}
    if (pointNo > 0) --pointNo;
    while (pointNo < pointsCount - 1) {
        gc->drawLine(pointPositions[pointNo][0]   << 3 >> 16, pointPositions[pointNo][1]   << 3 >> 16,
                     pointPositions[pointNo+1][0] << 3 >> 16, pointPositions[pointNo+1][1] << 3 >> 16);
        if (startFlagPoint  == pointNo) { gc->renderStartFlag (pointPositions[startFlagPoint ][0]<<3>>16,pointPositions[startFlagPoint ][1]<<3>>16); gc->setColor(0,255,0); }
        if (finishFlagPoint == pointNo) { gc->renderFinishFlag(pointPositions[finishFlagPoint][0]<<3>>16,pointPositions[finishFlagPoint][1]<<3>>16); gc->setColor(0,255,0); }
        if (pointPositions[pointNo][0] > maxX) break;
        ++pointNo;
    }
}

void GameLevel::addPointSimple(int v1, int v2) {
    addPoint(v1 << 16 >> 3, v2 << 16 >> 3);
}

void GameLevel::addPoint(int x, int y) {
    if (pointPositions.empty() || (int)pointPositions.size() <= pointsCount) {
        int newSz = 100;
        if (!pointPositions.empty())
            newSz = (int)pointPositions.size() + 30 < 100 ? 100 : (int)pointPositions.size() + 30;
        pointPositions.resize(newSz, std::vector<int>(2));
    }
    if (pointsCount == 0 || pointPositions[pointsCount - 1][0] < x) {
        pointPositions[pointsCount][0] = x;
        pointPositions[pointsCount][1] = y;
        ++pointsCount;
    }
}

void GameLevel::load(FileStream* s) {
    init();
    int8_t c;
    s->readVariable(&c, true);
    if (c == 50) {
        char buf[20];
        s->readVariable(buf, false, 20);
    }
    finishFlagPoint = 0; startFlagPoint = 0;
    int pX, pY;
    short cnt;
    s->readVariable(&startPosX,  true);
    s->readVariable(&startPosY,  true);
    s->readVariable(&finishPosX, true);
    s->readVariable(&finishPosY, true);
    s->readVariable(&cnt,  true);
    s->readVariable(&pX, true);
    s->readVariable(&pY, true);
    int offX = pX, offY = pY;
    addPointSimple(pX, pY);

    for (int i = 1; i < cnt; ++i) {
        int8_t modeOrDx;
        s->readVariable(&modeOrDx, true);
        if (modeOrDx == -1) {
            offY = 0; offX = 0;
            s->readVariable(&pX, true);
            s->readVariable(&pY, true);
        } else {
            pX = modeOrDx;
            int8_t tmp;
            s->readVariable(&tmp, true);
            pY = tmp;
        }
        offX += pX; offY += pY;
        addPointSimple(offX, offY);
    }
}
