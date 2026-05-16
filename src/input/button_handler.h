#pragma once
#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════
// ButtonHandler — управление через Raspberry Pi Pico по UART
//
// ПОДКЛЮЧЕНИЕ (Expansion IO2 connector, 4p 1.25mm):
//   GND  ←→  GND
//   IO22 ←   GP0 (Serial1 TX)   кнопки → ESP32
//   IO27 →   GP1 (Serial1 RX)   команды ESP32 → Pico
//   3.3V →   3V3
//
// ВИБРО МОТОРЫ (подключены к Pico):
//   GP10 → NPN-транзистор → мотор 1 (отклик кнопок / UI)
//   GP11 → NPN-транзистор → мотор 2 (игровые события: смерть и т.д.)
//
// ПРОТОКОЛ:
//   Pico→ESP32:  [0xAA] [0x42] [btns]  [~btns]
//   ESP32→Pico:  [0xAA] [cmd]  [data]  [cmd^data]
//     0x01 = PING
//     0x20 = HAPTIC1  data=duration×10мс (0=стоп, 255=2550мс)
//     0x21 = HAPTIC2  data=duration×10мс
// ═══════════════════════════════════════════════════════════════════

// Команды вибро
#define PICO_CMD_HAPTIC1  0x20
#define PICO_CMD_HAPTIC2  0x21

class ButtonHandler {
public:
    void    init();
    uint8_t read();
    uint8_t readNew();
    uint8_t readCurrent() const { return _state; }  // текущее состояние (для авто-прокрутки)

    // Применяет таблицу переназначения кнопок (settings.btnMap) к произвольному
    // байту физического состояния. Бит i физ. кнопки → settings.btnMap[i].
    // Используется везде кроме экрана ремапа (там нужны сырые физ. кнопки).
    uint8_t applyBtnMap(uint8_t raw) const;
    bool    isConnected() const;
    void    sendCmd(uint8_t cmd, uint8_t data);
    void    sendCmd2(uint8_t cmd, uint8_t d1, uint8_t d2);
    void    update();

    // Вибро-отклик: duration_ms — длительность в мс (макс 2550)
    void vibrate1(uint16_t duration_ms);   // мотор 1 — UI / кнопки
    void vibrate2(uint16_t duration_ms);   // мотор 2 — игровые события

private:
    uint8_t  _state  = 0;
    uint8_t  _prev   = 0;
    uint8_t  _rxBuf[3] = {};
    uint8_t  _rxIdx  = 0;
    bool     _inPkt  = false;
    uint32_t _lastPkt = 0;

    void _processByte(uint8_t b);
    void _handlePacket();
};

extern ButtonHandler buttons;
