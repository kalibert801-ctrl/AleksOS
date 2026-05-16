// ntp_manager.cpp — NTP time sync for RetroESP
#include "network/ntp_manager.h"
#include "settings.h"
#include "system/time_manager.h"
#include <WiFi.h>
#include <time.h>

static bool _synced = false;

void ntpSync() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NTP] Not connected — skipping sync.");
        return;
    }

    // UTC+3 (Moscow) by default — no DST. User can adjust Hour later.
    // To use a different timezone, change the UTC offset (in seconds).
    const long  UTC_OFFSET_SEC = 3 * 3600;  // UTC+3
    const int   DST_OFFSET_SEC = 0;

    configTime(UTC_OFFSET_SEC, DST_OFFSET_SEC,
               "pool.ntp.org", "time.nist.gov", "time.google.com");

    struct tm timeinfo = {};
    uint32_t deadline = millis() + 10000;
    while (millis() < deadline) {
        if (getLocalTime(&timeinfo, 2000)) {
            _synced = true;
            int h = timeinfo.tm_hour;
            int m = timeinfo.tm_min;
            timeSet(h, m);
            settings.timeH = (unsigned char)h;
            settings.timeM = (unsigned char)m;
            Serial.printf("[NTP] Synced — %02d:%02d\n", h, m);
            return;
        }
        delay(500);
    }
    Serial.println("[NTP] Sync timeout.");
}

bool ntpIsSynced() { return _synced; }
