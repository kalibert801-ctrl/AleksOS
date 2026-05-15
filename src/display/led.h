#pragma once
#include <Arduino.h>

enum LedState {
    LED_OFF,
    LED_GREEN,   // всё OK
    LED_RED,     // ошибка
    LED_BLUE,    // игра запускается
    LED_YELLOW,  // настройки
    LED_BOOT,    // анимация при старте
};

void ledInit();
void ledSet(LedState s);
void ledUpdate();   // вызывать в loop() — управляет анимацией
