#include "GamePhysics.h"
#include "GameCanvas.h"
#include "LevelLoader.h"
#include "class_10.h"
#include "MathF16.h"
#include "GDMicro.h"
#include <algorithm>

GamePhysics::~GamePhysics() = default;

GamePhysics::GamePhysics(LevelLoader* ll) {
    for (int i = 0; i < 6; ++i)
        motoComponents[i] = std::make_unique<TimerOrMotoPartOrMenuElem>();
    field_44 = field_45 = 0;
    field_46 = isRenderMotoWithSprites = false;
    isInputAcceleration = isInputBreak = isInputBack = isInputForward = false;
    isInputUp = isInputDown = isInputLeft = isInputRight = false;
    field_68 = field_69 = false;
    isEnableLookAhead = true;
    camShiftX = camShiftY = 0;
    field_73 = 655360;
    field_80 = { {45875}, {32768}, {52428} };
    levelLoader = ll;
    resetSmth(true);
    isGenerateInputAI = false;
    method_53();
    field_35 = false;
}

int GamePhysics::method_21() {
    if (field_46 && isRenderMotoWithSprites) return 3;
    if (isRenderMotoWithSprites) return 1;
    return field_46 ? 2 : 0;
}

void GamePhysics::method_22(int v1) {
    field_46 = false; isRenderMotoWithSprites = false;
    if (v1 & 2) field_46 = true;
    if (v1 & 1) isRenderMotoWithSprites = true;
}

void GamePhysics::setMode(int mode) {
    field_45 = mode;
    switch (mode) {
    case 1: default:
        field_7 = 1310; field_8 = 1638400;
        setMotoLeague(1); resetSmth(true);
    }
}

void GamePhysics::setMotoLeague(int league) {
    curentMotoLeague = league;
    field_9 = 45875; field_10 = 13107; field_11 = 39321;
    field_14 = 1310720; field_16 = 262144; field_19 = 6553;
    switch (league) {
    case 0: default:
        motoParam1=19660; motoParam2=19660; motoParam3=1114112; motoParam4=52428800;
        motoParam5=3276800; motoParam6=327; motoParam7=0; motoParam8=32768;
        motoParam9=327680; motoParam10=19660800; break;
    case 1:
        motoParam1=32768; motoParam2=32768; motoParam3=1114112; motoParam4=65536000;
        motoParam5=3276800; motoParam6=6553; motoParam7=26214; motoParam8=26214;
        motoParam9=327680; motoParam10=19660800; break;
    case 2:
        motoParam1=32768; motoParam2=32768; motoParam3=1310720; motoParam4=75366400;
        motoParam5=3473408; motoParam6=6553; motoParam7=26214; motoParam8=39321;
        motoParam9=327680; motoParam10=21626880; break;
    case 3:
        motoParam1=32768; motoParam2=32768; motoParam3=1441792; motoParam4=78643200;
        motoParam5=3538944; motoParam6=6553; motoParam7=26214; motoParam8=65536;
        motoParam9=1310720; motoParam10=21626880;
    }
    resetSmth(true);
}

void GamePhysics::resetSmth(bool /*unused*/) {
    field_44 = 0;
    method_27(levelLoader->method_93(), levelLoader->method_94());
    field_31 = 0; field_39 = 0; field_35 = false; field_36 = false;
    field_68 = false; field_69 = false; isGenerateInputAI = false;
    field_41 = false; field_42 = false;
    levelLoader->gameLevel->method_183(
        field_29[2]->motoComponents[5]->xF16 + 98304 - const175_1_half[0],
        field_29[1]->motoComponents[5]->xF16 - 98304 + const175_1_half[0]);
}

void GamePhysics::method_26(bool v1) {
    int d = (v1 ? 65536 : -65536) << 1;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            field_29[i]->motoComponents[j]->yF16 += d;
}

void GamePhysics::method_27(int v1, int v2) {
    if (field_29.empty()) field_29 = std::vector<std::unique_ptr<class_10>>(6);
    if (field_30.empty()) field_30 = std::vector<std::unique_ptr<TimerOrMotoPartOrMenuElem>>(10);

    for (int i = 0; i < 6; ++i) {
        int8_t v5 = 0; int v4 = 0, v6 = 0, v7 = 0; short v8 = 0;
        switch (i) {
        case 0: v5=1; v4=360448; v6=0; v7=0; break;
        case 1: v5=0; v4=98304;  v6=229376; v7=0; break;
        case 2: v5=0; v4=360448; v6=-229376; v7=0; v8=21626; break;
        case 3: v5=1; v4=229376; v6=131072; v7=196608; break;
        case 4: v5=1; v4=229376; v6=-131072; v7=196608; break;
        case 5: v5=2; v4=294912; v6=0; v7=327680; break;
        }
        if (!field_29[i]) field_29[i] = std::make_unique<class_10>();
        field_29[i]->reset();
        field_29[i]->field_257 = const175_1_half[v5];
        field_29[i]->field_258 = v5;
        field_29[i]->field_259 = (int)((int64_t)((int)(281474976710656L/(int64_t)v4>>16))*(int64_t)field_14>>16);
        field_29[i]->motoComponents[index01]->xF16 = v1 + v6;
        field_29[i]->motoComponents[index01]->yF16 = v2 + v7;
        field_29[i]->motoComponents[5]->xF16 = v1 + v6;
        field_29[i]->motoComponents[5]->yF16 = v2 + v7;
        field_29[i]->field_260 = v8;
    }
    for (int i = 0; i < 10; ++i) {
        if (!field_30[i]) field_30[i] = std::make_unique<TimerOrMotoPartOrMenuElem>();
        field_30[i]->setToZeros();
        field_30[i]->xF16 = motoParam10;
        field_30[i]->angleF16 = field_16;
    }
    field_30[0]->yF16=229376; field_30[1]->yF16=229376; field_30[2]->yF16=236293;
    field_30[3]->yF16=236293; field_30[4]->yF16=262144; field_30[5]->yF16=219814;
    field_30[6]->yF16=219814; field_30[7]->yF16=185363; field_30[8]->yF16=185363;
    field_30[9]->yF16=327680;
    field_30[5]->angleF16=(int)((int64_t)field_16*45875L>>16);
    field_30[6]->xF16=(int)(6553L*(int64_t)motoParam10>>16);
    field_30[5]->xF16=(int)(6553L*(int64_t)motoParam10>>16);
    field_30[9]->xF16=(int)(72089L*(int64_t)motoParam10>>16);
    field_30[8]->xF16=(int)(72089L*(int64_t)motoParam10>>16);
    field_30[7]->xF16=(int)(72089L*(int64_t)motoParam10>>16);
}

