// emu_runner.cpp — nofrendo wrapper for AleksOS

extern "C" {
#include "nofrendo/nofrendo.h"
#include "nofrendo/event.h"
}

#include "emu_runner.h"
#include "display/display_manager.h"
#include "config.h"
#include <stdio.h>

// Declared in osd.cpp (C++ linkage)
bool osd_rom_load(const char *path);
void osd_rom_free();

extern "C" int emu_run(const char *path) {
    printf("[EMU] Loading ROM: %s\n", path);
    if (!osd_rom_load(path)) return -1;

    lcd.fillScreen(TFT_BLACK);
    printf("[EMU] Starting nofrendo...\n");
    int ret = main_loop(path, system_autodetect);
    lcd.fillScreen(TFT_BLACK);

    osd_rom_free();
    printf("[EMU] nofrendo exited: %d\n", ret);
    return ret;
}

extern "C" void emu_quit(void) {
    event_t h = event_get(event_quit);
    if (h) h(0);
}
