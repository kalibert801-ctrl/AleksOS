// pico_ota.cpp — UART OTA update for Pico controller
//
// Протокол: детально описано в pico_controller.ino
// Serial2 = UART між ESP32 і Pico (PICO_RX_PIN=22, PICO_TX_PIN=27, 115200 baud)
//
// ВАЖЛИВО — порядок кроків:
//   1. Спочатку HTTP GET (отримуємо розмір і відкриваємо з'єднання)
//   2. Потім тригеримо Pico OTA
//   3. Одразу надсилаємо розмір (Pico не чекає довго)
//   4. Стримінг сторінок з вже відкритого HTTP з'єднання
//
//   Якщо спочатку тригерити Pico а потім робити HTTP GET — Pico
//   завершує таймаут (~5с) поки ESP32 завантажує прошивку (7-15с).
//   Це призводить до запису сміття і несправної прошивки на Pico.

#include "network/pico_ota.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino.h>

// ── CRC16-CCITT (polynomial 0x1021, init 0xFFFF) ─────────────────────────────
static uint16_t crc16(const uint8_t *data, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

// ── Розбір тегу "v14.5" → 1405 ──────────────────────────────────────────────
int picoOtaParseTag(const char *tag) {
    if (!tag || tag[0] == '\0') return -1;
    const char *p = tag;
    if (*p == 'v' || *p == 'V') p++;
    int major = 0, minor = 0;
    while (*p >= '0' && *p <= '9') major = major * 10 + (*p++ - '0');
    if (*p == '.') p++;
    while (*p >= '0' && *p <= '9') minor = minor * 10 + (*p++ - '0');
    if (major == 0 && minor == 0) return -1;
    return major * 100 + minor;
}

// ── Запит версії від Pico ────────────────────────────────────────────────────
int picoOtaGetVersion() {
    delay(50);
    while (Serial2.available()) Serial2.read();

    uint8_t cmd[4] = {0xAA, 0x02, 0x00, 0x02};
    Serial2.write(cmd, 4);

    Serial2.setTimeout(1500);
    uint8_t resp[4];
    if (Serial2.readBytes(resp, 4) != 4) {
        Serial.println("[PICOTA] No version response");
        return -1;
    }
    if (resp[0] != 0xAA || resp[1] != 0x56) {
        Serial.printf("[PICOTA] Bad version response: %02X %02X\n", resp[0], resp[1]);
        return -1;
    }
    int ver = ((int)resp[2] << 8) | resp[3];
    Serial.printf("[PICOTA] Pico version: %d.%d (encoded %d)\n",
                  ver / 100, ver % 100, ver);
    return ver;
}

// ── URL builder ──────────────────────────────────────────────────────────────
const char *picoOtaBuildUrl(const char *tag) {
    static char buf[200];
    snprintf(buf, sizeof(buf),
             "https://github.com/" "kalibert801-ctrl" "/" "AleksOS"
             "/releases/download/%s/pico_firmware.bin", tag);
    return buf;
}

// ── Основна функція OTA ───────────────────────────────────────────────────────
bool picoOtaUpdate(const char *binUrl, void (*progressCb)(int pct)) {
    const int PAGE = 256;

    Serial.printf("[PICOTA] URL: %s\n", binUrl);

    // ════════════════════════════════════════════════════════════════════════
    // КРОК 1: Спочатку HTTP GET — отримуємо розмір і відкриваємо з'єднання.
    // Тільки ПІСЛЯ цього тригеримо Pico, щоб він не чекав HTTP завантаження.
    // ════════════════════════════════════════════════════════════════════════
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, binUrl);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(20000);
    http.addHeader("User-Agent", "RetroESP-PicoOTA/1.0");
    http.addHeader("Cache-Control", "no-cache");

    Serial.println("[PICOTA] HTTP GET...");
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[PICOTA] HTTP error: %d\n", code);
        http.end();
        return false;
    }

    int totalSize = (int)http.getSize();
    if (totalSize <= 0) {
        Serial.println("[PICOTA] Unknown content length");
        http.end();
        return false;
    }
    Serial.printf("[PICOTA] Firmware size: %d bytes (%d pages)\n",
                  totalSize, (totalSize + PAGE - 1) / PAGE);

    // ════════════════════════════════════════════════════════════════════════
    // КРОК 2: Тепер тригеримо Pico OTA.
    // HTTP з'єднання вже відкрите — розмір відомий одразу.
    // Між ACK від Pico і відправкою розміру — нуль затримки.
    // ════════════════════════════════════════════════════════════════════════
    // Чекаємо 100мс щоб Pico гарантовано не відправив кнопковий пакет
    // саме зараз (пакети кожні 16мс → 100мс = 6+ пакетів, всі вже прийшли)
    delay(100);
    while (Serial2.available()) Serial2.read();  // чистимо буфер

    // Надсилаємо OTA trigger і чекаємо [0xAA][0xF1]
    // Pico отримає trigger в loop(), обробить rxByte(), надішле F1 ACK
    {
        uint8_t trigger[4] = {0xAA, 0xF0, 0x00, 0xF0};
        Serial2.write(trigger, 4);

        // Читаємо відповідь пропускаючи можливий кнопковий пакет [0xAA][0x42]
        // який міг бути відправлений Pico прямо перед обробкою trigger
        uint8_t ack[4] = {0, 0, 0, 0};
        Serial2.setTimeout(3000);
        uint32_t deadline = millis() + 3000;
        bool gotF1 = false;
        while (millis() < deadline) {
            if (Serial2.readBytes(ack, 4) != 4) break;
            if (ack[0] == 0xAA && ack[1] == 0xF1) { gotF1 = true; break; }
            // Це кнопковий пакет або сміття — ігноруємо, читаємо ще
        }
        if (!gotF1) {
            Serial.println("[PICOTA] Pico did not respond to OTA trigger");
            http.end();
            return false;
        }
    }
    Serial.println("[PICOTA] Pico ready. Sending size...");

    // ════════════════════════════════════════════════════════════════════════
    // КРОК 3: Одразу надсилаємо розмір — Pico щойно прийняв ACK і чекає.
    // ════════════════════════════════════════════════════════════════════════
    {
        uint8_t szBuf[4] = {
            (uint8_t)( totalSize        & 0xFF),
            (uint8_t)((totalSize >> 8)  & 0xFF),
            (uint8_t)((totalSize >> 16) & 0xFF),
            (uint8_t)((totalSize >> 24) & 0xFF)
        };
        Serial2.write(szBuf, 4);

        // Чекаємо F2 ACK, пропускаємо можливі строві байти
        uint8_t sack[4] = {0, 0, 0, 0};
        Serial2.setTimeout(3000);
        uint32_t sackDeadline = millis() + 3000;
        bool gotF2 = false;
        while (millis() < sackDeadline) {
            if (Serial2.readBytes(sack, 4) != 4) break;
            if (sack[0] == 0xAA && sack[1] == 0xF2) { gotF2 = true; break; }
        }
        if (!gotF2) {
            Serial.println("[PICOTA] No size ACK from Pico");
            http.end();
            return false;
        }
    }
    Serial.println("[PICOTA] Size ACK. Streaming pages...");

    // ════════════════════════════════════════════════════════════════════════
    // КРОК 4: Стримінг сторінок з вже відкритого HTTP з'єднання.
    // ════════════════════════════════════════════════════════════════════════
    WiFiClient &stream = http.getStream();
    int totalPages = (totalSize + PAGE - 1) / PAGE;
    int bytesRead  = 0;

    for (int p = 0; p < totalPages; p++) {
        uint8_t page[PAGE];
        memset(page, 0xFF, PAGE);   // pad остання сторінка 0xFF (=стерта flash)
        int toRead = min(PAGE, totalSize - bytesRead);

        int got = 0;
        uint32_t pageDeadline = millis() + 10000;
        while (got < toRead) {
            int n = stream.readBytes(page + got, toRead - got);
            if (n > 0) { got += n; }
            else if (millis() > pageDeadline) {
                Serial.printf("[PICOTA] HTTP timeout at page %d\n", p);
                http.end();
                return false;
            }
        }
        bytesRead += toRead;

        uint16_t crc = crc16(page, PAGE);

        Serial2.write(page, PAGE);
        uint8_t crcBuf[2] = {(uint8_t)(crc & 0xFF), (uint8_t)(crc >> 8)};
        Serial2.write(crcBuf, 2);

        // Чекаємо ACK (4с — включає час стирання сектора ~50мс кожні 16 стор.)
        Serial2.setTimeout(4000);
        uint8_t pageAck[4] = {0, 0, 0, 0};  // ініціалізуємо щоб уникнути сміття зі стеку
        if (Serial2.readBytes(pageAck, 4) != 4 ||
            pageAck[0] != 0xAA || pageAck[1] != 0xF3) {
            Serial.printf("[PICOTA] Page %d: NAK or timeout (got %02X %02X)\n",
                          p, pageAck[0], pageAck[1]);
            http.end();
            return false;
        }

        if (progressCb) progressCb((p + 1) * 100 / totalPages);

        if ((p % 16) == 15 || p == totalPages - 1) {
            Serial.printf("[PICOTA] Page %d/%d  (%d%%)\n",
                          p + 1, totalPages, (p + 1) * 100 / totalPages);
        }
    }

    http.end();

    // ── Крок 5: чекаємо сигнал "done" від Pico ──────────────────────────────
    // Фаза 2 (запис flash): ~4-6 секунд для 96KB прошивки
    // Таймаут 15с — з великим запасом
    Serial2.setTimeout(15000);
    uint8_t done[4] = {0, 0, 0, 0};
    if (Serial2.readBytes(done, 4) == 4 &&
        done[0] == 0xAA && done[1] == 0xF4) {
        Serial.println("[PICOTA] Done! Pico firmware written, rebooting...");
    } else {
        Serial.printf("[PICOTA] No done signal (got %02X %02X) — firmware may still be written\n",
                      done[0], done[1]);
    }

    // Чекаємо перезавантаження Pico і очищуємо буфер
    delay(1500);
    while (Serial2.available()) Serial2.read();

    return true;
}
