#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define SAMPLES  200
#define ADC_PIN  A0
#define WAVE_X   10
#define WAVE_Y   10
#define WAVE_W   220
#define WAVE_H   200

uint16_t buf[SAMPLES];
uint16_t prev[SAMPLES];
bool first = true;

void setup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // Рамка и сетка
  drawStatic();
}

void drawStatic() {
  // Рамка
  tft.drawRect(WAVE_X, WAVE_Y, WAVE_W, WAVE_H, TFT_WHITE);
  
  // Сетка
  for (int i = 1; i < 4; i++) {
    tft.drawFastHLine(WAVE_X, WAVE_Y + WAVE_H/4*i, WAVE_W, 0x2104);
    tft.drawFastVLine(WAVE_X + WAVE_W/4*i, WAVE_Y, WAVE_H, 0x2104);
  }
  
  // Подписи
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(250, 10);  tft.print("Vmx");
  tft.setCursor(250, 40);  tft.print("Vmn");
  tft.setCursor(250, 70);  tft.print("Vpp");
  tft.setCursor(250, 100); tft.print("V  ");
}

void loop() {
  // Сэмплирование
  uint16_t vmax = 0, vmin = 1023;
  for (int i = 0; i < SAMPLES; i++) {
    buf[i] = analogRead(ADC_PIN);
    if (buf[i] > vmax) vmax = buf[i];
    if (buf[i] < vmin) vmin = buf[i];
  }

  uint16_t range = vmax - vmin;
  if (range == 0) range = 1;

  // Рисуем волну
  for (int i = 0; i < WAVE_W; i++) {
    uint16_t val = buf[(long)i * SAMPLES / WAVE_W];
    int ny = WAVE_Y + WAVE_H - 1 - (long)(val - vmin) * (WAVE_H-1) / range;
    
    // Стираем старую
    if (!first) {
      int oy = prev[i];
      int opy = (i > 0) ? prev[i-1] : oy;
      int y1 = min(oy, opy);
      int y2 = max(oy, opy);
      for (int y = y1; y <= y2; y++) {
        // Восстанавливаем сетку
        uint16_t col = TFT_BLACK;
        if (y == WAVE_Y + WAVE_H/4)   col = 0x2104;
        if (y == WAVE_Y + WAVE_H/2)   col = 0x2104;
        if (y == WAVE_Y + WAVE_H*3/4) col = 0x2104;
        if (x_is_grid(i))             col = 0x2104;
        tft.drawPixel(WAVE_X + i, y, col);
      }
    }
    prev[i] = ny;
  }
  
  // Рисуем новую волну
  for (int i = 0; i < WAVE_W; i++) {
    int ny = prev[i];
    int py = (i > 0) ? prev[i-1] : ny;
    int y1 = min(ny, py);
    int y2 = max(ny, py);
    tft.drawFastVLine(WAVE_X + i, y1, y2-y1+1, TFT_GREEN);
  }
  first = false;

  // Напряжения
  float vmax_v = vmax * 5.0 / 1023.0;
  float vmin_v = vmin * 5.0 / 1023.0;
  float vpp_v  = vmax_v - vmin_v;
  float vmid_v = (vmax_v + vmin_v) / 2.0;

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(250, 25);  tft.print(vmax_v, 2); tft.print("V");
  tft.setCursor(250, 55);  tft.print(vmin_v, 2); tft.print("V");
  tft.setCursor(250, 85);  tft.print(vpp_v,  2); tft.print("V");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(250, 115); tft.print(vmid_v, 2); tft.print("V");
}

bool x_is_grid(int x) {
  return (x == WAVE_W/4 || x == WAVE_W/2 || x == WAVE_W*3/4);
}