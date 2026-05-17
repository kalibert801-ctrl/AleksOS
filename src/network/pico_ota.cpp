// pico_ota.cpp — UART OTA update for Pico controller
//
// Протокол: детально описано в pico_controller.ino
// Serial2 = UART між ESP32 і Pico (PICO_RX_PIN=22, PICO_TX_PIN=27, 115200 baud)

#include "network/pico_ota.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino.h>

// ── CRC16-CCITT (polynomial 0x1021, init 0xFFFF) ─────────────────────────────
// Повинна збігатися з реалізацією на Pico (crc16_ota в .ino)
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
    if (*p == 'v' || *p == 'V') p++;          // пропускаємо 'v'
    int major = 0, minor = 0;
    while (*p >= '0' && *p <= '9') major = major * 10 + (*p++ - '0');
    if (*p == '.') p++;
    while (*p >= '0' && *p <= '9') minor = minor * 10 + (*p++ - '0');
    if (major == 0 && minor == 0) return -1;  // не вдалось розпарсити
    return major * 100 + minor;
}

// ── Запит версії від Pico ────────────────────────────────────────────────────
int picoOtaGetVersion() {
    // Чистимо буфер перед запитом
    delay(50);
    while (Serial2.available()) Serial2.read();

    // Надсилаємо команду VERSION: [0xAA][0x02][0x00][0x02]
    uint8_t cmd[4] = {0xAA, 0x02, 0x00, 0x02};
    Serial2.write(cmd, 4);

    // Чекаємо відповідь: [0xAA][0x56][ver_hi][ver_lo]
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

// ── Відправка пакету ESP32→Pico і очікування ACK ────────────────────────────
static bool sendAndWaitAck(uint8_t cmd, uint8_t data, uint8_t expectCmd,
                           uint32_t timeoutMs = 3000) {
    // Flush incoming
    while (Serial2.available()) Serial2.read();

    uint8_t pkt[4] = {0xAA, cmd, data, (uint8_t)(cmd ^ data)};
    Serial2.write(pkt, 4);

    uint8_t ack[4];
    Serial2.setTimeout(timeoutMs);
    if (Serial2.readBytes(ack, 4) != 4) return false;
    return (ack[0] == 0xAA && ack[1] == expectCmd);
}

// ── Основна функція OTA ───────────────────────────────────────────────────────
bool picoOtaUpdate(const char *binUrl, void (*progressCb)(int pct)) {
    const int PAGE = 256;

    Serial.printf("[PICOTA] URL: %s\n", binUrl);

    // ── Крок 1: тригер OTA на Pico ───────────────────────────────────────────
    // Pico може бути зайнятий надсиланням пакетів кнопок.
    // Чекаємо 100мс і чистимо буфер перед відправкою.
    delay(120);
    while (Serial2.available()) Serial2.read();

    if (!sendAndWaitAck(0xF0, 0x00, 0xF1, 3000)) {
        Serial.println("[PICOTA] Pico did not respond to OTA trigger");
        return false;
    }
    Serial.println("[PICOTA] Pico ready");

    // ── Крок 2: HTTP GET ──────────────────────────────────────────────────────
    WiFiClientSecure client;
    client.setInsecure();          // GitHub CDN, сертифікат не перевіряємо

    HTTPClient http;
    http.begin(client, binUrl);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    http.addHeader("User-Agent", "RetroESP-PicoOTA/1.0");
    http.addHeader("Cache-Control", "no-cache");

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

    // ── Крок 3: надсилаємо розмір Pico ──────────────────────────────────────
    {
        uint8_t szBuf[4] = {
            (uint8_t)( totalSize        & 0xFF),
            (uint8_t)((totalSize >> 8)  & 0xFF),
            (uint8_t)((totalSize >> 16) & 0xFF),
            (uint8_t)((totalSize >> 24) & 0xFF)
        };
        Serial2.write(szBuf, 4);

        // Чекаємо ACK розміру
        uint8_t sack[4];
        Serial2.setTimeout(3000);
        if (Serial2.readBytes(sack, 4) != 4 ||
            sack[0] != 0xAA || sack[1] != 0xF2) {
            Serial.println("[PICOTA] No size ACK");
            http.end();
            return false;
        }
    }
    Serial.println("[PICOTA] Size ACK. Starting pages...");

    // ── Крок 4: стримінг сторінок ─────────────────────────────────────────────
    WiFiClient &stream = http.getStream();
    int totalPages = (totalSize + PAGE - 1) / PAGE;
    int bytesRead  = 0;

    for (int p = 0; p < totalPages; p++) {
        // Заповнюємо сторінку (остання часткова → pad 0xFF = стерта flash)
        uint8_t page[PAGE];
        memset(page, 0xFF, PAGE);
        int toRead = min(PAGE, totalSize - bytesRead);

        // Читаємо toRead байт з HTTP стріму (може приходити кількома шматками)
        int got = 0;
        uint32_t pageDeadline = millis() + 8000;
        while (got < toRead) {
            int n = stream.readBytes(page + got, toRead - got);
            if (n > 0) { got += n; }
            else if (millis() > pageDeadline) {
                Serial.printf("[PICOTA] HTTP stream timeout at page %d\n", p);
                http.end();
                return false;
            }
        }
        bytesRead += toRead;

        // CRC16-CCITT над всіма 256 байтами (включно з padding)
        uint16_t crc = crc16(page, PAGE);

        // Відправляємо сторінку + CRC
        Serial2.write(page, PAGE);
        uint8_t crcBuf[2] = {(uint8_t)(crc & 0xFF), (uint8_t)(crc >> 8)};
        Serial2.write(crcBuf, 2);

        // Чекаємо ACK (timeout 3с — враховує час стирання сектора ~50мс)
        Serial2.setTimeout(3000);
        uint8_t pageAck[4];
        if (Serial2.readBytes(pageAck, 4) != 4 ||
            pageAck[0] != 0xAA || pageAck[1] != 0xF3) {
            Serial.printf("[PICOTA] Page %d NAK or timeout\n", p);
            http.end();
            return false;
        }

        if (progressCb) progressCb((p + 1) * 100 / totalPages);

        // Кожні 16 сторінок (1 сектор) → логуємо прогрес
        if ((p % 16) == 15 || p == totalPages - 1) {
            Serial.printf("[PICOTA] Page %d/%d  (%d%%)\n",
                          p + 1, totalPages, (p + 1) * 100 / totalPages);
        }
    }

    http.end();

    // ── Крок 5: чекаємо сигнал "done" від Pico ──────────────────────────────
    Serial2.setTimeout(5000);
    uint8_t done[4];
    if (Serial2.readBytes(done, 4) == 4 &&
        done[0] == 0xAA && done[1] == 0xF4) {
        Serial.println("[PICOTA] Done! Pico is rebooting...");
    } else {
        // Навіть без done-сигналу прошивка вже записана — вважаємо успіхом
        Serial.println("[PICOTA] Done (no done-pkt, firmware written anyway)");
    }

    delay(500); // чекаємо поки Pico перезавантажиться (~200мс)
    // Очищуємо Serial2 від можливого сміття після перезавантаження Pico
    delay(1000);
    while (Serial2.available()) Serial2.read();

    return true;
}
