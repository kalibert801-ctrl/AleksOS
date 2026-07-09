#pragma once
// ── Версионирование ───────────────────────────────────────
// Правило: minor изменения = вторая цифра +1 (v8.4→v8.5)
//          major изменения = первая цифра +1, вторая = 0 (v8.x→v9.0)
#define FIRMWARE_VERSION "AleksOS BETA v15.08"

// ── Дисплей (VSPI) ────────────────────────────────────────
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_MISO 12
#define TFT_BL   21

// ── Тач XPT2046 ───────────────────────────────────────────
#define TOUCH_CLK  25
#define TOUCH_CS   33
#define TOUCH_DIN  32
#define TOUCH_OUT  39
#define TOUCH_IRQ  36

// ── SD карта (HSPI) ───────────────────────────────────────
#define SD_CS    5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18

// ── Периферия ─────────────────────────────────────────────
// RGB LED удалён — GPIO16 и GPIO17 заняты PSRAM (CS + CLK).
// На плату впаян одноцветный 1206 LED на GPIO4 (место среднего пина RGB).
#define LED_PIN       4     // единственный оставшийся LED
#define LED_R         LED_PIN   // псевдоним для совместимости с led.cpp
#define AUDIO_PIN    26
#define LIGHT_SENSOR 34

// ── Pico UART (Expansion IO2 connector, 4p 1.25mm) ────────
// Expansion IO2 распиновка (слева направо):
//   GND  |  IO22  |  IO27  |  3.3V
//
// Подключение Pico:
//   ESP32 IO22 ← Pico GP0 (Serial1 TX)   RX
//   ESP32 IO27 → Pico GP1 (Serial1 RX)   TX
//   ESP32 GND  ─  Pico GND
//   ESP32 3.3V →  Pico 3V3               питание логики
//
// Используем Serial2 (UART2) — ОТДЕЛЬНЫЙ от USB (UART0/GPIO1/GPIO3).
// Можно подключать Pico и USB одновременно без конфликтов.
#define PICO_RX_PIN   22    // IO22 ← Pico TX
#define PICO_TX_PIN   27    // IO27 → Pico RX
#define PICO_BAUD   115200

// Протокол Pico ↔ ESP32:
//   Pico→ESP32:  0xAA  0x42  <btns:1>  <chk:1>   chk = ~btns
//   ESP32→Pico:  0xAA  <cmd:1>  <data:1>  <chk:1>  chk = cmd^data
// Команды ESP32→Pico:
#define PICO_CMD_PING    0x01  // data=0  → Pico отвечает PONG (0x50)
#define PICO_CMD_VERSION 0x02  // data=0  → Pico отвечает [0xAA][0x56][ver_hi][ver_lo]
                               //           ver = MAJOR*100+MINOR  напр. v14.4 → 1404
#define PICO_CMD_LED     0x10  // data=RGB565 ниблы (будущее)
#define PICO_CMD_HAPTIC  0x20  // data=длительность×10мс (будущее)
#define PICO_BTN_PKT     0x42  // Pico→ESP32: байт состояния кнопок (8 игровых кнопок)
#define PICO_SYS_PKT     0x43  // Pico→ESP32: системные кнопки  [0xAA][0x43][sys][~sys]
                               //   sys бит 0 = HOME (GP14) — выход из эмулятора
#define PICO_CMD_OTA     0xF0  // data=0  → Pico входит в UART OTA режим

// ── Индексы физических кнопок в btnMap[] ─────────────────
// Порядок соответствует битам в протоколе Pico (бит i = 1<<i)
#define BTN_IDX_A     0   // бит 0 = 0x01
#define BTN_IDX_B     1   // бит 1 = 0x02
#define BTN_IDX_SEL   2   // бит 2 = 0x04
#define BTN_IDX_STA   3   // бит 3 = 0x08
#define BTN_IDX_UP    4   // бит 4 = 0x10
#define BTN_IDX_DOWN  5   // бит 5 = 0x20
#define BTN_IDX_LEFT  6   // бит 6 = 0x40
#define BTN_IDX_RIGHT 7   // бит 7 = 0x80
#define BTN_MAP_COUNT 8

// ── Дисплей ───────────────────────────────────────────────
#define SCREEN_W    320
#define SCREEN_H    240
#define SCREEN_ROT    1

// ── UI (settings screens — оригинальные размеры, не менять) ──
#define HDR_H        40
#define ROW_H        32
#define ROWS_VISIBLE  5
#define BTNBAR_H     44

// ── Компактное главное меню v2 (v14.72) ──────────────────────
#define M_HDR_H    30                      // высота шапки меню
#define M_BAR_H    40                      // высота подвала меню
#define M_ROW_H    24                      // высота строки (компактный список)
#define M_ROWS      7                      // видимых строк в списке
#define M_LIST_W  196                      // ширина панели списка (левая)
#define M_PANEL_X 196                      // X начала правой панели
#define M_PANEL_W (SCREEN_W - M_PANEL_X)  // 124 пикселя
#define M_DPAD_Y  (SCREEN_H - M_BAR_H)    // 200
#define M_DPAD_CY (M_DPAD_Y + M_BAR_H/2)  // 220
#define M_COVER_H  108                     // высота обложки в правой панели
#define M_INFO_Y  (M_HDR_H + M_COVER_H)   // 138 — начало инфо-секции
#define M_INFO_H  (M_DPAD_Y - M_INFO_Y)   // 62  — высота инфо-секции

// ── Кнопки (маски BTN_* = INP_PAD_* из nesinput.h) ───────
#define BTN_A       0x01
#define BTN_B       0x02
#define BTN_SEL     0x04
#define BTN_STA     0x08
#define BTN_UP      0x10
#define BTN_DOWN    0x20
#define BTN_LEFT    0x40
#define BTN_RIGHT   0x80

// ── Батарея (ADC1 — безопасен при WiFi) ──────────────────
// GPIO35: единственный свободный пин ADC1 (INPUT_ONLY).
// Схема: LiPo(+) → R_TOP=100кОм → GPIO35 → R_BOT=100кОм → GND
// Делитель × 2: 4.2V → 2.1V, 3.0V → 1.5V (в зоне ADC11db 0–3.3V).
// Если батарея не подключена — оставьте BAT_ADC_PIN = -1.
#define BAT_ADC_PIN   35      // -1 = батарея не подключена
#define BAT_R_TOP  100000     // верхний резистор делителя (Ом) — 100кОм
#define BAT_R_BOT  100000     // нижний резистор делителя (Ом)
// ВАЖНО: добавить 100нФ конденсатор между GPIO35 и GND!
// Без него S/H АЦП не успевает зарядиться (RC=50мкс >> время сэмплирования).
// Конденсатор фильтрует шум и держит напряжение стабильным между опросами.

// ── Пути ─────────────────────────────────────────────────
#define ROM_DIR   "/FomiCon"
#define CFG_FILE  "/config.json"