void GamePhysics::setRenderMinMaxX(int mn, int mx) { levelLoader->setMinMaxX(mn, mx); }
void GamePhysics::processPointerReleased() { isInputUp=isInputDown=isInputRight=isInputLeft=false; }

void GamePhysics::method_30(int v1, int v2) {
    if (!isGenerateInputAI) {
        isInputUp=isInputDown=isInputRight=isInputLeft=false;
        if (v1>0) isInputUp=true; else if (v1<0) isInputDown=true;
        if (v2>0) isInputRight=true;
        if (v2<0) isInputLeft=true;
    }
}

void GamePhysics::enableGenerateInputAI()  { resetSmth(true); isGenerateInputAI=true; }
void GamePhysics::disableGenerateInputAI() { isGenerateInputAI=false; }

void GamePhysics::setInputFromAI() {
    int v1=field_29[1]->motoComponents[index01]->xF16 - field_29[2]->motoComponents[index01]->xF16;
    int v2=field_29[1]->motoComponents[index01]->yF16 - field_29[2]->motoComponents[index01]->yF16;
    int v3=getSmthLikeMaxAbs(v1,v2);
    if (v3==0) return;
    v2=(int)(((int64_t)v2<<32)/(int64_t)v3>>16);
    isInputBreak=false;
    if (v2<0) { isInputBack=true; isInputForward=false; }
    else if (v2>0) { isInputForward=true; isInputBack=false; }
    bool v4;
    if ((!(v4=(field_29[2]->motoComponents[index01]->yF16-field_29[0]->motoComponents[index01]->yF16>0?1:-1)*
           (field_29[2]->motoComponents[index01]->field_382-field_29[0]->motoComponents[index01]->field_382>0?1:-1)>0)
         || !isInputForward) && (v4 || !isInputBack))
        isInputAcceleration=false;
    else
        isInputAcceleration=true;
}

