// RetroESP Pico Controller v5 — UART OTA + VERSION + авто-вібро
//
// ── Версія прошивки Pico ──────────────────────────────────────────────────────
#define PICO_VER_MAJOR  5
#define PICO_VER_MINOR  7
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
// ПРОТОКОЛ OTA (двофазний — без конфлікту з Arduino UART interrupt):
//   Фаза 1 (прийом у SRAM через Serial1):
//     ESP32→Pico: [0xAA][0xF0][0x00][0xF0]           тригер
//     Pico→ESP32: [0xAA][0xF1][0x00][0xF1]           готовий
//     ESP32→Pico: [sz0][sz1][sz2][sz3]               розмір .bin (LE uint32)
//     Pico→ESP32: [0xAA][0xF2][0x00][0xF2]           розмір отримано
//     For each 256-byte page:
//       ESP32→Pico: [256 байт даних][crc_lo][crc_hi] CRC16-CCITT
//       Pico→ESP32: [0xAA][0xF3][page_lo][0xF3^page_lo]  OK
//                or [0xAA][0xFE][0xFF][0x01]              CRC помилка → abort
//   Фаза 2 (запис SRAM→Flash, тільки RAM-код):
//     Pico→ESP32: [0xAA][0xF4][0x00][0xF4]           готово, перезавантаження
//
// ЧОМУ двофазний:
//   Старий підхід: OTA_RX() читає з uart0_hw->dr (hardware FIFO).
//   Але Arduino UART interrupt перехоплює байти з FIFO у software buffer.
//   OTA_RX() бачить порожній FIFO → нескінченний spin → Pico зависає назавжди.
//
//   Новий підхід: Serial1.readBytes() читає з software buffer (через interrupt).
//   Немає конфлікту. Всі байти потрапляють туди куди треба.

#include <hardware/flash.h>   // flash_range_erase / flash_range_program
#include <hardware/sync.h>    // save_and_disable_interrupts / restore_interrupts
#include <hardware/uart.h>    // uart0_hw — тільки для TX в picoOtaFlash

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
#define LED_PIN    25

// ── Параметри авто-вібро від кнопок ──────────────────────────────────────────
#define BTN_HAPTIC_DUR   3   // 30мс (×10мс)

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
// OTA: SRAM буфер для прийому прошивки (Фаза 1)
// 120KB — з запасом для будь-якого розміру прошивки Pico
// Завжди в SRAM (.bss), не займає flash
// ═══════════════════════════════════════════════════════════════════════════════
static uint8_t _otaBuf[120 * 1024];

