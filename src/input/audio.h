#pragma once
#include <Arduino.h>
#include "settings.h"

void audioInit();
void audioUpdate();     // вызывать каждый loop()

// Типы звуков
void soundClick();      // обычный тап
void soundSelect();     // выбор/запуск
void soundBack();       // назад
void soundError();      // ошибка
void soundOK();         // успех (загрузка SD и т.д.)

// Music player API
void soundPlayPath(const char *path);  // воспроизвести произвольный WAV-файл
void soundStop();                      // остановить текущее воспроизведение
bool audioIsBusy();                    // true пока идёт воспроизведение