void GamePhysics::method_35() {
    if (field_35) return;
    int v1=field_29[1]->motoComponents[index01]->xF16 - field_29[2]->motoComponents[index01]->xF16;
    int v2=field_29[1]->motoComponents[index01]->yF16 - field_29[2]->motoComponents[index01]->yF16;
    int v3=getSmthLikeMaxAbs(v1,v2);
    if (v3==0) return;
    v1=(int)(((int64_t)v1<<32)/(int64_t)v3>>16);
    v2=(int)(((int64_t)v2<<32)/(int64_t)v3>>16);
    if (isInputAcceleration && field_31>=-motoParam4) field_31-=motoParam5;
    if (isInputBreak) {
        field_31=0;
        field_29[1]->motoComponents[index01]->field_384=(int)((int64_t)field_29[1]->motoComponents[index01]->field_384*(int64_t)(65536-motoParam6)>>16);
        field_29[2]->motoComponents[index01]->field_384=(int)((int64_t)field_29[2]->motoComponents[index01]->field_384*(int64_t)(65536-motoParam6)>>16);
        if (field_29[1]->motoComponents[index01]->field_384<6553) field_29[1]->motoComponents[index01]->field_384=0;
        if (field_29[2]->motoComponents[index01]->field_384<6553) field_29[2]->motoComponents[index01]->field_384=0;
    }
    field_29[0]->field_259=(int)(11915L*(int64_t)field_14>>16);
    field_29[4]->field_259=(int)(18724L*(int64_t)field_14>>16);
    field_29[3]->field_259=(int)(18724L*(int64_t)field_14>>16);
    field_29[1]->field_259=(int)(43690L*(int64_t)field_14>>16);
    field_29[2]->field_259=(int)(11915L*(int64_t)field_14>>16);
    field_29[5]->field_259=(int)(14563L*(int64_t)field_14>>16);
    if (isInputBack) {
        field_29[0]->field_259=(int)(18724L*(int64_t)field_14>>16);
        field_29[4]->field_259=(int)(14563L*(int64_t)field_14>>16);
        field_29[3]->field_259=(int)(18724L*(int64_t)field_14>>16);
        field_29[1]->field_259=(int)(43690L*(int64_t)field_14>>16);
        field_29[2]->field_259=(int)(10082L*(int64_t)field_14>>16);
    } else if (isInputForward) {
        field_29[0]->field_259=(int)(18724L*(int64_t)field_14>>16);
        field_29[4]->field_259=(int)(18724L*(int64_t)field_14>>16);
        field_29[3]->field_259=(int)(14563L*(int64_t)field_14>>16);
        field_29[1]->field_259=(int)(26214L*(int64_t)field_14>>16);
        field_29[2]->field_259=(int)(11915L*(int64_t)field_14>>16);
    }
    if (isInputBack || isInputForward) {
        int v4=-v2;
        TimerOrMotoPartOrMenuElem* p;
        int v6,v7,v8,v9,v10,v11;
        if (isInputBack && field_39>-motoParam9) {
            v6=65536;
            if (field_39<0) v6=(int)(((int64_t)(motoParam9-(field_39<0?-field_39:field_39))<<32)/(int64_t)motoParam9>>16);
            v7=(int)((int64_t)motoParam8*(int64_t)v6>>16);
            v8=(int)((int64_t)v4*(int64_t)v7>>16); v9=(int)((int64_t)v1*(int64_t)v7>>16);
            v10=(int)((int64_t)v1*(int64_t)v7>>16); v11=(int)((int64_t)v2*(int64_t)v7>>16);
            if (field_37>32768) field_37=field_37-1638<0?0:field_37-1638;
            else field_37=field_37-3276<0?0:field_37-3276;
            (p=field_29[4]->motoComponents[index01].get())->field_382-=v8; p->field_383-=v9;
            (p=field_29[3]->motoComponents[index01].get())->field_382+=v8; p->field_383+=v9;
            (p=field_29[5]->motoComponents[index01].get())->field_382-=v10; p->field_383-=v11;
        }
        if (isInputForward && field_39<motoParam9) {
            v6=65536;
            if (field_39>0) v6=(int)(((int64_t)(motoParam9-field_39)<<32)/(int64_t)motoParam9>>16);
            v7=(int)((int64_t)motoParam8*(int64_t)v6>>16);
            v8=(int)((int64_t)v4*(int64_t)v7>>16); v9=(int)((int64_t)v1*(int64_t)v7>>16);
            v10=(int)((int64_t)v1*(int64_t)v7>>16); v11=(int)((int64_t)v2*(int64_t)v7>>16);
            if (field_37>32768) field_37=field_37+1638<65536?field_37+1638:65536;
            else field_37=field_37+3276<65536?field_37+3276:65536;
            (p=field_29[4]->motoComponents[index01].get())->field_382+=v8; p->field_383+=v9;
            (p=field_29[3]->motoComponents[index01].get())->field_382-=v8; p->field_383-=v9;
            (p=field_29[5]->motoComponents[index01].get())->field_382+=v10; p->field_383+=v11;
        }
        return;
    }
    if (field_37<26214) { field_37+=3276; return; }
    if (field_37>39321) { field_37-=3276; return; }
    field_37=32768;
}

int GamePhysics::updatePhysics() {
    isInputAcceleration=isInputUp;
    isInputBreak=isInputDown;
    isInputBack=isInputLeft;
    isInputForward=isInputRight;
    if (isGenerateInputAI) setInputFromAI();
    GameCanvas::method_151();
    method_35();
    int v1;
    if ((v1=method_39(field_7))!=5 && !field_36) {
        if (field_35) return 3;
        else if (isTrackStarted()) { field_69=false; return 4; }
        else return v1;
    }
    return 5;
}

bool GamePhysics::isTrackStarted() {
    return field_29[1]->motoComponents[index01]->xF16 < levelLoader->method_92();
}
bool GamePhysics::method_38() {
    return field_29[1]->motoComponents[index10]->xF16 > levelLoader->method_91()
        || field_29[2]->motoComponents[index10]->xF16 > levelLoader->method_91();
}

int GamePhysics::method_39(int v1) {
    bool v2=field_68; int v3=0, v4=v1;
label77:
    do {
        int v5;
        while (v3<v1) {
            method_45(v4-v3);
            if (!v2 && method_38()) v5=3;
            else v5=method_46(index10);
            if (!v2 && field_68) { return (v5!=3)?2:1; }
            if (v5==0) { v4=(v3+v4)>>1; goto label77; }
            if (v5==3) { field_68=true; v4=(v3+v4)>>1; }
            else {
                if (v5==1) { int v6; do { method_47(index10); if ((v6=method_46(index10))==0) return 5; } while (v6!=2); }
                v3=v4; v4=v1; index01=index01==1?0:1; index10=index10==1?0:1;
            }
        }
        int v5sq=(int)((int64_t)(field_29[1]->motoComponents[index01]->xF16-field_29[2]->motoComponents[index01]->xF16)*(int64_t)(field_29[1]->motoComponents[index01]->xF16-field_29[2]->motoComponents[index01]->xF16)>>16)
               +(int)((int64_t)(field_29[1]->motoComponents[index01]->yF16-field_29[2]->motoComponents[index01]->yF16)*(int64_t)(field_29[1]->motoComponents[index01]->yF16-field_29[2]->motoComponents[index01]->yF16)>>16);
        if (v5sq<983040) field_35=true;
        if (v5sq>4587520) field_35=true;
        return 0;
    } while (((v4=(v3+v4)>>1)-v3<0?-(v4-v3):v4-v3)>=65);
    return 5;
}

