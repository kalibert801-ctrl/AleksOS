// gb_runner.cpp — Game Boy emulator wrapper (Peanut-GB)
//
// Требует: src/emulator/peanut_gb.h
// Скачать: https://github.com/nicowillis/peanut-gb/raw/master/peanut_gb.h
// или:     https://raw.githubusercontent.com/deltabeard/Peanut-GB/master/peanut_gb.h
//
// ROM: .gb (DMG) и .gbc (Color в DMG-режиме)
// Экран: 160×144 → 320×240 (Bresenham: 2× по X, 1.666× по Y)
// Звук: отключён (добавить позже)
// Сохранения: battery RAM → <rom>.sav | save state → <rom>.gbst

#define PEANUT_GB_HIGH_LCD_ACCURACY 0
#define ENABLE_SOUND                0
#define ENABLE_SOUND_MINIGB         0

#include "gb_runner.h"
#include "peanut_gb.h"

#include "display/display_manager.h"
#include "input/button_handler.h"
#include "settings.h"
#include "config.h"
#include <SD.h>
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>

// ── ROM / RAM буферы (PSRAM) ─────────────────────────────────────────────────
static uint8_t *_romBuf  = nullptr;
static uint8_t *_ramBuf  = nullptr;
static size_t   _ramSize = 0;

// ── Рендер ───────────────────────────────────────────────────────────────────
// Стратегия: во время gb_run_frame() пишем в PSRAM-framebuffer (быстро).
// После кадра — один lcd.pushImage(0,0,320,240) = один DMA-burst (15ms).
// Раньше было 240 отдельных pushImage(320×1) с накладными SPI-расходами.
//
// Масштабирование 160×144 → 320×240:
//   X: ×2 (дублируем каждый пиксель)
//   Y: Bresenham 5:3 (144 строки → 240 строк, каждые 3 → 5)

static uint16_t *_frameBuf = nullptr;  // 320×240×2 = 150 KB в PSRAM
static uint16_t  _rowBuf[320];         // временный буфер одной строки (internal RAM)
static int       _scaleErr = 0;
static int       _screenY  = 0;

// DMG-палитра: классический зеленоватый стиль оригинального Game Boy
static const uint16_t _gbPal[4] = {
    0x9F20,  // светлый  (светло-зелёный)
    0x6B20,  // средний
    0x2980,  // тёмный
    0x0000,  // чёрный
};

static void gb_lcd_draw_line_cb(struct gb_s *gb, const uint8_t pixels[160], uint_fast8_t line) {
    (void)gb;
    if (line == 0) { _screenY = 0; _scaleErr = 0; }

    // Строим строку 320px в internal-RAM (быстро, без PSRAM latency)
    for (int x = 0; x < 160; x++) {
        uint16_t col = _gbPal[pixels[x] & 3];
        _rowBuf[x * 2]     = col;
        _rowBuf[x * 2 + 1] = col;
    }

    // Bresenham по Y: каждые 3 строки GB → 5 строк экрана
    _scaleErr += 240;
    while (_scaleErr >= 144) {
        if (_screenY < SCREEN_H && _frameBuf)
            memcpy(_frameBuf + _screenY * 320, _rowBuf, 320 * sizeof(uint16_t));
        _screenY++;
        _scaleErr -= 144;
    }
}

// ── Peanut-GB callbacks ──────────────────────────────────────────────────────
static uint8_t gb_rom_read_cb(struct gb_s *gb, uint_fast32_t addr) {
    (void)gb; return _romBuf[addr];
}
static uint8_t gb_cart_ram_read_cb(struct gb_s *gb, uint_fast32_t addr) {
    (void)gb; return _ramSize ? _ramBuf[addr % _ramSize] : 0xFF;
}
static void gb_cart_ram_write_cb(struct gb_s *gb, uint_fast32_t addr, uint8_t val) {
    (void)gb; if (_ramSize) _ramBuf[addr % _ramSize] = val;
}
static void gb_error_cb(struct gb_s *gb, enum gb_error_e err, uint16_t val) {
    (void)gb; (void)val;
    Serial.printf("[GB] error %d\n", (int)err);
}

// ── Battery RAM ──────────────────────────────────────────────────────────────
static String _savPath;

static void gbSaveRAM() {
    if (!_ramSize || !_ramBuf) return;
    File f = SD.open(_savPath.c_str(), FILE_WRITE);
    if (!f) return;
    f.write(_ramBuf, _ramSize);
    f.close();
    Serial.printf("[GB] sav: %s\n", _savPath.c_str());
}

