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
    const char *listTitle;   // заголовок экрана списка ROM
};

extern const Str STR_RU;
extern const Str STR_EN;
extern const Str STR_CZ;

inline const Str& S() {
    switch (settings.language) {
        case LANG_RU: return STR_RU;
        case LANG_CZ: return STR_CZ;
        default:      return STR_EN;
    }
}

// ── cyrStr() ─────────────────────────────────────────────────────────
// Транскодирует UTF-8 строку в однобайтовое представление шрифта CyrDejaVu9.
// ASCII (0x00-0x7F) проходит без изменений.
// Кириллица (А-Я, а-я) транскодируется в позиции 0x80-0xBF шрифта:
//   А-Я: U+0410-U+042F  →  0x80-0x9F  (D0 90..D0 AF → n-0x10)
//   а-п: U+0430-U+043F  →  0xA0-0xAF  (D0 B0..D0 BF → n-0x10)
//   р-я: U+0440-U+044F  →  0xB0-0xBF  (D1 80..D1 8F → n+0x30)
// Вызывайте перед каждым lcd.drawString(S().*...) при использовании
// шрифта CyrDejaVu9.
// Определение в src/ui/strings_data.cpp — единственный экземпляр на всю программу.
const char* cyrStr(const char* s);

#endif /* __cplusplus */
