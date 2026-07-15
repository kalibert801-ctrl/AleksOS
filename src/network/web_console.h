#pragma once
#include <stddef.h>

// Инициализация: перехват ESP_LOG* + создание буфера.
// Вызвать один раз после Serial.begin().
void webConsoleInit();

// Диагностический printf: пишет в UART и в веб-консоль одновременно.
// Используй вместо Serial.printf() / printf() в диагностических блоках.
void webConsolePrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Получить данные из буфера начиная с позиции fromPos.
// Возвращает количество скопированных байт; *newPos = fromPos + скопировано.
size_t webConsoleRead(size_t fromPos, char *outBuf, size_t maxLen, size_t *newPos);

// Текущая позиция (сколько всего байт записано в буфер с начала работы).
size_t webConsolePos();
