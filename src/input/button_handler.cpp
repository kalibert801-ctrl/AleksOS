#include "input/button_handler.h"
#include "settings.h"
#include "network/web_console.h"

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
                if (data != _state && settings.diagButtons) {
                    webConsolePrintf("[BTN] 0x%02X%s%s%s%s%s%s%s%s%s\n", data,
                        (data & 0x01) ? " STA"     : "",
                        (data & 0x02) ? " SEL"     : "",
                        (data & 0x04) ? " A"       : "",
                        (data & 0x08) ? " B"       : "",
                        (data & 0x10) ? " UP"      : "",
                        (data & 0x20) ? " DOWN"    : "",
                        (data & 0x40) ? " LEFT"    : "",
                        (data & 0x80) ? " RIGHT"   : "",
                        (data == 0)   ? " release" : "");
                }
                _state   = data;
                _lastPkt = millis();
            }
            break;
        case PICO_SYS_PKT:               // 0x43 — системные кнопки (HOME и т.д.)
            if (chk == (uint8_t)(~data)) {
                _sysState = data;
                _lastPkt  = millis();
                if (settings.diagButtons && data) {
                    webConsolePrintf("[BTN] SYS=0x%02X%s\n", data,
                                    (data & BTN_SYS_HOME) ? " HOME" : "");
                }
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

uint8_t ButtonHandler::read() {
    update();
    uint8_t web = 0;
    if (_webMask) {
        if (millis() < _webExpMs) web = _webMask;
        else                      _webMask = 0;
    }
    return _state | web;
}

void ButtonHandler::webInject(uint8_t mask) {
    _webMask  = mask;
    _webExpMs = millis() + 500;
}

uint8_t ButtonHandler::readNew() {
    uint8_t cur = read(); // includes web inject
    uint8_t newly = cur & ~_prev;
    _prev = cur;
    return newly;
}

uint8_t ButtonHandler::applyBtnMap(uint8_t raw) const {
    // Каждый физический бит i заменяется на settings.btnMap[i].
    // settings.btnMap[i] == 0x00 означает «кнопка отключена».
    uint8_t out = 0;
    for (int i = 0; i < 8; i++)
        if (raw & (1 << i)) out |= settings.btnMap[i];
    return out;
}

bool ButtonHandler::isConnected() const {
    return _lastPkt > 0 && (millis() - _lastPkt) < 500;
}

void ButtonHandler::sendCmd(uint8_t cmd, uint8_t data) {
    // Protocol: [0xAA][cmd][data][cmd^data] — matches Pico v4 firmware
    uint8_t pkt[4] = { 0xAA, cmd, data, (uint8_t)(cmd ^ data) };
    PICO_SERIAL.write(pkt, 4);
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

void ButtonHandler::vibrateBoth(uint16_t duration_ms) {
    if (!isConnected() || !settings.vibroEnabled) return;
    uint8_t dur = (uint8_t)min((int)(duration_ms / 10), 255);
    if (dur == 0) dur = 1;
    sendCmd(PICO_CMD_HAPTIC_B, dur);  // 0x22 — оба мотора одновременно
}

void ButtonHandler::picoHapticEnable(bool en) {
    if (!isConnected()) return;
    sendCmd(PICO_CMD_HAPTIC_EN, en ? 1 : 0);  // 0x23
}
