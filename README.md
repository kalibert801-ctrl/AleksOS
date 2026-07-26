
# AleksOS — NES консоль на ESP32

**AleksOS** — портативная игровая консоль на базе **ESP32-2432S028** («Cheap Yellow Display»).  
NES-эмулятор **Nofrendo** с полностью переработанным интерфейсом, веб-менеджером, виброотдачей и поддержкой WiFi.
<img width="1100" height="2444" alt="IMG_20260726_133154414" src="https://github.com/user-attachments/assets/c72e5b01-b47e-4029-afbf-c507f819335b" />
<img width="1167" height="2592" alt="IMG_20260726_133100712_HDR" src="https://github.com/user-attachments/assets/b108dd02-263d-4322-aef2-74800967cb69" />
<img width="1198" height="2662" alt="IMG_20260726_133041177_HDR" src="https://github.com/user-attachments/assets/3d1222c4-d76a-432e-8064-ebd70ecd382d" />
<img width="1151" height="2557" alt="IMG_20260726_133021497_HDR" src="https://github.com/user-attachments/assets/806ac94c-bbb1-41b6-a92f-1b33cf85e1ea" />
<img width="1611" height="2147" alt="IMG_20260726_132854269_HDR" src="https://github.com/user-attachments/assets/48815d97-3e87-4beb-9c1e-743788e33950" />
<img width="2174" height="978" alt="IMG_20260724_225933957_HDR" src="https://github.com/user-attachments/assets/e9ead0a4-658e-4241-bf18-4d75c3697210" />
> ⚠️ Проект создан под конкретное модифицированное железо. Работа на других платах не гарантируется.

---

## Железо

| Компонент | Описание |
|-----------|---------|
| МК | ESP32 @ 240 МГц, 4 МБ Flash, 8 МБ PSRAM (мод) |
| Дисплей | ST7789 320×240 IPS, SPI DMA 80 МГц (LovyanGFX) |
| Звук | I2S ЦАП GPIO26, 44100 Гц, 4× передискретизация, усилитель SC8002B |
| Хранилище | MicroSD (SPI) |
| Контроллер | Raspberry Pi Pico по UART2 (GPIO22/27) |
| Вибро | Виброматор на Pico (GP10) |
| Тачскрин | XPT2046 резистивный |
| Подсветка | Авто-яркость, фоторезистор GPIO34 |
| Батарея | LiPo, делитель R=100кОм / GPIO35, TP4056 |

---

## Возможности

### Эмулятор NES
- 50+ FPS, звук 44100 Гц
- Поддержка популярных mapper'ов: NROM, MMC1, UNROM, CNROM, MMC3 и другие
- Исправленный IRQ в MMC3
- Game Genie и Game Shark коды
- Турбо-режим кнопок
- Переназначение кнопок (ремаппинг)
- Авто-сохранение позиции

### Медиа и библиотека
- Браузер ROM — сканирует SD карту, навигация кнопками и тачем
- Обложки игр (`.raw`, `.jpg`, `.png` в `/covers/`)
- Скриншоты на SD карту
- Встроенный MP3/WAV плеер

### Интерфейс
- Несколько тем оформления
- Часы и дата в подвале меню — NTP синхронизация при старте по WiFi
- Мониторинг батареи — процент заряда, напряжение, анимация зарядки, статистика
- Авто-яркость по датчику освещённости
- Вибро-отклик в меню
- Сон экрана по таймеру
- Сканлайны

### Веб-интерфейс (по WiFi)
- **Файловый менеджер** — загрузка, удаление, переименование файлов и папок
- **Аудиоплеер** — MP3/WAV прямо в браузере, без скачивания
- **Просмотр изображений** — JPG, PNG, BMP; скриншоты `.raw` через Canvas
- **Веб-консоль** — Serial-монитор в браузере (`/console`)
- **Удалённый экран** (`/remote`) — трансляция экрана + виртуальный геймпад

### OTA обновления
- Обновление прошивки по WiFi (GitHub/веб-менеджер)
- Обновление прошивки с SD карты (`/update/*.bin`) — выбор файла кнопками, PIN-защита

---

## Структура SD карты

