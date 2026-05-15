#include "display/display_manager.h"
#include "config.h"
#include "settings.h"

LGFX lcd;

void initDisplay() {
    lcd.init();
    lcd.setRotation(SCREEN_ROT);
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setTextDatum(MC_DATUM);
    setBrightness(80);
}

void setBrightness(uint8_t pct) {
    pct = constrain(pct, 10, 100);
    lcd.setBrightness(map(pct, 10, 100, 25, 255));
}

void updateAutoBrightness() {
    if (!settings.autoBrightness) return;
    static uint32_t lastRead = 0;
    static int      smoothed = -1;   // -1 = not initialized yet
    if (millis() - lastRead < 1500) return;  // check every 1.5s
    lastRead = millis();

    // GPIO34 (ADC1_CH6) — фоторезистор.
    // Делаем 4 замера и усредняем для стабильности.
    int sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += analogRead(LIGHT_SENSOR);
        delay(2);
    }
    int raw = sum / 4;   // 0 (bright) … 4095 (dark)

    // Экспоненциальное сглаживание (α = 0.25):
    // сглаживает резкие колебания, не тормозит реакцию
    if (smoothed < 0) smoothed = raw;
    smoothed = smoothed * 3/4 + raw * 1/4;

    // Полный диапазон: dark=4095→10%, bright=0→100%
    // Старый диапазон 20–100 обрезал нижнюю часть →
    // при небольшом затенении сразу падал до 70%.
    uint8_t target = (uint8_t)map(constrain(smoothed, 0, 4095), 4095, 0, 10, 100);

    // Обновляем только если изменение > 2% (гистерезис)
    if (abs((int)target - (int)settings.brightness) > 2) {
        settings.brightness = target;
        setBrightness(target);
    }
}
