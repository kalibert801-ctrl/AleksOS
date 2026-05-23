#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  boot_screen.h  —  AleksOS animated boot screen
// ─────────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus

#include <Arduino.h>

// Load /boot.raw from SD card into PSRAM.
// Call AFTER sdMgr.init() and BEFORE bootScreenRun().
// If the file is missing or PSRAM unavailable, falls back to text animation.
void bootLogoLoad();

// Run the intro animation (image from PSRAM, or fallback text fade-in).
// Call AFTER initDisplay() and bootLogoLoad().
void bootScreenRun();

// Update the status line while initialisation is in progress.
// pct : 0–100  (reserved for future progress bar)
// msg : short string ≤ 30 chars
void bootProgress(uint8_t pct, const char* msg);

// Advance the dot spinner — call every ~100 ms during startup.
void bootTick();

// Fade out, clear screen, free boot image buffer.
void bootScreenDone();

#endif // __cplusplus