void GamePhysics::method_40(int v1) {
    for (int i=0;i<6;++i) {
        class_10* b=field_29[i].get();
        TimerOrMotoPartOrMenuElem* p=b->motoComponents[v1].get();
        p->field_385=0; p->field_386=0; p->field_387=0;
        p->field_386-=(int)(((int64_t)field_8<<32)/(int64_t)b->field_259>>16);
    }
    if (!field_35) {
        method_42(field_29[0].get(),field_30[1].get(),field_29[2].get(),v1,65536);
        method_42(field_29[0].get(),field_30[0].get(),field_29[1].get(),v1,65536);
        method_42(field_29[2].get(),field_30[6].get(),field_29[4].get(),v1,131072);
        method_42(field_29[1].get(),field_30[5].get(),field_29[3].get(),v1,131072);
    }
    method_42(field_29[0].get(),field_30[2].get(),field_29[3].get(),v1,65536);
    method_42(field_29[0].get(),field_30[3].get(),field_29[4].get(),v1,65536);
    method_42(field_29[3].get(),field_30[4].get(),field_29[4].get(),v1,65536);
    method_42(field_29[5].get(),field_30[8].get(),field_29[3].get(),v1,65536);
    method_42(field_29[5].get(),field_30[7].get(),field_29[4].get(),v1,65536);
    method_42(field_29[5].get(),field_30[9].get(),field_29[0].get(),v1,65536);
    TimerOrMotoPartOrMenuElem* p=field_29[2]->motoComponents[v1].get();
    field_31=(int)((int64_t)field_31*(int64_t)(65536-field_19)>>16);
    p->field_387=field_31;
    if (p->field_384>motoParam3) p->field_384=motoParam3;
    if (p->field_384<-motoParam3) p->field_384=-motoParam3;
    int sx=0,sy=0;
    for (int i=0;i<6;++i) { sx+=field_29[i]->motoComponents[v1]->field_382; sy+=field_29[i]->motoComponents[v1]->field_383; }
    sx=(int)(((int64_t)sx<<32)/393216L>>16); sy=(int)(((int64_t)sy<<32)/393216L>>16);
    int v10=0;
    for (int i=0;i<6;++i) {
        int dx=field_29[i]->motoComponents[v1]->field_382-sx;
        int dy=field_29[i]->motoComponents[v1]->field_383-sy;
        if ((v10=getSmthLikeMaxAbs(dx,dy))>1966080) {
            int ex=(int)(((int64_t)dx<<32)/(int64_t)v10>>16);
            int ey=(int)(((int64_t)dy<<32)/(int64_t)v10>>16);
            field_29[i]->motoComponents[v1]->field_382-=ex;
            field_29[i]->motoComponents[v1]->field_383-=ey;
        }
    }
    int s1=field_29[2]->motoComponents[v1]->yF16-field_29[0]->motoComponents[v1]->yF16>=0?1:-1;
    int s2=field_29[2]->motoComponents[v1]->field_382-field_29[0]->motoComponents[v1]->field_382>=0?1:-1;
    field_39=(s1*s2>0)?v10:-v10;
}

int GamePhysics::getSmthLikeMaxAbs(int x, int y) {
    int ax=x<0?-x:x, ay=y<0?-y:y;
    int mx=ax>=ay?ax:ay, mn=ax>=ay?ay:ax;
    return (int)(64448L*(int64_t)mx>>16)+(int)(28224L*(int64_t)mn>>16);
}

void GamePhysics::method_42(class_10* a, TimerOrMotoPartOrMenuElem* sp, class_10* b, int v4, int v5) {
    TimerOrMotoPartOrMenuElem* pa=a->motoComponents[v4].get();
    TimerOrMotoPartOrMenuElem* pb=b->motoComponents[v4].get();
    int dx=pa->xF16-pb->xF16, dy=pa->yF16-pb->yF16;
    int len; if (((len=getSmthLikeMaxAbs(dx,dy))<0?-len:len)<3) return;
    dx=(int)(((int64_t)dx<<32)/(int64_t)len>>16);
    dy=(int)(((int64_t)dy<<32)/(int64_t)len>>16);
    int dl=len-sp->yF16;
    int f1=(int)((int64_t)dx*(int64_t)((int)((int64_t)dl*(int64_t)sp->xF16>>16))>>16);
    int f2=(int)((int64_t)dy*(int64_t)((int)((int64_t)dl*(int64_t)sp->xF16>>16))>>16);
    int rvx=pa->field_382-pb->field_382, rvy=pa->field_383-pb->field_383;
    int imp=(int)((int64_t)((int)((int64_t)dx*(int64_t)rvx>>16)+(int)((int64_t)dy*(int64_t)rvy>>16))*(int64_t)sp->angleF16>>16);
    f1+=(int)((int64_t)dx*(int64_t)imp>>16); f2+=(int)((int64_t)dy*(int64_t)imp>>16);
    f1=(int)((int64_t)f1*(int64_t)v5>>16); f2=(int)((int64_t)f2*(int64_t)v5>>16);
    pa->field_385-=f1; pa->field_386-=f2; pb->field_385+=f1; pb->field_386+=f2;
}

