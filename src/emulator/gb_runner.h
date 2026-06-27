#pragma once

// Game Boy emulator entry point (Peanut-GB).
// Blocks until the game exits (via in-game menu → Exit or SELECT+START).
// Requires peanut_gb.h to be present at src/emulator/peanut_gb.h.
void gb_run(const char *romPath);