// ═══════════════════════════════════════════════════════════════════════════════
// CRC16-CCITT (poly=0x1021, init=0xFFFF) — в RAM для безпеки
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
// OTA Фаза 2: запис SRAM буфера у flash + сигнал "готово" + ARM reset
//
// СЕКЦІЯ .scratch_x (SRAM4, 0x20040000) — ГАРАНТОВАНО у SRAM:
//   .scratch_x — це фізично ОКРЕМИЙ банк пам'яті (не striped SRAM).
//   Flash операції НІКОЛИ не торкаються SRAM4.
//   На відміну від .time_critical (залежить від toolchain), .scratch_x
//   завжди в SRAM у будь-якому компіляторі для RP2040.
//
// Ця функція:
//   - Надсилає F8 "flash started" до початку будь-яких flash операцій
//   - Стирає та перезаписує весь flash з SRAM буфера
//   - Надсилає F4 "done" після завершення
//   - Виконує ARM reset
// ═══════════════════════════════════════════════════════════════════════════════
void __attribute__((section(".scratch_x.picoOtaFlash"), noinline))
picoOtaFlash(const uint8_t *buf, uint32_t totalSize) {

    // Макрос TX — прямий запис у hardware UART TX FIFO (не потребує flash-коду)
    #define OTA_TX(b) do {                                       \
        while (uart0_hw->fr & UART_UARTFR_TXFF_BITS);           \
        uart0_hw->dr = (uint8_t)(b);                            \
    } while(0)

    // Сигнал [0xAA][0xF8]: "Flash phase started"
    // Надсилається ДО будь-яких flash операцій.
    // Якщо ESP32 отримає F8 але не F4 — значить flash операція впала.
    // Якщо ESP32 не отримає навіть F8 — функція не виконалась.
    OTA_TX(0xAA); OTA_TX(0xF8); OTA_TX(0x00); OTA_TX(0xF8);

    const uint32_t PAGE = FLASH_PAGE_SIZE;     // 256 байт
    const uint32_t SECT = FLASH_SECTOR_SIZE;   // 4096 байт

    uint32_t totalPages = (totalSize + PAGE - 1) / PAGE;
    uint32_t offset = 0;

    // ── КРИТИЧНО: вимикаємо переривання ОДИН РАЗ для всього циклу ────────────
    // ПОМИЛКА якщо робити per-operation disable/enable:
    //   flash_range_erase(0, SECT)  → сектор 0 стертий, boot2 + вектор таблиця
    //                                 за адресою 0x10000100 стає 0xFF
    //   restore_interrupts()        → SysTick вмикається
    //   SysTick читає вектор 0x10000100 = 0xFFFFFFFF → Hard Fault
    //   flash_range_program() вже не виконується → boot2 стертий → BOOTSEL
    //
    // Рішення: один disable перед циклом, один restore ПІСЛЯ передачі F4.
    // 376 сторінок × (erase ~50мс/сектор + program ~1мс/стор) ≈ 1.5с — прийнятно.
    uint32_t ints = save_and_disable_interrupts();

    for (uint32_t p = 0; p < totalPages; p++, offset += PAGE) {
        if ((offset % SECT) == 0) {
            flash_range_erase(offset, SECT);
        }
        flash_range_program(offset, buf + p * PAGE, PAGE);
    }

    // Сигнал [0xAA][0xF4]: "Done, rebooting"
    // Надсилаємо поки переривання ще вимкнені: нова прошивка вже в flash,
    // але ми не хочемо щоб нові обробники переривань конфліктували з UART.
    OTA_TX(0xAA); OTA_TX(0xF4); OTA_TX(0x00); OTA_TX(0xF4);

    // Чекаємо поки TX FIFO та shift register порожні (байти точно передані)
    while (!(uart0_hw->fr & UART_UARTFR_TXFE_BITS));
    while (uart0_hw->fr & UART_UARTFR_BUSY_BITS);

    restore_interrupts(ints);

    // Пауза ~16мс — щоб ESP32 гарантовано обробив сигнал до reset
    for (volatile uint32_t i = 0; i < 2000000UL; i++);

    // ARM Cortex-M System Reset (SCB AIRCR, завжди доступний — регістр ARM)
    *((volatile uint32_t*)0xE000ED0C) = 0x5FA0004UL;
    while (1);

    #undef OTA_TX
}

// ═══════════════════════════════════════════════════════════════════════════════
// Мотори
// ═══════════════════════════════════════════════════════════════════════════════
static uint32_t mot1_end = 0;

void motorUpdate() {
    uint32_t now = millis();
    if (mot1_end && now >= mot1_end) { analogWrite(PIN_MOT1, 0); mot1_end = 0; }
}

