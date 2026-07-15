// emu_runner.cpp — nofrendo wrapper for AleksOS

extern "C" {
#include "nofrendo/nofrendo.h"
#include "nofrendo/event.h"
}
// Forward-declare mmc_peek from nofrendo/nes/nes_mmc.c (C linkage, no header chain needed)
extern "C" bool mmc_peek(int map_num);

#include "emu_runner.h"
#include "display/display_manager.h"
#include "config.h"
#include "settings.h"
#include "emulator/game_genie.h"
#include <SD.h>
#include <stdio.h>

// Declared in osd.cpp (C++ linkage)
bool osd_rom_load(const char *path);
void osd_rom_free();
uint8_t *osd_getrom(uint32_t *sizeOut);

// osd_shutdown() is declared extern "C" in nofrendo/osd.h
extern "C" void osd_shutdown(void);

int nes_read_mapper(const char *path) {
    File f = SD.open(path, FILE_READ);
    if (!f) return -1;
    uint8_t hdr[16];
    int n = f.read(hdr, 16);
    f.close();
    if (n < 16) return -1;
    if (hdr[0]!='N' || hdr[1]!='E' || hdr[2]!='S' || hdr[3]!=0x1A) return -1;
    return (hdr[7] & 0xF0) | (hdr[6] >> 4);
}

bool nes_mapper_supported(int mapper) {
    if (mapper < 0) return true;  // неверный заголовок — дать nofrendo попробовать
    return mmc_peek(mapper);
}

extern "C" int emu_run(const char *path) {
    printf("[EMU] Loading ROM: %s\n", path);
    if (!osd_rom_load(path)) return -1;

    // ── Game Genie: патчим PRG ROM ДО запуска nofrendo ────────────────────
    // osd_getrom() возвращает указатель на буфер, из которого nofrendo читает ROM.
    // Патчинг здесь гарантирует что nofrendo видит уже изменённые байты.
    if (settings.ggEnabled) {
        uint32_t romSize = 0;
        uint8_t *romBuf  = osd_getrom(&romSize);
        if (romBuf && romSize > 0) gg_apply(romBuf, romSize);
    }

    lcd.fillScreen(TFT_BLACK);
    printf("[EMU] Starting nofrendo...\n");
    int ret = main_loop(path, system_autodetect);
    lcd.fillScreen(TFT_BLACK);

    // Stop I2S, free frame buffer and bitmap — must be called before
    // returning to menu, otherwise audio DMA keeps running in background.
    osd_shutdown();
    osd_rom_free();
    printf("[EMU] nofrendo exited: %d\n", ret);
    return ret;
}

