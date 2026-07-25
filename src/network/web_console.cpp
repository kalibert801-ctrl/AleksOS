// web_console.cpp — кольцевой буфер для Web Console.
//
// В ESP-IDF printf() идёт через newlib VFS → UART напрямую, минуя vprintf.
// Поэтому используем два механизма:
//   1. esp_log_set_vprintf — перехватывает ESP_LOGI/LOGD/LOGE и т.п.
//   2. webConsolePrintf()  — явная функция для Serial.printf → web console

#include "web_console.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "esp_log.h"
#include "driver/uart.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// ── Кольцевой буфер (8 KB в DRAM) ─────────────────────────────────────────────
static const size_t CON_BUF = 8192;
static char         _buf[CON_BUF];
static volatile size_t _total = 0;
static SemaphoreHandle_t _mux = nullptr;

static void _append(const char *s, size_t len) {
    if (!_mux || !len) return;
    if (xSemaphoreTake(_mux, pdMS_TO_TICKS(10)) != pdTRUE) return;
    for (size_t i = 0; i < len; i++)
        _buf[(_total + i) % CON_BUF] = s[i];
    _total += len;
    xSemaphoreGive(_mux);
}

// ── Хук для ESP_LOG* (ESP_LOGI, ESP_LOGD, ...) ────────────────────────────────
static int _espLogHook(const char *fmt, va_list args) {
    char tmp[512];
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    if (len <= 0) return len;
    size_t out = (size_t)len < sizeof(tmp) ? (size_t)len : sizeof(tmp) - 1;
    uart_write_bytes(UART_NUM_0, tmp, out);
    _append(tmp, out);
    return len;
}

// ── PUBLIC API ────────────────────────────────────────────────────────────────
void webConsoleInit() {
    _mux = xSemaphoreCreateMutex();
    esp_log_set_vprintf(_espLogHook);
}

// Явная функция для диагностики: пишет в UART + кольцевой буфер.
// Используй вместо Serial.printf() / printf() в местах где нужно видеть
// вывод в веб-консоли.
void webConsolePrintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[512];
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len <= 0) return;
    size_t out = (size_t)len < sizeof(tmp) ? (size_t)len : sizeof(tmp) - 1;
    uart_write_bytes(UART_NUM_0, tmp, out);
    _append(tmp, out);
}

size_t webConsolePos() {
    return _total;
}

size_t webConsoleRead(size_t fromPos, char *outBuf, size_t maxLen, size_t *newPos) {
    if (!maxLen || !outBuf || !_mux) { if (newPos) *newPos = fromPos; return 0; }

    if (xSemaphoreTake(_mux, pdMS_TO_TICKS(10)) != pdTRUE) {
        if (newPos) *newPos = fromPos;
        return 0;
    }

    size_t cur = _total;
    if (fromPos >= cur) {
        xSemaphoreGive(_mux);
        if (newPos) *newPos = cur;
        return 0;
    }

    size_t avail = cur - fromPos;
    if (avail > CON_BUF) { fromPos = cur - CON_BUF; avail = CON_BUF; }

    size_t toSend = avail < maxLen ? avail : maxLen;
    size_t start  = fromPos % CON_BUF;

    if (start + toSend <= CON_BUF) {
        memcpy(outBuf, _buf + start, toSend);
    } else {
        size_t first = CON_BUF - start;
        memcpy(outBuf,         _buf + start, first);
        memcpy(outBuf + first, _buf,          toSend - first);
    }

    xSemaphoreGive(_mux);
    if (newPos) *newPos = fromPos + toSend;
    return toSend;
}
