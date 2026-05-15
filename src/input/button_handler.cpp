#include "input/button_handler.h"
#include "settings.h"

ButtonHandler buttons;

// Serial2 = UART2 ESP32, аппаратно отдельный от USB (UART0).
// Можно одновременно иметь USB подключённый для отладки.
#define PICO_SERIAL Serial2

void ButtonHandler::init() {
    PICO_SERIAL.begin(PICO_BAUD, SERIAL_8N1, PICO_RX_PIN, PICO_TX_PIN);
    _state = _prev = 0;
    _rxIdx = 0;
    _inPkt = false;
    _lastPkt = 0;
    Serial.printf("[BTN] Serial2 started: RX=GPIO%d TX=GPIO%d baud=%d\n",
                  PICO_RX_PIN, PICO_TX_PIN, PICO_BAUD);
}

void ButtonHandler::_processByte(uint8_t b) {
    if (!_inPkt) {
        if (b == 0xAA) { _inPkt = true; _rxIdx = 0; }
        return;
    }
    _rxBuf[_rxIdx++] = b;
    if (_rxIdx >= 3) {
        _handlePacket();
        _inPkt = false;
        _rxIdx = 0;
    }
}

void ButtonHandler::_handlePacket() {
    uint8_t type = _rxBuf[0];
    uint8_t data = _rxBuf[1];
    uint8_t chk  = _rxBuf[2];

    switch (type) {
        case PICO_BTN_PKT:               // 0x42 — кнопки
            if (chk == (uint8_t)(~data)) {
                _state   = data;
                _lastPkt = millis();
            }
            break;
        case 0x50:                       // PONG
            _lastPkt = millis();
            break;
        default:
            break;
    }
}

void ButtonHandler::update() {
    int avail = PICO_SERIAL.available();
    while (avail-- > 0)
        _processByte((uint8_t)PICO_SERIAL.read());
}

uint8_t ButtonHandler::read()    { update(); return _state; }

uint8_t ButtonHandler::readNew() {
    update();
    uint8_t newly = _state & ~_prev;
    _prev = _state;
    return newly;
}

bool ButtonHandler::isConnected() const {
    return _lastPkt > 0 && (millis() - _lastPkt) < 500;
}

void ButtonHandler::sendCmd(uint8_t cmd, uint8_t data) {
    // Protocol: [0xAA][cmd][data][cmd^data] — matches Pico v4 firmware
    uint8_t pkt[4] = { 0xAA, cmd, data, (uint8_t)(cmd ^ data) };
    PICO_SERIAL.write(pkt, 4);
}

void ButtonHandler::sendCmd2(uint8_t cmd, uint8_t d1, uint8_t d2) {
    // Unused — kept for compatibility, routes to sendCmd with d1 only
    sendCmd(cmd, d1);
    (void)d2;
}

// ── Вибро ────────────────────────────────────────────────────────
void ButtonHandler::vibrate1(uint16_t duration_ms) {
    if (!isConnected() || !settings.vibroEnabled) return;
    uint8_t dur = (uint8_t)min((int)(duration_ms / 10), 255);
    if (dur == 0) dur = 1;
    sendCmd(PICO_CMD_HAPTIC1, dur);
}

void ButtonHandler::vibrate2(uint16_t duration_ms) {
    if (!isConnected() || !settings.vibroEnabled) return;
    uint8_t dur = (uint8_t)min((int)(duration_ms / 10), 255);
    if (dur == 0) dur = 1;
    sendCmd(PICO_CMD_HAPTIC2, dur);
}
