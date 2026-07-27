#include "LevelLoader.h"
#include <algorithm>
#include <Arduino.h>

int LevelLoader::field_133 = 0;
int LevelLoader::field_134 = 0;
int LevelLoader::field_135 = 0;
int LevelLoader::field_136 = 0;
bool LevelLoader::isEnabledPerspective = true;
bool LevelLoader::isEnabledShadows = true;

LevelLoader::LevelLoader(const char* mrgFilePath) {
    for (int i = 0; i < 3; ++i) {
        int h = (GamePhysics::const175_1_half[i] + 19660) >> 1;
        int l = (GamePhysics::const175_1_half[i] - 19660) >> 1;
        field_123[i] = (int)((int64_t)h * (int64_t)h >> 16);
        field_124[i] = (int)((int64_t)l * (int64_t)l >> 16);
    }
    levelFileStream = new FileStream(mrgFilePath);
    if (!levelFileStream->isOpen()) {
        Serial.println("[GD] levels.mrg not found on SD root!");
        return;
    }
    loadLevels();
    method_87();
}

LevelLoader::~LevelLoader() {
    delete levelFileStream;
    delete gameLevel;
}

void LevelLoader::loadLevels() {
    std::vector<int8_t> buf(40);
    std::vector<int> counts(3);

    for (int league = 0; league < 3; ++league) {
        levelFileStream->readVariable(&counts[league], true);
        levelOffsetInFile[league] = std::vector<int>(counts[league]);
        levelNames[league]        = std::vector<std::string>(counts[league]);

        for (int lv = 0; lv < counts[league]; ++lv) {
            int off;
            levelFileStream->readVariable(&off, true);
            levelOffsetInFile[league][lv] = off;

            for (int k = 0; k < 40; ++k) {
                levelFileStream->readVariable(&buf[k], true);
                if (buf[k] == 0) {
                    std::string s(reinterpret_cast<char*>(buf.data()), k);
                    std::replace(s.begin(), s.end(), '_', ' ');
                    levelNames[league][lv] = s;
                    break;
                }
            }
        }
    }
}

std::string LevelLoader::getName(int league, int level) {
    return (league < 3 && level < (int)levelNames[league].size())
        ? levelNames[league][level] : "---";
}

void LevelLoader::method_87() { method_88(field_125, field_126 + 1); }

int LevelLoader::method_88(int v1, int v2) {
    field_125 = v1;
    field_126 = v2;
    if (field_126 >= (int)levelNames[field_125].size()) field_126 = 0;
    method_89(field_125 + 1, field_126 + 1);
    return field_126;
}

void LevelLoader::method_89(int v1, int v2) {
    levelFileStream->setPos(levelOffsetInFile[v1-1][v2-1]);
    if (!gameLevel) gameLevel = new GameLevel();
    gameLevel->load(levelFileStream);
    method_96(gameLevel);
}

void LevelLoader::method_90(int v1) {
    (void)v1;
    field_129 = gameLevel->startPosX << 1;
    field_130 = gameLevel->startPosY << 1;
}

int LevelLoader::method_91() { return gameLevel->pointPositions[gameLevel->finishFlagPoint][0] << 1; }
int LevelLoader::method_92() { return gameLevel->pointPositions[gameLevel->startFlagPoint][0] << 1; }
int LevelLoader::method_93() { return gameLevel->startPosX << 1; }
int LevelLoader::method_94() { return gameLevel->startPosY << 1; }
int LevelLoader::method_95(int v1) { return gameLevel->method_181(v1 >> 1); }

void LevelLoader::method_96(GameLevel* gl) {
    field_131 = INT_MIN;
    gameLevel = gl;
    int sz = gl->pointsCount;
    if (field_121.empty() || field_132 < sz) {
        field_132 = sz < 100 ? 100 : sz;
        field_121.assign(field_132, std::vector<int>(2));
    }
    field_133 = field_134 = 0;
    field_135 = gl->pointPositions[0][0];
    field_136 = gl->pointPositions[0][0];

    for (int i = 0; i < sz; ++i) {
        int dx = gl->pointPositions[(i+1)%sz][0] - gl->pointPositions[i][0];
        int dy = gl->pointPositions[(i+1)%sz][1] - gl->pointPositions[i][1];
        if (i != 0 && i != sz-1)
            field_131 = field_131 < gl->pointPositions[i][0] ? gl->pointPositions[i][0] : field_131;
        int nx = -dy;
        int nb = GamePhysics::getSmthLikeMaxAbs(nx, dx);
        if (nb == 0) continue;
        field_121[i][0] = (int)(((int64_t)nx << 32) / (int64_t)nb >> 16);
        field_121[i][1] = (int)(((int64_t)dx << 32) / (int64_t)nb >> 16);
        if (gl->startFlagPoint  == 0 && gl->pointPositions[i][0] > gl->startPosX)
            gl->startFlagPoint  = i + 1;
        if (gl->finishFlagPoint == 0 && gl->pointPositions[i][0] > gl->finishPosX)
            gl->finishFlagPoint = i;
    }
    field_133 = field_134 = field_135 = field_136 = 0;
}

