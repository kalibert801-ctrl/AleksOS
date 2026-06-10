#include "lang.h"
#include "settings.h"

// ── cyrStr() ──────────────────────────────────────────────────────────────────
// Транскодирует UTF-8 Кириллицу в позиции шрифта CyrDejaVu9.
//
// 4 чередующихся буфера (round-robin): каждый вызов получает следующий слот.
// Это устраняет баг с одним статическим буфером: при нескольких вызовах в одном
// выражении (аргументы функции) C++ не гарантирует порядок вычисления, и второй
// вызов перезаписывал бы буфер первого. Теперь до 4 вызовов в одном выражении —
// каждый пишет в отдельный буфер и возвращает устойчивый указатель.
const char* cyrStr(const char* s) {
    static char bufs[4][128];
    static int  slot = 0;
    slot = (slot + 1) & 3;
    char* buf = bufs[slot];
    if (!s) { buf[0] = '\0'; return buf; }   // защита от nullptr
    char* dst = buf;
    const uint8_t* p = (const uint8_t*)s;
    while (*p && (dst - buf) < 126) {
        uint8_t c = *p++;
        if (c < 0x80) {
            *dst++ = (char)c;
        } else if (c == 0xD0 && *p) {
            uint8_t n = *p++;
            if (n >= 0x90 && n <= 0xBF) *dst++ = (char)(n - 0x10);   // А-п → 0x80-0xAF
        } else if (c == 0xD1 && *p) {
            uint8_t n = *p++;
            if (n >= 0x80 && n <= 0x8F) *dst++ = (char)(n + 0x30);   // р-я → 0xB0-0xBF
        } else if (c >= 0x80) {
            // Неизвестная многобайтовая последовательность (é, ü, …, и т.д.) —
            // пропускаем все continuation bytes (0x80–0xBF) чтобы не получить мусор.
            while (*p && (*p & 0xC0) == 0x80) p++;
        }
    }
    *dst = '\0';
    return buf;
}

// Цветовые палитры тем перенесены в src/ui/themes/theme_*.cpp
// и регистрируются через ThemeRegistry.

const Str STR_RU = {
    "RetroESP",  "Загрузка...",
    "ROM не найдены",  "Положите .nes в /FomiCon на SD",
    "SD карта не найдена",  "Вставьте SD и перезагрузите",
    "ИГРАТЬ",  "НАСТРОЙКИ",  "НАЗАД",
    "Сейчас играет",  "Нажмите = выход",  "Настройки",
    { "Яркость","Громкость","Тема","Язык","Масштаб",
      "FPS","Авто-сохр","Авто-ярк","Звук","Тип звука","Версия","RAM" },
    "",  "",  "",  "",   // btnUp, btnDown, btnLeft, btnRight (не используются)
    "Инфо о ROM",  "Маппер:",  "Размер:",  "Путь:",
    "NES ИГРЫ",
};

const Str STR_EN = {
    "RetroESP",  "Loading...",
    "No ROMs found",  "Put .nes files in /FomiCon on SD",
    "SD card not found",  "Insert SD card and reset",
    "PLAY",  "SETUP",  "BACK",
    "Now Playing",  "Tap header to exit",  "Settings",
    { "Brightness","Volume","Theme","Language","Scale",
      "FPS","Auto Save","Auto Bright","Sound","Sound Type","Version","RAM" },
    "",  "",  "",  "",   // btnUp, btnDown, btnLeft, btnRight (unused)
    "ROM Info",  "Mapper:",  "Size:",  "Path:",
    "NES GAMES",
};

const Str STR_CZ = {
    "RetroESP",  "Nacitani...",
    "Zadne ROM soubory",  "Vlozit .nes do /FomiCon na SD",
    "SD karta nenalezena",  "Vlozit SD a restartovat",
    "HRAT",  "NASTAVENI",  "ZPET",
    "Nyni hraje",  "Klepit = navrat",  "Nastaveni",
    { "Jas","Hlasitost","Tema","Jazyk","Meritko",
      "FPS","Auto Uloz","Auto Jas","Zvuk","Typ zvuku","Verze","RAM" },
    "",  "",  "",  "",   // btnUp, btnDown, btnLeft, btnRight (unused)
    "Info o ROM",  "Mapper:",  "Velikost:",  "Cesta:",
    "NES HRY",
};