static void gbLoadRAM() {
    if (!_ramSize || !_ramBuf) return;
    File f = SD.open(_savPath.c_str(), FILE_READ);
    if (!f) return;
    f.read(_ramBuf, _ramSize);
    f.close();
    Serial.printf("[GB] sav loaded: %s\n", _savPath.c_str());
}

// ── Save States ──────────────────────────────────────────────────────────────
static String _stPath;

static void gbSaveState(struct gb_s *gb) {
    File f = SD.open(_stPath.c_str(), FILE_WRITE);
    if (!f) { Serial.println("[GB] state write failed"); return; }
    f.write((const uint8_t*)gb, sizeof(*gb));
    if (_ramSize && _ramBuf) f.write(_ramBuf, _ramSize);
    f.close();
    Serial.printf("[GB] state saved: %s\n", _stPath.c_str());
}

static bool gbLoadState(struct gb_s *gb) {
    File f = SD.open(_stPath.c_str(), FILE_READ);
    if (!f) return false;
    if (f.read((uint8_t*)gb, sizeof(*gb)) != sizeof(*gb)) { f.close(); return false; }
    // Восстанавливаем указатели (function pointers не сохраняются)
    gb->gb_rom_read         = gb_rom_read_cb;
    gb->gb_cart_ram_read    = gb_cart_ram_read_cb;
    gb->gb_cart_ram_write   = gb_cart_ram_write_cb;
    gb->gb_error            = gb_error_cb;
    gb_init_lcd(gb, gb_lcd_draw_line_cb);
    if (_ramSize && _ramBuf) f.read(_ramBuf, _ramSize);
    f.close();
    Serial.printf("[GB] state loaded: %s\n", _stPath.c_str());
    return true;
}

// ── Скриншот ─────────────────────────────────────────────────────────────────
static void gbScreenshot(const char *baseName) {
    SD.mkdir("/Screenshots");
    char path[80];
    int n = 0;
    do { snprintf(path, sizeof(path), "/Screenshots/%s_%03d.raw", baseName, n++); }
    while (SD.exists(path) && n < 1000);

    File f = SD.open(path, FILE_WRITE);
    if (!f) return;
    uint16_t w = SCREEN_W, h = SCREEN_H;
    f.write((uint8_t*)&w, 2);
    f.write((uint8_t*)&h, 2);
    static uint16_t row[320];
    for (int y = 0; y < SCREEN_H; y++) {
        lcd.readRect(0, y, SCREEN_W, 1, row);
        f.write((uint8_t*)row, SCREEN_W * 2);
    }
    f.close();
    Serial.printf("[GB] screenshot: %s\n", path);
}

// ── In-game меню (аналог NES _ingameMenu) ────────────────────────────────────
static bool _gbExitReq = false;

