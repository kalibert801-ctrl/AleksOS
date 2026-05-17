// RetroESP Pico Controller v5  — з підтримкою UART OTA + VERSION + авто-вібро
//
// ── Версія прошивки Pico ──────────────────────────────────────────────────────
// Правило: при кожному оновленні міняй PICO_VER_MINOR (або MAJOR).
#define PICO_VER_MAJOR  5
#define PICO_VER_MINOR  2
// ─────────────────────────────────────────────────────────────────────────────
// ПРОТОКОЛ (звичайний режим):
//   Pico→ESP32: [0xAA][0x42][btns][~btns]       — 4 байти, кожні 16мс
//   ESP32→Pico: [0xAA][cmd][data][cmd^data]      — 4 байти
//     0x01 = PING
//     0x02 = VERSION   → відповідь [0xAA][0x56][hi][lo]
//     0x20 = MOT1      data=duration×10ms  (0=стоп)
//     0x21 = MOT2      data=duration×10ms  (0=стоп)
//     0x22 = BOTH      data=duration×10ms  — обидва мотори одночасно
//     0x23 = HAPTIC_EN data=0/1            — вмкн/вимкн авто-вібро від кнопок
//     0xF0 = OTA       data=0  → починаємо оновлення прошивки
//
// ПРОТОКОЛ OTA (після команди 0xF0):
//   ESP32→Pico: [0xAA][0xF0][0x00][0xF0]           тригер
//   Pico→ESP32: [0xAA][0xF1][0x00][0xF1]           готовий
//   ESP32→Pico: [sz0][sz1][sz2][sz3]               розмір .bin (LE uint32)
//   Pico→ESP32: [0xAA][0xF2][0x00][0xF2]           розмір отримано
//   For each 256-byte page:
//     ESP32→Pico: [256 байт даних][crc_lo][crc_hi] CRC16-CCITT
//     Pico→ESP32: [0xAA][0xF3][page_lo][0xF3^page_lo]  OK
//              or [0xAA][0xFE][0xFF][0x01]              CRC помилка → abort
//   Pico→ESP32: [0xAA][0xF4][0x00][0xF4]           готово, перезавантаження

// ── Pico SDK заголовки (доступні в RP2040 Arduino core Earle Philhower) ──────
#include <hardware/flash.h>    // flash_range_erase / flash_range_program (в RAM)
#include <hardware/sync.h>     // save_and_disable_interrupts / restore_interrupts
#include <hardware/uart.h>     // uart0_hw — прямий доступ до регістрів UART0

// ── Піни ─────────────────────────────────────────────────────────────────────
#define PIN_UP     2
#define PIN_DOWN   3
#define PIN_LEFT   4
#define PIN_RIGHT  5
#define PIN_A      6
#define PIN_B      7
#define PIN_SELECT 8
#define PIN_START  9
#define PIN_MOT1   10
#define PIN_MOT2   11
#define LED_PIN    25

// ── Параметри авто-вібро від кнопок ──────────────────────────────────────────
// Тривалість вібро при натисканні кнопки (в одиницях 10мс)
// 3 = 30мс — короткий відклик (можна змінити від 1 до 25)
#define BTN_HAPTIC_DUR   3

// ── Біти кнопок ──────────────────────────────────────────────────────────────
#define BIT_A      0x01
#define BIT_B      0x02
#define BIT_SEL    0x04
#define BIT_STA    0x08
#define BIT_UP     0x10
#define BIT_DOWN   0x20
#define BIT_LEFT   0x40
#define BIT_RIGHT  0x80

