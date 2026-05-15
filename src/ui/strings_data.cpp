#include "lang.h"
#include "settings.h"

// ── Темы (RGB565) ─────────────────────────────────────────────
// Индекс 0: RetroNight (основная тема — тёмная, синяя)
//   bg=#0A0F1E  header=#111827  rowEven=#0D1526  rowOdd=#0A1020
//   sel=#1D4ED8  txtPri=#E2E8F0  txtSec=#64748B  accent=#38BDF8
//   danger=#F87171  ok=#4ADE80
const Theme565 THEMES[THEME_COUNT] = {
    // Dark (RetroNight)
    { 0x0863, 0x10C4, 0x08A4, 0x0884, 0x1A7B,
      0xE75E, 0x63B1, 0x3DFF, 0xFB8E, 0x4EF0 },
    // Light
    { 0xFFFF, 0xC638, 0xFFFF, 0xDEDB, 0x435C,
      0x0000, 0x4208, 0x001F, 0xF800, 0x03E0 },
    // Green
    { 0x0020, 0x0061, 0x0020, 0x0041, 0x02A0,
      0x07E0, 0x02E0, 0x07FF, 0xF800, 0x07E0 },
    // Amber
    { 0x2000, 0x4820, 0x2000, 0x3000, 0xFD20,
      0xFDE0, 0x9240, 0xFD20, 0xF800, 0xFDE0 },
};

const Str STR_RU = {
    "RetroESP",  "Zagruzka...",
    "ROM ne naydeny",  "Polozhite .nes v /FomiCon na SD",
    "SD karta ne naydena",  "Vstavte SD kartu i perezagruzite",
    "IGRAT",  "NASTROYKI",  "NAZAD",
    "Igraet",  "Tap = vyhod",  "Nastroyki",
    { "Yarkost","Gromkost","Tema","Yazyk","Masshtab",
      "FPS","Avtosokhr","AvtoYark","Zvuk","Tip zvuka","Versiya","RAM" },
    "",  "",  // btnUp/btnDown — больше не используем текст
    "Info o ROM",  "Mapper:",  "Razmer:",  "Put:",
};

const Str STR_EN = {
    "RetroESP",  "Loading...",
    "No ROMs found",  "Put .nes files in /FomiCon on SD",
    "SD card not found",  "Insert SD card and reset",
    "PLAY",  "SETUP",  "BACK",
    "Now Playing",  "Tap header to exit",  "Settings",
    { "Brightness","Volume","Theme","Language","Scale",
      "FPS","Auto Save","Auto Bright","Sound","Sound Type","Version","RAM" },
    "",  "",
    "ROM Info",  "Mapper:",  "Size:",  "Path:",
};
