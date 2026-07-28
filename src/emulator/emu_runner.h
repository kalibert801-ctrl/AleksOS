#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Run emulator (blocking). Returns 0 on clean exit, -1 on error.
int emu_run(const char *path);

// Read iNES mapper number from file header. Returns -1 if not a valid NES file.
int  nes_read_mapper(const char *path);
// Returns true if the given mapper number is supported by nofrendo.
bool nes_mapper_supported(int mapper_num);

// Feed controller state (bit layout: A=0x01 B=0x02 Sel=0x04 Sta=0x08 Up=0x10 Dn=0x20 L=0x40 R=0x80)
void emu_setController(uint8_t state);
void emu_setController2(uint8_t state);  // player 2 (Bluetooth)

#ifdef __cplusplus
}
#endif
