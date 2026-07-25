#pragma once
#include <vector>
#include <string>
#include <climits>
#include "GamePhysics.h"
#include "GameCanvas.h"
#include "GameLevel.h"
#include "TimerOrMotoPartOrMenuElem.h"
#include "GDFileStream.h"

class LevelLoader {
    std::vector<std::vector<int>> field_121;
    int field_123[3];
    int field_124[3];
    inline static std::vector<std::vector<int>> levelOffsetInFile
        = std::vector<std::vector<int>>(3);
    int field_132 = 0;
    static int field_133, field_134, field_135, field_136;
    FileStream* levelFileStream = nullptr;
    void loadLevels();

public:
    static const int field_114 = 0, field_115 = 1, field_116 = 2;
    static const int field_117 = 0, field_118 = 1;
    static bool isEnabledPerspective;
    static bool isEnabledShadows;
    GameLevel* gameLevel = nullptr;
    int field_125 = 0, field_126 = -1;
    std::vector<std::vector<std::string>> levelNames
        = std::vector<std::vector<std::string>>(3);
    int field_129 = 0, field_130 = 0, field_131 = 0;
    int field_137 = 0, field_138 = 0;

    explicit LevelLoader(const char* mrgFilePath);
    ~LevelLoader();

    std::string getName(int league, int level);
    void method_87();
    int  method_88(int v1, int v2);
    void method_89(int v1, int v2);
    void method_90(int v1);
    int  method_91();
    int  method_92();
    int  method_93();
    int  method_94();
    int  method_95(int v1);
    void method_96(GameLevel* gl);
    void setMinMaxX(int minX, int maxX);
    void renderLevel3D(GameCanvas* gc, int xF16, int yF16);
    void renderTrackNearestLine(GameCanvas* gc);
    void method_100(int v1, int v2, int v3);
    int  method_101(TimerOrMotoPartOrMenuElem* v1, int v2);
};
