#pragma once
// pico_ota.h — OTA firmware update for Pico over UART
//
// Схема роботи:
//   1. ESP32 надсилає тригер 0xF0 по Serial2 (UART → Pico)
//   2. Pico входить в RAM-режим (picoOtaRun) і чекає бінарник
//   3. ESP32 качає pico_firmware.bin з GitHub і стримінгом надсилає по 256 байт
//   4. Кожна сторінка підтверджується CRC16-CCITT
//   5. Після останньої сторінки Pico скидається і завантажує нову прошивку

#include <Arduino.h>

// Завантажити pico_firmware.bin з URL і прошити Pico через Serial2.
// binUrl — пряме посилання на .bin файл (GitHub CDN URL).
// progressCb — виклик з 0..100 для відображення прогресу (може бути nullptr).
// Повертає true якщо всі сторінки записані успішно (Pico перезавантажується сам).
bool picoOtaUpdate(const char *binUrl, void (*progressCb)(int pct));

// Побудувати URL для pico_firmware.bin з тегу релізу.
// tag — наприклад "v14.4". Результат у статичному буфері (overwritten кожен раз).
const char *picoOtaBuildUrl(const char *tag);

// Запитати поточну версію прошивки Pico через UART (команда 0x02).
// Повертає MAJOR*100 + MINOR  (наприклад v14.4 → 1404).
// Повертає -1 якщо Pico не відповів (не підключений або стара прошивка).
int picoOtaGetVersion();

// Розпарсити тег GitHub релізу в число MAJOR*100+MINOR.
// "v14.5" → 1405,  "v9.0" → 900,  "14.3" → 1403.
// Повертає -1 при помилці парсингу.
int picoOtaParseTag(const char *tag);