void GamePhysics::method_43(int v1, int v2, int v3) {
    for (int i=0;i<6;++i) {
        TimerOrMotoPartOrMenuElem* s=field_29[i]->motoComponents[v1].get();
        TimerOrMotoPartOrMenuElem* d; (d=field_29[i]->motoComponents[v2].get())->xF16=(int)((int64_t)s->field_382*(int64_t)v3>>16);
        d->yF16=(int)((int64_t)s->field_383*(int64_t)v3>>16);
        int inv=(int)((int64_t)v3*(int64_t)field_29[i]->field_259>>16);
        d->field_382=(int)((int64_t)s->field_385*(int64_t)inv>>16);
        d->field_383=(int)((int64_t)s->field_386*(int64_t)inv>>16);
    }
}

void GamePhysics::method_44(int v1, int v2, int v3) {
    for (int i=0;i<6;++i) {
        TimerOrMotoPartOrMenuElem* d=field_29[i]->motoComponents[v1].get();
        TimerOrMotoPartOrMenuElem* a=field_29[i]->motoComponents[v2].get();
        TimerOrMotoPartOrMenuElem* b=field_29[i]->motoComponents[v3].get();
        d->xF16=a->xF16+(b->xF16>>1); d->yF16=a->yF16+(b->yF16>>1);
        d->field_382=a->field_382+(b->field_382>>1); d->field_383=a->field_383+(b->field_383>>1);
    }
}

void GamePhysics::method_45(int v1) {
    method_40(index01); method_43(index01,2,v1);
    method_44(4,index01,2); method_40(4); method_43(4,3,v1>>1);
    method_44(4,index01,3); method_44(index10,index01,2); method_44(index10,index10,3);
    for (int i=1;i<=2;++i) {
        TimerOrMotoPartOrMenuElem* s=field_29[i]->motoComponents[index01].get();
        TimerOrMotoPartOrMenuElem* d; (d=field_29[i]->motoComponents[index10].get())->angleF16=s->angleF16+(int)((int64_t)v1*(int64_t)s->field_384>>16);
        d->field_384=s->field_384+(int)((int64_t)v1*(int64_t)((int)((int64_t)field_29[i]->field_260*(int64_t)s->field_387>>16))>>16);
    }
}

int GamePhysics::method_46(int v1) {
    int8_t r=2;
    int xmx=std::max({field_29[1]->motoComponents[v1]->xF16,field_29[2]->motoComponents[v1]->xF16,field_29[5]->motoComponents[v1]->xF16});
    int xmn=std::min({field_29[1]->motoComponents[v1]->xF16,field_29[2]->motoComponents[v1]->xF16,field_29[5]->motoComponents[v1]->xF16});
    levelLoader->method_100(xmn-const175_1_half[0],xmx+const175_1_half[0],field_29[5]->motoComponents[v1]->yF16);
    int dx=field_29[1]->motoComponents[v1]->xF16-field_29[2]->motoComponents[v1]->xF16;
    int dy=field_29[1]->motoComponents[v1]->yF16-field_29[2]->motoComponents[v1]->yF16;
    int len=getSmthLikeMaxAbs(dx,dy);
    if (len==0) return 2;
    dx=(int)(((int64_t)dx<<32)/(int64_t)len>>16);
    int ny=-((int)(((int64_t)dy<<32)/(int64_t)len>>16));
    int nx=dx;
    for (int i=0;i<6;++i) {
        if (i==4||i==3) continue;
        TimerOrMotoPartOrMenuElem* p=field_29[i]->motoComponents[v1].get();
        if (i==0) { p->xF16+=(int)((int64_t)ny*65536L>>16); p->yF16+=(int)((int64_t)nx*65536L>>16); }
        int res=levelLoader->method_101(p,field_29[i]->field_258);
        if (i==0) { p->xF16-=(int)((int64_t)ny*65536L>>16); p->yF16-=(int)((int64_t)nx*65536L>>16); }
        field_33=levelLoader->field_137; field_34=levelLoader->field_138;
        if (i==5 && res!=2) field_36=true;
        if (i==1 && res!=2) field_69=true;
        if (res==1) { field_28=i; r=1; }
        else if (res==0) { field_28=i; r=0; break; }
    }
    return r;
}

void GamePhysics::method_47(int v1) {
    class_10* b=field_29[field_28].get();
    TimerOrMotoPartOrMenuElem* p=b->motoComponents[v1].get();
    p->xF16+=(int)((int64_t)field_33*3276L>>16);
    p->yF16+=(int)((int64_t)field_34*3276L>>16);
    int v4,v5,v6,v7,v8;
    if (isInputBreak && (field_28==2||field_28==1) && p->field_384<6553) {
        v4=field_9-motoParam7; v5=13107; v6=39321; v7=26214-motoParam7; v8=26214-motoParam7;
    } else { v4=field_9; v5=field_10; v6=field_11; v7=motoParam1; v8=motoParam2; }
    int nl=getSmthLikeMaxAbs(field_33,field_34);
    if (nl==0) return;
    int ux=(int)(((int64_t)field_33<<32)/(int64_t)nl>>16);
    int uy=(int)(((int64_t)field_34<<32)/(int64_t)nl>>16);
    int vx=p->field_382, vy=p->field_383;
    int vn=-((int)((int64_t)vx*(int64_t)ux>>16)+(int)((int64_t)vy*(int64_t)uy>>16));
    int vt=-((int)((int64_t)vx*(int64_t)(-uy)>>16)+(int)((int64_t)vy*(int64_t)ux>>16));
    int wn=(int)((int64_t)v4*(int64_t)p->field_384>>16)-(int)((int64_t)v5*(int64_t)((int)(((int64_t)vt<<32)/(int64_t)b->field_257>>16))>>16);
    int wt=(int)((int64_t)v7*(int64_t)vt>>16)-(int)((int64_t)v6*(int64_t)((int)((int64_t)p->field_384*(int64_t)b->field_257>>16))>>16);
    int wf=-((int)((int64_t)v8*(int64_t)vn>>16));
    p->field_384=wn;
    p->field_382=(int)((int64_t)(-wt)*(int64_t)(-uy)>>16)+(int)((int64_t)(-wf)*(int64_t)ux>>16);
    p->field_383=(int)((int64_t)(-wt)*(int64_t)ux>>16)+(int)((int64_t)(-wf)*(int64_t)uy>>16);
}

