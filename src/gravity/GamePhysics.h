#pragma once
#include <vector>
#include <memory>
#include "TimerOrMotoPartOrMenuElem.h"

class LevelLoader;
class class_10;
class GameCanvas;

class GamePhysics {
    int index01 = 0, index10 = 1;
    int field_28 = -1;
    std::vector<std::unique_ptr<TimerOrMotoPartOrMenuElem>> field_30;
    int field_31 = 0;
    LevelLoader* levelLoader;
    int field_33 = 0, field_34 = 0;
    bool field_35 = false, field_36 = false;
    int field_37 = 32768;
    const int field_38 = 3276;
    int field_39 = 0;
    bool field_42 = false;
    bool field_68 = false;
    bool isEnableLookAhead = true;
    int  camShiftX = 0, camShiftY = 0;
    int  field_73 = 655360;
    std::vector<std::unique_ptr<TimerOrMotoPartOrMenuElem>> motoComponents
        = std::vector<std::unique_ptr<TimerOrMotoPartOrMenuElem>>(6);
    int field_44 = 0;
    bool isInputAcceleration = false;
    bool isInputBreak = false;
    bool isInputBack = false;
    bool isInputForward = false;

    const std::vector<std::vector<int>> hardcodedArr1 = {{183500,-52428},{262144,-163840},{406323,-65536},{445644,-39321},{235929,39321},{16384,-144179},{13107,-78643},{288358,81920}};
    const std::vector<std::vector<int>> hardcodedArr2 = {{190054,-111411},{308019,-235929},{334233,-114688},{393216,-58982},{262144,98304},{65536,-124518},{13107,-78643},{288358,81920}};
    const std::vector<std::vector<int>> hardcodedArr3 = {{157286,13107},{294912,-13107},{367001,91750},{406323,190054},{347340,72089},{39321,-98304},{13107,-52428},{294912,81920}};
    const std::vector<std::vector<int>> hardcodedArr4 = {{183500,-39321},{262144,-131072},{393216,-65536},{458752,-39321},{294912,6553},{16384,-144179},{13107,-78643},{288358,85196}};
    const std::vector<std::vector<int>> hardcodedArr5 = {{190054,-91750},{255590,-235929},{334233,-114688},{393216,-42598},{301465,6553},{65536,-78643},{13107,-78643},{288358,85196}};
    const std::vector<std::vector<int>> hardcodedArr6 = {{157286,13107},{294912,-13107},{367001,104857},{406323,176947},{347340,72089},{39321,-98304},{13107,-52428},{288358,85196}};
    std::vector<std::vector<int>> field_80;

    void method_27(int v1, int v2);
    void setInputFromAI();
    void method_35();
    int  method_39(int v1);
    void method_40(int v1);
    void method_42(class_10* v1, TimerOrMotoPartOrMenuElem* v2, class_10* v3, int v4, int v5);
    void method_43(int v1, int v2, int v3);
    void method_44(int v1, int v2, int v3);
    void method_45(int v1);
    int  method_46(int v1);
    void method_47(int v1);
    void renderEngine(GameCanvas* gc, int v2, int v3);
    void renderMotoFork(GameCanvas* gc);
    void renderWheelTires(GameCanvas* gc);
    void renderWheelSpokes(GameCanvas* gc);
    void renderSmth(GameCanvas* gc, int v2, int v3, int v4, int v5);
    void renderMotoAsLines(GameCanvas* gc, int v2, int v3, int v4, int v5);

public:
    inline static int field_7  = 0;
    inline static int field_8  = 0;
    inline static int field_9  = 0;
    inline static int field_10 = 0;
    inline static int field_11 = 0;
    inline static int motoParam1 = 0, motoParam2 = 0;
    inline static int field_14 = 0;
    inline static int motoParam10 = 0;
    inline static int field_16 = 0;
    inline static std::vector<int> const175_1_half = { 114688, 65536, 32768 };
    inline static int motoParam3  = 0;
    inline static int field_19 = 0;
    inline static int motoParam4  = 0;
    inline static int motoParam5  = 0;
    inline static int motoParam6  = 0;
    inline static int motoParam7  = 0;
    inline static int motoParam8  = 0;
    inline static int motoParam9  = 0;

    std::vector<std::unique_ptr<class_10>> field_29;
    bool field_41 = false;
    int  field_45 = 0;
    bool field_46 = false;
    bool isRenderMotoWithSprites = false;
    inline static int curentMotoLeague = 0;
    bool field_69 = false;
    bool isGenerateInputAI = false;

    bool isInputUp    = false;
    bool isInputDown  = false;
    bool isInputLeft  = false;
    bool isInputRight = false;

    explicit GamePhysics(LevelLoader* ll);
    ~GamePhysics();
    int  method_21();
    void method_22(int v1);
    void setMode(int mode);
    void setMotoLeague(int league);
    void resetSmth(bool unused);
    void method_26(bool v1);
    void setRenderMinMaxX(int minX, int maxX);
    void processPointerReleased();
    void method_30(int v1, int v2);
    void enableGenerateInputAI();
    void disableGenerateInputAI();
    int  updatePhysics();
    bool isTrackStarted();
    bool method_38();
    static int getSmthLikeMaxAbs(int xF16, int yF16);
    void setEnableLookAhead(bool v);
    void setMinimalScreenWH(int minWH);
    int  getCamPosX();
    int  getCamPosY();
    int  method_52();
    void method_53();
    void setMotoComponents();
    void renderGame(GameCanvas* gc);
};