```
/
├── FomiCon/        ← ROM файлы (.nes)
├── Music/          ← Музыка (.mp3, .wav)
├── sounds/         ← Звуки интерфейса (.wav)
├── covers/         ← Обложки игр (.raw, .jpg, .png)
├── update/         ← Прошивки для SD OTA (.bin)
├── Screenshots/    ← Скриншоты (создаётся автоматически)
└── config.json     ← Настройки (создаётся автоматически)
```

---

## Программный стек

- **Фреймворк:** Arduino (ESP-IDF via PlatformIO)
- **Дисплей:** LovyanGFX 1.x
- **Ядро эмулятора:** Nofrendo © 1998–2000 Matthew Conte
- **Конфиг:** ArduinoJSON на SD карте
- **NTP:** ip-api.com (часовой пояс по IP) + pool.ntp.org

---

## Модификация PSRAM

Плата модифицирована — добавлена внешняя микросхема PSRAM **APS6404L-3SQR-SN** (8 МБ QSPI).  
Используются GPIO16/17, которые на оригинальной плате заняты RGB-светодиодом — светодиод удалён.

Автор идеи: [hexeguitar](https://github.com/hexeguitar/ESP32_TFT_PIO/blob/main/README.md)

<img alt="cyd_PSRAM_mod" src="https://github.com/user-attachments/assets/13a14e45-822b-409a-b7bd-12c2a5f268f4"/>

**Подходящие микросхемы:**
- `APS6404L-3SQR-SN` — 8 МБ (рекомендуется)
- `APS1604M-3SQR-SN` — 2 МБ (минимум)

> ⚠️ **Только версия `3SQR`** — без `3` рассчитаны на 1.8V, на CYD 3.3V чип сгорит.

### Порядок установки PSRAM

При старте на линиях GPIO16/17 подаётся напряжение. Если припаять PSRAM до прошивки — чип сгорит.

1. Прошить ESP32
2. Припаять APS6404L-3SQR-SN
3. Включить питание

---

## Контроллер кнопок (Raspberry Pi Pico)

На ESP32-2432S028 нет свободных GPIO для кнопок — используется **Pico** как контроллер по UART.

**Подключение:**

| ESP32 | Pico | Назначение |
|-------|------|------------|
| IO22  | GP0 (TX) | Данные кнопок → ESP32 |
| IO27  | GP1 (RX) | Команды ESP32 → Pico |
| GND   | GND | Общая земля |
| 3.3V  | 3V3 | Питание Pico |

**Протокол:**
```
Pico → ESP32:  [0xAA] [0x42] [кнопки] [~кнопки]
ESP32 → Pico:  [0xAA] [cmd]  [data]   [cmd ^ data]
```

**Биты кнопок:** `START=0x01  SELECT=0x02  B=0x04  A=0x08  ↑=0x10  ↓=0x20  ←=0x40  →=0x80`

<img alt="pico connection" src="https://github.com/user-attachments/assets/a8ed13d6-e5b9-4065-87f8-9d7238d9c8a2"/>

### Прошивка Pico

Исходники — папка `pico_controller/`. Прошивается через Arduino IDE.

> ⚠️ **Перед прошивкой Pico отключить провода от ESP32 (GP0, GP1, 3V3, GND).**  
> USB питает Pico на 5V → через 3.3V на ESP32 → перегружает стабилизатор Pico.

1. Отключить Pico от ESP32
2. Подключить по USB к ПК
3. Открыть `pico_controller/` в Arduino IDE, плата "Raspberry Pi Pico"
4. Загрузить прошивку
5. Подключить обратно к ESP32

> При прошивке **ESP32** отключать Pico не нужно.

---

## Сборка и прошивка

```bash
# Установить PlatformIO
pip install platformio

# Клонировать и собрать
git clone https://github.com/kalibert801-ctrl/AleksOS.git
cd AleksOS
pio run

# Прошивка (зажать BOOT, затем:)
pio run --target upload
```

Готовый бинарник: `releases/firmware.bin` — прошивается через esptool или через `/update` в веб-менеджере.

---

## Лицензия

- Ядро **Nofrendo** — © 1998–2000 Matthew Conte — [GNU LGPL v2](https://www.gnu.org/licenses/old-licenses/lgpl-2.0.html)
- OSD, интерфейс, остальной код — **MIT**
- Все изменения ядра распространяются под GNU LGPL v2
