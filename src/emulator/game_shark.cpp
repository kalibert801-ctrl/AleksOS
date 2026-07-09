// game_shark.cpp — NES Game Shark / Pro Action Replay декодирование.
// Применение кодов (запись в RAM каждый кадр) реализовано в osd.cpp,
// где есть прямой доступ к внутренностям nofrendo.

#include "game_shark.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

bool gs_decode(const char *code, uint16_t *addr, uint8_t *val) {
    if (!code || !addr || !val) return false;
    if ((int)strlen(code) != 6) return false;

    char upper[7];
    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)code[i])) return false;
        upper[i] = (char)toupper((unsigned char)code[i]);
    }
    upper[6] = '\0';

    char addrBuf[5]; memcpy(addrBuf, upper, 4); addrBuf[4] = '\0';
    char valBuf[3];  memcpy(valBuf, upper + 4, 2); valBuf[2] = '\0';

    *addr = (uint16_t)strtol(addrBuf, nullptr, 16);
    *val  = (uint8_t) strtol(valBuf,  nullptr, 16);
    return true;
}
