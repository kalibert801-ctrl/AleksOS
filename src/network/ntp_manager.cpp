// ntp_manager.cpp — NTP time sync for RetroESP
// Шаг 1: узнаём часовой пояс по IP через ip-api.com  (HTTP, бесплатно, без ключа)
// Шаг 2: синхронизируем время через NTP с полученным offset
#include "network/ntp_manager.h"
#include "settings.h"
#include "system/time_manager.h"
#include "storage/config_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static bool _synced       = false;
static long _utcOffsetSec = 3 * 3600;  // запасной вариант: UTC+3

// ── Определение часового пояса по IP ─────────────────────────────────────────
// ip-api.com: GET http://ip-api.com/json/?fields=offset
// Ответ: {"offset":10800}  — смещение в секундах, уже включает летнее время.
// Лимит: 45 запросов/мин — для нас (1 раз при подключении) более чем достаточно.
static long detectTimezoneByIP() {
    HTTPClient http;
    http.begin("http://ip-api.com/json/?fields=offset");
    http.setTimeout(5000);
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        String body = http.getString();
        http.end();

        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, body);
        if (!err && doc.containsKey("offset")) {
            long off = doc["offset"].as<long>();
            Serial.printf("[NTP] IP-timezone: UTC%+ld h  (%ld s)\n", off / 3600, off);
            return off;
        }
        Serial.printf("[NTP] IP-timezone parse error: %s\n", err.c_str());
    } else {
        http.end();
        Serial.printf("[NTP] IP-timezone HTTP error: %d\n", code);
    }
    Serial.println("[NTP] Fallback → UTC+3");
    return 3 * 3600;
}

// ── Публичный интерфейс ───────────────────────────────────────────────────────
void ntpSync() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NTP] Not connected — skip.");
        return;
    }

    // 1. Часовой пояс по IP
    _utcOffsetSec = detectTimezoneByIP();

    // 2. Настраиваем SNTP (DST=0 — уже учтено в offset от ip-api)
    configTime(_utcOffsetSec, 0,
               "pool.ntp.org", "time.nist.gov", "time.google.com");

    // 3. Ждём синхронизации (не более 6 сек)
    struct tm timeinfo = {};
    if (getLocalTime(&timeinfo, 6000)) {
        _synced = true;
        int h    = timeinfo.tm_hour;
        int m    = timeinfo.tm_min;
        int d    = timeinfo.tm_mday;
        int mon  = timeinfo.tm_mon + 1;          // tm_mon: 0–11 → 1–12
        int year = timeinfo.tm_year + 1900;
        timeSet(h, m);
        timeSetDate((uint8_t)d, (uint8_t)mon, (uint16_t)year);
        settings.timeH   = (unsigned char)h;
        settings.timeM   = (unsigned char)m;
        settings.timeDay = (unsigned char)d;
        settings.timeMon = (unsigned char)mon;
        settings.timeYear= (unsigned short)year;
        cfgSave();
        Serial.printf("[NTP] OK — %02d:%02d  %02d.%02d.%04d  (UTC%+ld)\n",
                      h, m, d, mon, year, _utcOffsetSec / 3600);
    } else {
        Serial.println("[NTP] Timeout — time not synced.");
    }
}

bool ntpIsSynced()     { return _synced; }
long ntpGetUtcOffset() { return _utcOffsetSec; }
