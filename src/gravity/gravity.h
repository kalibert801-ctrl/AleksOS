#pragma once
#include <cstdint>

// Returns false if /levels.mrg not found on SD root
bool gravityOpen(int league = 0, int level = 0);
void gravityClose();

// Call every loop iteration when state == S_GRAVITY
// Returns: 0=running, 1=level complete, 2=crashed, 3=exit requested
int  gravityUpdate();

// Touch: x<0 = no touch
void gravityHandleTouch(int x, int y);
void gravityNoTouch();

// Held physical buttons (raw readCurrent state)
bool gravityNavBtn(uint8_t heldPhysBtn);

// Level/league selection screen. Call before gravityOpen.
// Returns false if levels.mrg not found or user pressed Back.
bool gravityLevelSelect(int* outLeague, int* outLevel);

// Pause menu: returns false if user chose "Exit"
bool gravityPause();