void GamePhysics::setEnableLookAhead(bool v) { isEnableLookAhead=v; }
void GamePhysics::setMinimalScreenWH(int w) {
    field_73=(int)(((int64_t)((int)(655360L*(int64_t)(w<<16)>>16))<<32)/8388608L>>16);
}

int GamePhysics::getCamPosX() {
    if (isEnableLookAhead) camShiftX=(int)(((int64_t)motoComponents[0]->field_382<<32)/1572864L>>16)+(int)((int64_t)camShiftX*57344L>>16);
    else camShiftX=0;
    camShiftX=camShiftX<field_73?camShiftX:field_73;
    camShiftX=camShiftX<-field_73?-field_73:camShiftX;
    return (motoComponents[0]->xF16+camShiftX)<<2>>16;
}

int GamePhysics::getCamPosY() {
    if (isEnableLookAhead) camShiftY=(int)(((int64_t)motoComponents[0]->field_383<<32)/1572864L>>16)+(int)((int64_t)camShiftY*57344L>>16);
    else camShiftY=0;
    camShiftY=camShiftY<field_73?camShiftY:field_73;
    camShiftY=camShiftY<-field_73?-field_73:camShiftY;
    return (motoComponents[0]->yF16+camShiftY)<<2>>16;
}

int GamePhysics::method_52() {
    int xm=motoComponents[1]->xF16<motoComponents[2]->xF16?motoComponents[2]->xF16:motoComponents[1]->xF16;
    return field_35?levelLoader->method_95(motoComponents[0]->xF16):levelLoader->method_95(xm);
}

void GamePhysics::method_53() {
    for (int i=0;i<6;++i) {
        field_29[i]->motoComponents[5]->xF16=field_29[i]->motoComponents[index01]->xF16;
        field_29[i]->motoComponents[5]->yF16=field_29[i]->motoComponents[index01]->yF16;
        field_29[i]->motoComponents[5]->angleF16=field_29[i]->motoComponents[index01]->angleF16;
    }
    field_29[0]->motoComponents[5]->field_382=field_29[0]->motoComponents[index01]->field_382;
    field_29[0]->motoComponents[5]->field_383=field_29[0]->motoComponents[index01]->field_383;
    field_29[2]->motoComponents[5]->field_384=field_29[2]->motoComponents[index01]->field_384;
}

void GamePhysics::setMotoComponents() {
    for (int i=0;i<6;++i) {
        motoComponents[i]->xF16=field_29[i]->motoComponents[5]->xF16;
        motoComponents[i]->yF16=field_29[i]->motoComponents[5]->yF16;
        motoComponents[i]->angleF16=field_29[i]->motoComponents[5]->angleF16;
    }
    motoComponents[0]->field_382=field_29[0]->motoComponents[5]->field_382;
    motoComponents[0]->field_383=field_29[0]->motoComponents[5]->field_383;
    motoComponents[2]->field_384=field_29[2]->motoComponents[5]->field_384;
}

void GamePhysics::renderEngine(GameCanvas* gc, int v2, int v3) {
    // sprite rendering — skip (isRenderMotoWithSprites = false)
}

void GamePhysics::renderMotoFork(GameCanvas* gc) {
    gc->setColor(128,128,128);
    gc->drawLineF16(motoComponents[3]->xF16,motoComponents[3]->yF16,motoComponents[1]->xF16,motoComponents[1]->yF16);
}

void GamePhysics::renderWheelTires(GameCanvas* gc) {
    int r_px = (field_29[1]->field_257 << 2 >> 16) + 1;  // tire outer radius in pixels

    int f_x = motoComponents[1]->xF16 << 2 >> 16;
    int f_y = motoComponents[1]->yF16 << 2 >> 16;
    int b_x = motoComponents[2]->xF16 << 2 >> 16;
    int b_y = motoComponents[2]->yF16 << 2 >> 16;

    // Tire rubber (outer, dark)
    gc->setColor(35, 35, 35);
    gc->fillCircle(f_x, f_y, r_px);
    gc->fillCircle(b_x, b_y, r_px);

    // Metal rim face (inner, lighter) — only when tire is large enough
    if (r_px > 3) {
        gc->setColor(140, 140, 140);
        gc->fillCircle(f_x, f_y, r_px - 3);
        gc->fillCircle(b_x, b_y, r_px - 3);
    }
}