void LevelLoader::setMinMaxX(int mn, int mx) { gameLevel->setMinMaxX(mn, mx); }

void LevelLoader::renderLevel3D(GameCanvas* gc, int xF16, int yF16) {
    if (!gc) return;
    gc->setColor(0, 170, 0);
    gameLevel->renderLevel3D(gc, xF16 >> 1, yF16 >> 1);
}

void LevelLoader::renderTrackNearestLine(GameCanvas* gc) {
    gc->setColor(0, 255, 0);
    gameLevel->renderTrackNearestGreenLine(gc);
}

void LevelLoader::method_100(int v1, int v2, int v3) {
    gameLevel->method_184((v1 + 98304) >> 1, (v2 - 98304) >> 1, v3 >> 1);
    v2 >>= 1; v1 >>= 1;
    field_134 = field_134 < gameLevel->pointsCount - 1 ? field_134 : gameLevel->pointsCount - 1;
    field_133 = field_133 < 0 ? 0 : field_133;
    if (v2 > field_136) {
        while (field_134 < gameLevel->pointsCount-1 && v2 > gameLevel->pointPositions[++field_134][0]) {}
    } else if (v1 < field_135) {
        while (field_133 > 0 && v1 < gameLevel->pointPositions[--field_133][0]) {}
    } else {
        while (field_133 + 1 < gameLevel->pointsCount && v1 > gameLevel->pointPositions[field_133 + 1][0])
            ++field_133;
        if (field_133 > 0) --field_133;
        while (field_134 > 0 && v2 < gameLevel->pointPositions[--field_134][0]) {}
        field_134 = field_134+1 < gameLevel->pointsCount-1 ? field_134+1 : gameLevel->pointsCount-1;
    }
    field_135 = gameLevel->pointPositions[field_133][0];
    field_136 = gameLevel->pointPositions[field_134][0];
}

int LevelLoader::method_101(TimerOrMotoPartOrMenuElem* v1, int v2) {
    int cnt = 0;
    int8_t result = 2;
    int px = v1->xF16 >> 1;
    int py = v1->yF16 >> 1;
    if (isEnabledPerspective) py -= 65536;
    int nx = 0, ny = 0;

    for (int i = field_133; i < field_134; ++i) {
        int x0 = gameLevel->pointPositions[i][0];
        int y0 = gameLevel->pointPositions[i][1];
        int x1 = gameLevel->pointPositions[i+1][0];
        int y1 = gameLevel->pointPositions[i+1][1];

        if (px - field_123[v2] <= x1 && px + field_123[v2] >= x0) {
            int dx = x0 - x1, dy = y0 - y1;
            int dlen = (int)((int64_t)dx*(int64_t)dx >> 16) + (int)((int64_t)dy*(int64_t)dy >> 16);
            int dot  = (int)((int64_t)(px-x0)*(int64_t)(-dx) >> 16) + (int)((int64_t)(py-y0)*(int64_t)(-dy) >> 16);
            int t;
            if ((dlen < 0 ? -dlen : dlen) >= 3)
                t = (int)(((int64_t)dot << 32) / (int64_t)dlen >> 16);
            else
                t = (dot > 0 ? 1 : -1) * (dlen > 0 ? 1 : -1) * INT_MAX;
            if (t < 0)      t = 0;
            if (t > 65536)  t = 65536;
            int cx = x0 + (int)((int64_t)t * (int64_t)(-dx) >> 16);
            int cy = y0 + (int)((int64_t)t * (int64_t)(-dy) >> 16);
            int ex = px - cx, ey = py - cy;
            int8_t r;
            int64_t dist = ((int64_t)ex*(int64_t)ex >> 16) + ((int64_t)ey*(int64_t)ey >> 16);
            if      (dist < (int64_t)field_123[v2]) r = (dist >= (int64_t)field_124[v2]) ? 1 : 0;
            else                                    r = 2;

            int vel_dot = (int)((int64_t)field_121[i][0] * (int64_t)v1->field_382 >> 16)
                        + (int)((int64_t)field_121[i][1] * (int64_t)v1->field_383 >> 16);
            if (r == 0 && vel_dot < 0) {
                field_137 = field_121[i][0];
                field_138 = field_121[i][1];
                return 0;
            }
            if (r == 1 && vel_dot < 0) {
                ++cnt; result = 1;
                if (cnt == 1) { nx = field_121[i][0]; ny = field_121[i][1]; }
                else          { nx += field_121[i][0]; ny += field_121[i][1]; }
            }
        }
    }

    if (result == 1) {
        int vd = (int)((int64_t)nx*(int64_t)v1->field_382 >> 16)
               + (int)((int64_t)ny*(int64_t)v1->field_383 >> 16);
        if (vd >= 0) return 2;
        field_137 = nx; field_138 = ny;
    }
    return result;
}
