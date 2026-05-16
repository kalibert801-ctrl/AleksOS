#pragma once
// ntp_manager.h — NTP time synchronisation via WiFi

#include <Arduino.h>

// Sync from NTP servers. Call after WiFi connects.
// Updates settings.timeH / settings.timeM and calls timeSet().
void ntpSync();

// Returns true if the last sync was successful.
bool ntpIsSynced();