void GamePhysics::renderWheelSpokes(GameCanvas* gc) {
    int r=field_29[1]->field_257;
    int xxxF16=(int)((int64_t)r*58982L>>16);
    gc->setColor(0,0,0);
    // isInGameMenu = false, so skip outer circles

    int8_t z=0;
    int angle; int c=MathF16::cosF16(angle=motoComponents[1]->angleF16), s=MathF16::sinF16(angle);
    int dx=(int)((int64_t)c*(int64_t)xxxF16>>16)+(int)((int64_t)(-s)*(int64_t)z>>16);
    int dy=(int)((int64_t)s*(int64_t)xxxF16>>16)+(int)((int64_t)c*(int64_t)z>>16);
    int ac=MathF16::cosF16(82354), as2=MathF16::sinF16(82354);
    for (int i=0;i<5;++i) {
        gc->drawLineF16(motoComponents[1]->xF16,motoComponents[1]->yF16,motoComponents[1]->xF16+dx,motoComponents[1]->yF16+dy);
        int tmp=dx; dx=(int)((int64_t)ac*(int64_t)dx>>16)+(int)((int64_t)(-as2)*(int64_t)dy>>16);
        dy=(int)((int64_t)as2*(int64_t)tmp>>16)+(int)((int64_t)ac*(int64_t)dy>>16);
    }
    z=0; c=MathF16::cosF16(angle=motoComponents[2]->angleF16); s=MathF16::sinF16(angle);
    dx=(int)((int64_t)c*(int64_t)xxxF16>>16)+(int)((int64_t)(-s)*(int64_t)z>>16);
    dy=(int)((int64_t)s*(int64_t)xxxF16>>16)+(int)((int64_t)c*(int64_t)z>>16);
    for (int i=0;i<5;++i) {
        gc->drawLineF16(motoComponents[2]->xF16,motoComponents[2]->yF16,motoComponents[2]->xF16+dx,motoComponents[2]->yF16+dy);
        int tmp=dx; dx=(int)((int64_t)ac*(int64_t)dx>>16)+(int)((int64_t)(-as2)*(int64_t)dy>>16);
        dy=(int)((int64_t)as2*(int64_t)tmp>>16)+(int)((int64_t)ac*(int64_t)dy>>16);
    }
    if (curentMotoLeague>0) {
        gc->setColor(curentMotoLeague>2?100:255, 0, curentMotoLeague>2?255:0);
        gc->drawCircle(motoComponents[2]->xF16<<2>>16, motoComponents[2]->yF16<<2>>16, 4);
        gc->drawCircle(motoComponents[1]->xF16<<2>>16, motoComponents[1]->yF16<<2>>16, 4);
    }
}

void GamePhysics::renderSmth(GameCanvas* gc, int v2, int v3, int v4, int v5) {
    int8_t v6=0; int v7=65536;
    int bx=motoComponents[0]->xF16, by=motoComponents[0]->yF16;
    int x6F16=0,y6F16=0,xF16=0,yF16=0,v14=0,v15=0,x2F16=0,y2F16=0,x3F16=0,y3F16=0,x4F16=0,y4F16=0,circleXF16=0,circleYF16=0,x5F16=0,y5F16=0;
    std::vector<std::vector<int>> v27,v28,v29;
    if (field_46) {
        if (field_37<32768) { v28=hardcodedArr2; v29=hardcodedArr1; v7=(int)((int64_t)field_37*131072L>>16); }
        else if (field_37>32768) { v6=1; v28=hardcodedArr1; v29=hardcodedArr3; v7=(int)((int64_t)(field_37-32768)*131072L>>16); }
        else v27=hardcodedArr1;
    } else {
        if (field_37<32768) { v28=hardcodedArr5; v29=hardcodedArr4; v7=(int)((int64_t)field_37*131072L>>16); }
        else if (field_37>32768) { v6=1; v28=hardcodedArr4; v29=hardcodedArr6; v7=(int)((int64_t)(field_37-32768)*131072L>>16); }
        else v27=hardcodedArr4;
    }
    for (size_t k=0;k<hardcodedArr1.size();++k) {
        int kx,ky;
        if (!v28.empty()) {
            kx=(int)((int64_t)v28[k][0]*(int64_t)(65536-v7)>>16)+(int)((int64_t)v29[k][0]*(int64_t)v7>>16);
            ky=(int)((int64_t)v28[k][1]*(int64_t)(65536-v7)>>16)+(int)((int64_t)v29[k][1]*(int64_t)v7>>16);
        } else { kx=v27[k][0]; ky=v27[k][1]; }
        int xx=bx+(int)((int64_t)v4*(int64_t)kx>>16)+(int)((int64_t)v2*(int64_t)ky>>16);
        int yy=by+(int)((int64_t)v5*(int64_t)kx>>16)+(int)((int64_t)v3*(int64_t)ky>>16);
        switch (k) {
        case 0: x2F16=xx; y2F16=yy; break; case 1: x3F16=xx; y3F16=yy; break;
        case 2: x4F16=xx; y4F16=yy; break; case 3: circleXF16=xx; circleYF16=yy; break;
        case 4: x5F16=xx; y5F16=yy; break; case 5: xF16=xx; yF16=yy; break;
        case 6: v14=xx; v15=yy; break; case 7: x6F16=xx; y6F16=yy; break;
        }
    }
    // wireframe rider (field_46 = false always)
    gc->setColor(0,0,0);
    gc->drawLineF16(xF16,yF16,x2F16,y2F16);
    gc->drawLineF16(x2F16,y2F16,x3F16,y3F16);
    gc->setColor(0,0,128);
    gc->drawLineF16(x3F16,y3F16,x4F16,y4F16);
    gc->drawLineF16(x4F16,y4F16,x5F16,y5F16);
    gc->drawLineF16(x5F16,y5F16,x6F16,y6F16);
    gc->setColor(156,0,0);
    gc->drawCircle(circleXF16<<2>>16,circleYF16<<2>>16,(65536+65536)<<2>>16);
    gc->setColor(0,0,0);
    gc->drawForthSpriteByCenter(x6F16<<2>>16,y6F16<<2>>16);
    gc->drawForthSpriteByCenter(v14<<2>>16,v15<<2>>16);
}