void motorRun(uint8_t dur10ms) {
    uint32_t ms = (uint32_t)dur10ms * 10;
    mot1_end = millis() + ms;
    analogWrite(PIN_MOT1, 255);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Парсер вхідних пакетів ESP32→Pico [0xAA][cmd][data][chk]
// ═══════════════════════════════════════════════════════════════════════════════
static uint8_t rxBuf[3];
static uint8_t rxIdx = 0;
static bool    inPkt = false;

static bool hapticEnabled = true;  // авто-вібро від кнопок (команда 0x23)

void rxByte(uint8_t b) {
    if (!inPkt) {
        if (b == 0xAA) { inPkt = true; rxIdx = 0; }
        return;
    }
    rxBuf[rxIdx++] = b;
    if (rxIdx < 3) return;

    inPkt = false; rxIdx = 0;
    if (rxBuf[2] != (uint8_t)(rxBuf[0] ^ rxBuf[1])) return;

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
            uint8_t r[4] = {0xAA, 0x56, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF)};
            Serial1.write(r, 4);
            break;
        }

        case 0x20: // MOT1
        case 0x21: // (MOT2 removed — mapped to MOT1)
        case 0x22: // BOTH — mapped to single motor
            if (data == 0) { analogWrite(PIN_MOT1, 0); mot1_end = 0; }
            else motorRun(data);
            break;

        case 0x23: // HAPTIC_EN
            hapticEnabled = (data != 0);
            break;

        case 0xF0: {
            // ════════════════════════════════════════════════════════════════
            // OTA Фаза 1: прийом прошивки через Serial1 (Arduino UART, SRAM buffer)
            // Serial1 використовує UART interrupt → software buffer → readBytes()
            // Немає конфлікту з hardware FIFO. Надійно на будь-якій версії.
            // ════════════════════════════════════════════════════════════════

            // F1 ACK: готовий до прийому
            { uint8_t f1[4] = {0xAA, 0xF1, 0x00, 0xF1}; Serial1.write(f1, 4); Serial1.flush(); }

            // Приймаємо розмір прошивки (4 байти LE, без заголовку)
            Serial1.setTimeout(30000);
            uint8_t szBuf[4] = {0, 0, 0, 0};
            Serial1.readBytes(szBuf, 4);
            uint32_t fwSize = (uint32_t)szBuf[0]
                            | ((uint32_t)szBuf[1] << 8)
                            | ((uint32_t)szBuf[2] << 16)
                            | ((uint32_t)szBuf[3] << 24);

            // Перевіряємо чи вміщується прошивка у буфер
            if (fwSize == 0 || fwSize > sizeof(_otaBuf)) {
                uint8_t err[4] = {0xAA, 0xFE, 0xEE, (uint8_t)(0xFE ^ 0xEE)};
                Serial1.write(err, 4); Serial1.flush();
                break;
            }

            // F2 ACK: розмір отримано, починаємо сторінки
            { uint8_t f2[4] = {0xAA, 0xF2, 0x00, 0xF2}; Serial1.write(f2, 4); Serial1.flush(); }

            // Приймаємо сторінки у SRAM буфер
            uint32_t totalPages = (fwSize + 255) / 256;
            bool     receiveOk  = true;

            for (uint32_t p = 0; p < totalPages && receiveOk; p++) {
                uint8_t page[256];

                // Читаємо 256 байт сторінки (timeout 5с на перший байт)
                Serial1.setTimeout(5000);
                if (Serial1.readBytes(page, 256) != 256) {
                    receiveOk = false; break;
                }

                // Читаємо 2 байти CRC
                uint8_t crcb[2] = {0, 0};
                if (Serial1.readBytes(crcb, 2) != 2) {
                    receiveOk = false; break;
                }

                // Перевіряємо CRC
                uint16_t rxCrc   = (uint16_t)crcb[0] | ((uint16_t)crcb[1] << 8);
                uint16_t calcCrc = crc16_ota(page, 256);
                if (rxCrc != calcCrc) {
                    uint8_t err[4] = {0xAA, 0xFE, 0xFF, 0x01};
                    Serial1.write(err, 4); Serial1.flush();
                    receiveOk = false; break;
                }

                // Копіюємо сторінку у SRAM буфер
                memcpy(_otaBuf + p * 256, page, 256);

                // Page ACK: [0xAA][0xF3][page_lo][0xF3^page_lo]
                uint8_t p_lo = (uint8_t)(p & 0xFF);
                uint8_t ack[4] = {0xAA, 0xF3, p_lo, (uint8_t)(0xF3 ^ p_lo)};
                Serial1.write(ack, 4); Serial1.flush();
            }

            if (receiveOk) {
                // ════════════════════════════════════════════════════════════
                // OTA Фаза 2: записуємо SRAM буфер у flash
                // picoOtaFlash — виконується тільки з RAM.
                // Сама надсилає [0xAA][0xF4] і робить ARM reset.
                // ════════════════════════════════════════════════════════════
                picoOtaFlash(_otaBuf, fwSize);  // ніколи не повертається
            }

            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Дебаунс кнопок
// ═══════════════════════════════════════════════════════════════════════════════
static uint8_t  lastRaw    = 0;
static uint8_t  stable     = 0;
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
    pinMode(LED_PIN,  OUTPUT);
    // Старт-сигнал: вібро 80мс
    motorRun(8);
}

void loop() {
    while (Serial1.available()) rxByte((uint8_t)Serial1.read());
    motorUpdate();

    static uint32_t lastSend = 0;
    static uint8_t  prevBtn  = 0;

    if (millis() - lastSend >= 16) {
        uint8_t b = readButtons();

        // Авто-вібро при натисканні кнопок (локально на Pico — без ESP32)
        if (hapticEnabled) {
            uint8_t newPress = b & ~prevBtn;
            if (newPress) motorRun(BTN_HAPTIC_DUR);
        }
        prevBtn = b;

        uint8_t pkt[4] = {0xAA, 0x42, b, (uint8_t)(~b)};
        Serial1.write(pkt, 4);
        lastSend = millis();
        digitalWrite(LED_PIN, b != 0);
    }
}