// ═══════════════════════════════════════════════════════════════════════════════
// OTA: CRC16-CCITT — ОБОВ'ЯЗКОВО в RAM
// ═══════════════════════════════════════════════════════════════════════════════
static uint16_t __no_inline_not_in_flash_func(crc16_ota)(const uint8_t *data, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

// ═══════════════════════════════════════════════════════════════════════════════
// OTA: ОСНОВНА ФУНКЦІЯ ЗАПИСУ — виконується ТІЛЬКИ з RAM
// ═══════════════════════════════════════════════════════════════════════════════
static void __no_inline_not_in_flash_func(picoOtaRun)(uint32_t totalSize) {
    #define OTA_RX() ({                                         \
        while (uart0_hw->fr & UART_UARTFR_RXFE_BITS);          \
        (uint8_t)(uart0_hw->dr & 0xFF);                        \
    })
    #define OTA_TX(b) do {                                      \
        while (uart0_hw->fr & UART_UARTFR_TXFF_BITS);          \
        uart0_hw->dr = (uint8_t)(b);                           \
    } while(0)

    const uint32_t PAGE = FLASH_PAGE_SIZE;    // 256 байт
    const uint32_t SECT = FLASH_SECTOR_SIZE;  // 4096 байт (16 сторінок)

    uint8_t  page[PAGE];
    uint32_t offset     = 0;
    uint32_t totalPages = (totalSize + PAGE - 1) / PAGE;

    for (uint32_t p = 0; p < totalPages; p++, offset += PAGE) {
        for (uint32_t i = 0; i < PAGE; i++) page[i] = OTA_RX();

        uint8_t  cl = OTA_RX(), ch = OTA_RX();
        uint16_t rxCrc   = (uint16_t)cl | ((uint16_t)ch << 8);
        uint16_t calcCrc = crc16_ota(page, (int)PAGE);

        if (rxCrc != calcCrc) {
            OTA_TX(0xAA); OTA_TX(0xFE); OTA_TX(0xFF); OTA_TX(0x01);
            while (!(uart0_hw->fr & UART_UARTFR_TXFE_BITS));
            *((volatile uint32_t*)0xE000ED0C) = 0x5FA0004UL;
            while (1);
        }

        if ((offset % SECT) == 0) {
            uint32_t ints = save_and_disable_interrupts();
            flash_range_erase(offset, SECT);
            restore_interrupts(ints);
        }

        {
            uint32_t ints = save_and_disable_interrupts();
            flash_range_program(offset, page, PAGE);
            restore_interrupts(ints);
        }

        uint8_t p_lo = (uint8_t)(p & 0xFF);
        OTA_TX(0xAA); OTA_TX(0xF3); OTA_TX(p_lo); OTA_TX(0xF3 ^ p_lo);
    }

    OTA_TX(0xAA); OTA_TX(0xF4); OTA_TX(0x00); OTA_TX(0xF4);
    while (!(uart0_hw->fr & UART_UARTFR_TXFE_BITS));
    for (volatile uint32_t i = 0; i < 300000UL; i++);
    *((volatile uint32_t*)0xE000ED0C) = 0x5FA0004UL;
    while (1);

    #undef OTA_RX
    #undef OTA_TX
}

// ═══════════════════════════════════════════════════════════════════════════════
// Мотори
// ═══════════════════════════════════════════════════════════════════════════════
static uint32_t mot1_end = 0, mot2_end = 0;

void motorUpdate() {
    uint32_t now = millis();
    if (mot1_end && now >= mot1_end) { analogWrite(PIN_MOT1, 0); mot1_end = 0; }
    if (mot2_end && now >= mot2_end) { analogWrite(PIN_MOT2, 0); mot2_end = 0; }
}

void motorRun(int n, uint8_t dur10ms) {
    uint32_t ms = (uint32_t)dur10ms * 10;
    if (n == 1) { mot1_end = millis() + ms; analogWrite(PIN_MOT1, 255); }
    else        { mot2_end = millis() + ms; analogWrite(PIN_MOT2, 255); }
}

