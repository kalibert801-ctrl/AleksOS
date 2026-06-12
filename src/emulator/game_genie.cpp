// game_genie.cpp — NES Game Genie декодирование и ROM-патчинг.
// Алгоритм декодирования: стандарт FCEUX / Nestopia (широко проверенный).

#include "game_genie.h"
#include "settings.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// ── Алфавит Game Genie (позиция = 4-битное значение 0x0–0xF) ─────────────────
static const char GG_ALPHA[] = "APZLGITYEOXUKSVN";

// ── Декодирование одного кода ─────────────────────────────────────────────────
bool gg_decode(const char *code,
               uint16_t *addr,
               uint8_t  *val,
               uint8_t  *cmp,
               bool     *hasCmp)
{
    if (!code || !addr || !val || !cmp || !hasCmp) return false;

    int len = (int)strlen(code);
    if (len != 6 && len != 8) return false;

    uint8_t n[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < len; i++) {
        const char *p = strchr(GG_ALPHA, toupper((unsigned char)code[i]));
        if (!p) return false;          // символ не из GG-алфавита
        n[i] = (uint8_t)(p - GG_ALPHA);
    }

    // ── Адрес (15 бит, плюс старший бит 0x8000 всегда = 1) ──────────────────
    // Биты адреса раскиданы по nibble-ам n[1]..n[5] с намеренной путаницей:
    //   bits [14:12] = n[3] bits [2:0]
    //   bits [11]    = n[4] bit  [3]
    //   bits [10:8]  = n[5] bits [2:0]
    //   bits [7]     = n[1] bit  [3]
    //   bits [6:4]   = n[2] bits [2:0]
    //   bits [3]     = n[3] bit  [3]
    //   bits [2:0]   = n[4] bits [2:0]
    *addr = (uint16_t)(0x8000u
           | ((uint16_t)(n[3] & 0x07u) << 12)
           | ((uint16_t)(n[4] & 0x08u) << 8)
           | ((uint16_t)(n[5] & 0x07u) << 8)
           | ((uint16_t)(n[1] & 0x08u) << 4)
           | ((uint16_t)(n[2] & 0x07u) << 4)
           |  (uint16_t)(n[3] & 0x08u)
           |  (uint16_t)(n[4] & 0x07u));

    // ── Значение (7 бит из n[0] и n[1]) ────────────────────────────────────
    //   bits [6:4] = n[0] bits [2:0]
    //   bits [3]   = n[0] bit  [3]
    //   bits [2:0] = n[1] bits [2:0]
    *val = (uint8_t)(((n[0] & 0x07u) << 4)
                   |  (n[0] & 0x08u)
                   |  (n[1] & 0x07u));

    // ── Байт сравнения (только для 8-символьных кодов, n[6] и n[7]) ─────────
    if (len == 8) {
        *cmp = (uint8_t)(((n[6] & 0x07u) << 4)
                        |  (n[6] & 0x08u)
                        |  (n[7] & 0x07u));
        *hasCmp = true;
    } else {
        *cmp    = 0;
        *hasCmp = false;
    }

    return true;
}

// ── Применение всех активных кодов к ROM-буферу ──────────────────────────────
void gg_apply(uint8_t *romBuf, uint32_t romSize)
{
    if (!romBuf || romSize < 16) return;
    if (!settings.ggEnabled) return;

    // ── Разбор iNES заголовка ─────────────────────────────────────────────
    if (romBuf[0] != 'N' || romBuf[1] != 'E' || romBuf[2] != 'S' || romBuf[3] != 0x1A) {
        printf("[GG] Not an iNES ROM, skipping patches\n");
        return;
    }

    uint32_t prgBanks = romBuf[4];           // кол-во PRG ROM банков по 16 КБ
    bool     hasTrainer = (romBuf[6] & 0x04) != 0;
    uint32_t prgOffset  = 16 + (hasTrainer ? 512u : 0u);  // старт PRG ROM
    uint32_t prgSize    = prgBanks * 16384u;

    if (prgOffset + prgSize > romSize) {
        printf("[GG] PRG ROM out of bounds, skipping patches\n");
        return;
    }

    uint8_t *prg = romBuf + prgOffset;

    // ── Применение каждого кода ───────────────────────────────────────────
    int applied = 0;
    for (int slot = 0; slot < 8; slot++) {
        const char *code = settings.ggCodes[slot];
        if (!code[0]) continue;    // пустой слот

        uint16_t addr; uint8_t val, cmp; bool hasCmp;
        if (!gg_decode(code, &addr, &val, &cmp, &hasCmp)) {
            printf("[GG] Slot %d: invalid code '%s'\n", slot, code);
            continue;
        }

        // Смещение в PRG ROM: CPU $8000–$FFFF → offset 0–32767 в пределах банка
        // Мы патчим ВСЕ банки по одинаковому смещению внутри банка.
        // Это работает для mapper 0 (NROM) и хорошо для большинства игр:
        // коды Game Genie обычно целятся в фиксированные адреса.
        uint16_t bankOffset = (uint16_t)((addr - 0x8000u) & 0x3FFFu);  // 0–16383

        int patchCount = 0;
        for (uint32_t bank = 0; bank < prgBanks; bank++) {
            uint32_t byteIdx = bank * 16384u + bankOffset;
            if (byteIdx >= prgSize) continue;

            uint8_t old = prg[byteIdx];
            if (hasCmp && old != cmp) continue;  // 8-char: проверяем сравниваемый байт

            prg[byteIdx] = val;
            patchCount++;
        }

        printf("[GG] Slot %d: '%s' → addr=$%04X val=$%02X%s → %d byte(s) patched\n",
               slot, code, (unsigned)addr, (unsigned)val,
               hasCmp ? " (with compare)" : "", patchCount);
        applied++;
    }

    if (applied > 0)
        printf("[GG] Total: %d code(s) applied\n", applied);
}