static void gbIngameMenu(struct gb_s *gb, const char *baseName) {
    const int MX = 35, MY = 50, MW = 250, MH = 140;
    lcd.fillRect(MX, MY, MW, MH, 0x0820);
    lcd.drawRoundRect(MX, MY, MW, MH, 8, 0xAD55);
    lcd.drawRoundRect(MX+1, MY+1, MW-2, MH-2, 7, 0x630C);

    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(0xFD20);
    lcd.drawString("|| PAUSE ||", SCREEN_W/2, MY+14);
    lcd.drawFastHLine(MX+8, MY+24, MW-16, 0xAD55);

    const char *items[] = { "Continue", "Save State", "Load State", "Screenshot", "Exit to Menu" };
    const int N = 5;
    int sel = 0;

    auto drawItems = [&]() {
        for (int i = 0; i < N; i++) {
            int iy = MY + 32 + i * 20;
            bool s = (i == sel);
            lcd.fillRect(MX+2, iy, MW-4, 19, s ? 0x001F : 0x0820);
            lcd.setFont(&lgfx::fonts::DejaVu9);
            lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(s ? 0xFFFF : 0xC618);
            lcd.drawString(items[i], SCREEN_W/2, iy+9);
        }
    };
    drawItems();

    // Debounce: ждём отпускания HOME
    { uint32_t to = millis()+3000;
      while ((buttons.readSysCurrent() & BTN_SYS_HOME) && millis()<to) {
          buttons.update(); vTaskDelay(pdMS_TO_TICKS(16)); }
      vTaskDelay(pdMS_TO_TICKS(80)); }

    auto showIcon = [&](bool ok, int shape) {
        const int icx = SCREEN_W/2, icy = MY+MH+24;
        uint16_t col = ok ? (uint16_t)0x07E0 : (uint16_t)0xF800;
        lcd.fillCircle(icx, icy, 16, 0x0000);
        lcd.fillCircle(icx, icy, 14, col);
        if (shape == 0) {                             // дискета
            lcd.fillRect(icx-6, icy-6, 12, 12, 0xFFFF);
            lcd.fillRect(icx-4, icy-6, 8, 5, 0x39C7);
            lcd.fillRect(icx-1, icy, 4, 5, 0x39C7);
        } else if (shape == 1) {                      // камера
            lcd.fillRoundRect(icx-7, icy-2, 14, 9, 1, 0xFFFF);
            lcd.fillRect(icx-3, icy-5, 6, 4, 0xFFFF);
            lcd.fillCircle(icx, icy+2, 3, col);
            lcd.fillCircle(icx, icy+2, 1, 0xFFFF);
        } else {                                      // X
            lcd.drawLine(icx-5,icy-5,icx+5,icy+5,0xFFFF);
            lcd.drawLine(icx-5,icy+5,icx+5,icy-5,0xFFFF);
            lcd.drawLine(icx-4,icy-5,icx+5,icy+4,0xFFFF);
            lcd.drawLine(icx-5,icy-4,icx+4,icy+5,0xFFFF);
        }
        delay(900);
    };

    bool homePrev = false;
    uint8_t prevPad = 0xFF;
    while (true) {
        buttons.update();
        uint8_t curPad = buttons.applyBtnMap(buttons.readCurrent());
        uint8_t newBtn = curPad & ~prevPad;
        prevPad = curPad;
        bool homeNow = (buttons.readSysCurrent() & BTN_SYS_HOME) != 0;

        if (newBtn & BTN_UP)   { sel = (sel-1+N)%N; drawItems(); }
        if (newBtn & BTN_DOWN) { sel = (sel+1)%N;   drawItems(); }

        bool confirm = (newBtn & (BTN_STA | BTN_SEL));
        if (newBtn & BTN_B) { sel = 0; confirm = true; }  // B → Continue

        if (confirm) {
            switch (sel) {
                case 0: break;
                case 1: gbSaveState(gb);              showIcon(true,  0); break;
                case 2: { bool ok=gbLoadState(gb);    showIcon(ok, ok?0:2); break; }
                case 3: gbScreenshot(baseName);        showIcon(true,  1); break;
                case 4: _gbExitReq = true;             return;
            }
            return;
        }
        if (!homePrev && homeNow) return;
        homePrev = homeNow;
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

// ── Главная точка входа ───────────────────────────────────────────────────────
void gb_run(const char *romPath) {
    _gbExitReq = false;

    // Загружаем ROM в PSRAM
    File romFile = SD.open(romPath, FILE_READ);
    if (!romFile) { Serial.printf("[GB] open failed: %s\n", romPath); return; }
    size_t romSize = romFile.size();
    _romBuf = (uint8_t*)heap_caps_malloc(romSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_romBuf) {
        Serial.printf("[GB] ROM alloc failed %u KB\n", (unsigned)(romSize/1024));
        romFile.close(); return;
    }
    romFile.read(_romBuf, romSize);
    romFile.close();
    Serial.printf("[GB] ROM: %s  %u KB\n", romPath, (unsigned)(romSize/1024));

    // Пути для sav и state
    String pathStr = String(romPath);
    int dotIdx   = pathStr.lastIndexOf('.');
    int slashIdx = pathStr.lastIndexOf('/');
    String base = (dotIdx > 0) ? pathStr.substring(0, dotIdx) : pathStr;
    _savPath = base + ".sav";
    _stPath  = base + ".gbst";

    // Базовое имя файла для скриншотов (без папки и расширения)
    String baseName = (slashIdx >= 0 && dotIdx > slashIdx)
        ? pathStr.substring(slashIdx+1, dotIdx)
        : pathStr;

    // Инициализируем Peanut-GB
    struct gb_s *gb = (struct gb_s*)heap_caps_malloc(sizeof(struct gb_s),
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!gb) {
        Serial.println("[GB] gb_s alloc failed");
        heap_caps_free(_romBuf); _romBuf = nullptr; return;
    }

    enum gb_init_error_e initErr = gb_init(gb,
        gb_rom_read_cb, gb_cart_ram_read_cb, gb_cart_ram_write_cb, gb_error_cb, nullptr);
    if (initErr != GB_INIT_NO_ERROR) {
        Serial.printf("[GB] init error %d\n", (int)initErr);
        heap_caps_free(gb); heap_caps_free(_romBuf); _romBuf = nullptr; return;
    }
    gb_init_lcd(gb, gb_lcd_draw_line_cb);

    // Cart RAM (battery save)
    { size_t tmp = 0; gb_get_save_size_s(gb, &tmp); _ramSize = tmp; }
    _ramBuf  = nullptr;
    if (_ramSize > 0) {
        _ramBuf = (uint8_t*)heap_caps_malloc(_ramSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (_ramBuf) { memset(_ramBuf, 0xFF, _ramSize); gbLoadRAM(); }
    }

    // Framebuffer 320×240 в PSRAM — заполняется в колбэке, пушится одним DMA
    _frameBuf = (uint16_t*)heap_caps_malloc(320 * 240 * sizeof(uint16_t),
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_frameBuf) {
        Serial.println("[GB] framebuf alloc failed");
        // Продолжаем без framebuffer — будет медленнее, но работоспособно
    }
    memset(_frameBuf ? (void*)_frameBuf : (void*)_rowBuf, 0,
           _frameBuf ? 320*240*2 : 0);

    Serial.printf("[GB] RAM %u KB  fb=%s  heap=%u KB\n",
                  (unsigned)(_ramSize/1024), _frameBuf ? "ok" : "FAIL",
                  (unsigned)(ESP.getFreeHeap()/1024));

    // ── Главный цикл ─────────────────────────────────────────────────────────
    uint8_t homeFrames = 0;
    bool    homePrev   = false;
    const uint32_t FRAME_US = 16742;  // ~59.73 fps

    // Комбо выхода: SELECT+START удерживать ~3 с
    uint32_t exitComboMs = 0;

    while (!_gbExitReq) {
        uint32_t t0 = micros();

        buttons.update();
        esp_task_wdt_reset();

        uint8_t curPad = buttons.applyBtnMap(buttons.readCurrent());

        // Кормим GB джойпад (0=нажата, 1=отпущена)
        gb->direct.joypad_bits.a      = !(curPad & BTN_A);
        gb->direct.joypad_bits.b      = !(curPad & BTN_B);
        gb->direct.joypad_bits.select = !(curPad & BTN_SEL);
        gb->direct.joypad_bits.start  = !(curPad & BTN_STA);
        gb->direct.joypad_bits.up     = !(curPad & BTN_UP);
        gb->direct.joypad_bits.down   = !(curPad & BTN_DOWN);
        gb->direct.joypad_bits.left   = !(curPad & BTN_LEFT);
        gb->direct.joypad_bits.right  = !(curPad & BTN_RIGHT);

        // SELECT+START ~3с → выход (как в NES)
        if ((curPad & (BTN_SEL | BTN_STA)) == (BTN_SEL | BTN_STA)) {
            if (exitComboMs == 0) exitComboMs = millis();
            else if (millis() - exitComboMs > 3000) { _gbExitReq = true; break; }
        } else {
            exitComboMs = 0;
        }

        // HOME 3 кадра → меню
        bool homeNow = (buttons.readSysCurrent() & BTN_SYS_HOME) != 0;
        if (homeNow) { if (++homeFrames >= 3) { homeFrames = 0; gbIngameMenu(gb, baseName.c_str()); } }
        else homeFrames = 0;
        homePrev = homeNow;

        if (!_gbExitReq) {
            gb_run_frame(gb);  // заполняет _frameBuf через колбэк

            // Один DMA push всего кадра (vs 240 отдельных)
            if (_frameBuf)
                lcd.pushImage(0, 0, SCREEN_W, SCREEN_H, _frameBuf);
        }

        // Выдерживаем 60 fps
        uint32_t elapsed = micros() - t0;
        if (elapsed < FRAME_US) delayMicroseconds(FRAME_US - elapsed);
    }

    // Сохраняем battery RAM и освобождаем память
    gbSaveRAM();
    if (_frameBuf) { heap_caps_free(_frameBuf); _frameBuf = nullptr; }
    if (_ramBuf)   { heap_caps_free(_ramBuf);   _ramBuf   = nullptr; }
    _ramSize = 0;
    heap_caps_free(gb);
    heap_caps_free(_romBuf); _romBuf = nullptr;

    Serial.printf("[GB] exited  heap=%u KB\n", (unsigned)(ESP.getFreeHeap()/1024));
}