void motorRunBoth(uint8_t dur10ms) {
    motorRun(1, dur10ms);
    motorRun(2, dur10ms);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Парсер вхідних пакетів ESP32→Pico [0xAA][cmd][data][chk]
// ═══════════════════════════════════════════════════════════════════════════════
static uint8_t rxBuf[3];
static uint8_t rxIdx  = 0;
static bool    inPkt  = false;

// Авто-вібро від кнопок: можна вимкнути командою 0x23 data=0
static bool    hapticEnabled = true;

void rxByte(uint8_t b) {
    if (!inPkt) {
        if (b == 0xAA) { inPkt = true; rxIdx = 0; }
        return;
    }
    rxBuf[rxIdx++] = b;
    if (rxIdx < 3) return;

    inPkt = false; rxIdx = 0;
    if (rxBuf[2] != (uint8_t)(rxBuf[0] ^ rxBuf[1])) return; // bad checksum

    uint8_t cmd  = rxBuf[0];
    uint8_t data = rxBuf[1];

    switch (cmd) {
        case 0x01: { // PING → PONG
            uint8_t p[4] = {0xAA, 0x50, 0x00, 0x50};
            Serial1.write(p, 4);
            break;
        }
        case 0x02: { // VERSION → [0xAA][0x56][ver_hi][ver_lo]
            uint16_t v = (uint16_t)PICO_VER_MAJOR * 100 + PICO_VER_MINOR;
            uint8_t r[4] = {0xAA, 0x56,
                            (uint8_t)(v >> 8),
                            (uint8_t)(v & 0xFF)};
            Serial1.write(r, 4);
            break;
        }
        case 0x20: // MOT1
            if (data == 0) { analogWrite(PIN_MOT1, 0); mot1_end = 0; }
            else motorRun(1, data);
            break;
        case 0x21: // MOT2
            if (data == 0) { analogWrite(PIN_MOT2, 0); mot2_end = 0; }
            else motorRun(2, data);
            break;
        case 0x22: // BOTH — обидва мотори одночасно (тач, UI)
            if (data == 0) {
                analogWrite(PIN_MOT1, 0); mot1_end = 0;
                analogWrite(PIN_MOT2, 0); mot2_end = 0;
            } else {
                motorRunBoth(data);
            }
            break;
        case 0x23: // HAPTIC_EN — вмкн/вимкн авто-вібро від кнопок
            hapticEnabled = (data != 0);
            break;

        case 0xF0: { // OTA — оновлення прошивки
            uint8_t ack[4] = {0xAA, 0xF1, 0x00, 0xF1};
            Serial1.write(ack, 4);
            Serial1.flush();

            Serial1.setTimeout(5000);
            uint8_t szBuf[4] = {0,0,0,0};
            Serial1.readBytes(szBuf, 4);
            uint32_t fwSize = (uint32_t)szBuf[0]
                            | ((uint32_t)szBuf[1] << 8)
                            | ((uint32_t)szBuf[2] << 16)
                            | ((uint32_t)szBuf[3] << 24);

            uint8_t sack[4] = {0xAA, 0xF2, 0x00, 0xF2};
            Serial1.write(sack, 4);
            Serial1.flush();

            picoOtaRun(fwSize);
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Дебаунс кнопок
// ═══════════════════════════════════════════════════════════════════════════════
static uint8_t lastRaw = 0, stable = 0;
static uint32_t lastChange = 0;

uint8_t readButtons() {
    uint8_t raw = 0;
    if (!digitalRead(PIN_A))      raw |= BIT_A;
    if (!digitalRead(PIN_B))      raw |= BIT_B;
    if (!digitalRead(PIN_SELECT)) raw |= BIT_SEL;
    if (!digitalRead(PIN_START))  raw |= BIT_STA;
    if (!digitalRead(PIN_UP))     raw |= BIT_UP;
    if (!digitalRead(PIN_DOWN))   raw |= BIT_DOWN;
    if (!digitalRead(PIN_LEFT))   raw |= BIT_LEFT;
    if (!digitalRead(PIN_RIGHT))  raw |= BIT_RIGHT;
    if (raw != lastRaw) { lastRaw = raw; lastChange = millis(); }
    if (millis() - lastChange >= 8) stable = raw;
    return stable;
}

// ═══════════════════════════════════════════════════════════════════════════════
// setup / loop
// ═══════════════════════════════════════════════════════════════════════════════
void setup() {
    Serial1.begin(115200);
    const int btns[] = {PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT,
                        PIN_A, PIN_B, PIN_SELECT, PIN_START};
    for (int p : btns) pinMode(p, INPUT_PULLUP);
    pinMode(PIN_MOT1, OUTPUT); analogWrite(PIN_MOT1, 0);
    pinMode(PIN_MOT2, OUTPUT); analogWrite(PIN_MOT2, 0);
    pinMode(LED_PIN,  OUTPUT);
    // Старт-сигнал: обидва мотори 80мс
    motorRunBoth(8);
}

void loop() {
    while (Serial1.available()) rxByte((uint8_t)Serial1.read());
    motorUpdate();

    static uint32_t lastSend = 0;
    static uint8_t  prevBtn  = 0;   // для визначення нових натискань

    if (millis() - lastSend >= 16) {
        uint8_t b = readButtons();

        // ── Авто-вібро при натисканні кнопок ─────────────────────────────────
        // Спрацьовує локально на Pico — не потребує зворотньої команди від ESP32.
        // Працює і в емуляторі, і в оболочці, де ESP32 зайнята і не може
        // відправити команду вібро назад.
        if (hapticEnabled) {
            uint8_t newPress = b & ~prevBtn;  // біти які щойно стали натисненими
            if (newPress) {
                motorRunBoth(BTN_HAPTIC_DUR); // обидва мотори одночасно
            }
        }
        prevBtn = b;
        // ─────────────────────────────────────────────────────────────────────

        uint8_t pkt[4] = {0xAA, 0x42, b, (uint8_t)(~b)};
        Serial1.write(pkt, 4);
        lastSend = millis();
        digitalWrite(LED_PIN, b != 0);
    }
}
