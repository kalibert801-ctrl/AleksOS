#pragma once

/*  lang.h — строки локализации.
    Имя lang.h (не strings.h) чтобы не перехватывать системный <string.h>.
    Всё C++ — внутри #ifdef __cplusplus.                               */

#ifdef __cplusplus

#include "settings.h"

struct Str {
    const char *appName, *loading, *noRoms, *noRomsHint;
    const char *noSD, *noSDHint;
    const char *play, *setup, *back;
    const char *nowPlaying, *tapBack;
    const char *settings;
    const char *sLbl[12];
    const char *btnUp, *btnDown, *btnLeft, *btnRight;
    const char *romInfo, *romMapper, *romSize, *romPath;
};

extern const Str STR_RU;
extern const Str STR_EN;

inline const Str& S() {
    return (settings.language == LANG_RU) ? STR_RU : STR_EN;
}

#endif /* __cplusplus */