void GamePhysics::renderMotoAsLines(GameCanvas* gc, int v2, int v3, int v4, int v5) {
    int bx=motoComponents[2]->xF16, by=motoComponents[2]->yF16;
    int v9=bx+(int)((int64_t)v4*32768>>16), v10=by+(int)((int64_t)v5*32768>>16);
    int v11=bx-(int)((int64_t)v4*32768>>16), v12=by-(int)((int64_t)v5*32768>>16);
    int v13=motoComponents[0]->xF16+(int)((int64_t)v2*32768L>>16), v14=motoComponents[0]->yF16+(int)((int64_t)v3*32768L>>16);
    int v15=v13-(int)((int64_t)v2*131072L>>16), v16=v14-(int)((int64_t)v3*131072L>>16);
    int v17=v15+(int)((int64_t)v4*65536L>>16), v18=v16+(int)((int64_t)v5*65536L>>16);
    int v19=v15+(int)((int64_t)v2*49152L>>16)+(int)((int64_t)v4*49152L>>16), v20=v16+(int)((int64_t)v3*49152L>>16)+(int)((int64_t)v5*49152L>>16);
    int v21=v15+(int)((int64_t)v4*32768L>>16), v22=v16+(int)((int64_t)v5*32768L>>16);
    int v23=motoComponents[1]->xF16, v24=motoComponents[1]->yF16;
    int v25=motoComponents[4]->xF16-(int)((int64_t)v2*49152L>>16), v26=motoComponents[4]->yF16-(int)((int64_t)v3*49152L>>16);
    int v27=v25-(int)((int64_t)v4*32768L>>16), v28=v26-(int)((int64_t)v5*32768L>>16);
    int v29=v25-(int)((int64_t)v2*131072L>>16)+(int)((int64_t)v4*16384L>>16), v30=v26-(int)((int64_t)v3*131072L>>16)+(int)((int64_t)v5*16384L>>16);
    int v31=motoComponents[3]->xF16, v32=motoComponents[3]->yF16;
    int v33=v31+(int)((int64_t)v4*32768L>>16), v34=v32+(int)((int64_t)v5*32768L>>16);
    int v35=v31+(int)((int64_t)v4*114688L>>16)-(int)((int64_t)v2*32768L>>16), v36=v32+(int)((int64_t)v5*114688L>>16)-(int)((int64_t)v3*32768L>>16);
    gc->setColor(50,50,50);
    gc->drawCircle(v21<<2>>16,v22<<2>>16,(32768+32768)<<2>>16);
    if (!field_35) { gc->drawLineF16(v9,v10,v17,v18); gc->drawLineF16(v11,v12,v15,v16); }
    gc->drawLineF16(v13,v14,v15,v16); gc->drawLineF16(v13,v14,v31,v32);
    gc->drawLineF16(v19,v20,v33,v34); gc->drawLineF16(v33,v34,v35,v36);
    if (!field_35) { gc->drawLineF16(v31,v32,v23,v24); gc->drawLineF16(v35,v36,v23,v24); }
    gc->drawLineF16(v17,v18,v27,v28); gc->drawLineF16(v19,v20,v25,v26);
    gc->drawLineF16(v25,v26,v29,v30); gc->drawLineF16(v27,v28,v29,v30);
}

void GamePhysics::renderGame(GameCanvas* gc) {
    gc->clearScreenWithWhite();
    int xxF16=motoComponents[3]->xF16-motoComponents[4]->xF16;
    int yyF16=motoComponents[3]->yF16-motoComponents[4]->yF16;
    int mx; if ((mx=getSmthLikeMaxAbs(xxF16,yyF16))!=0) { xxF16=(int)(((int64_t)xxF16<<32)/(int64_t)mx>>16); yyF16=(int)(((int64_t)yyF16<<32)/(int64_t)mx>>16); }
    int v5=-yyF16;
    if (field_35) {
        int x1=motoComponents[4]->xF16, x2=motoComponents[3]->xF16;
        if (x2<x1) { int tmp=x2; x2=x1; x1=tmp; }
        levelLoader->gameLevel->method_183(x1,x2);
    }
    if (LevelLoader::isEnabledPerspective) levelLoader->renderLevel3D(gc,motoComponents[0]->xF16,motoComponents[0]->yF16);
    // isRenderMotoWithSprites = false → skip renderEngine
    // isInGameMenu = false → renderWheelTires
    renderWheelTires(gc);
    renderWheelSpokes(gc);
    gc->setColor(50,50,50);
    gc->method_142(motoComponents[1]->xF16<<2>>16,motoComponents[1]->yF16<<2>>16,const175_1_half[0]<<2>>16,MathF16::atan2F16(xxF16,yyF16));
    if (!field_35) renderMotoFork(gc);
    renderSmth(gc,xxF16,yyF16,v5,xxF16);
    // isRenderMotoWithSprites = false → renderMotoAsLines
    renderMotoAsLines(gc,xxF16,yyF16,v5,xxF16);
    levelLoader->renderTrackNearestLine(gc);
}
