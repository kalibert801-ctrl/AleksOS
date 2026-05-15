// RetroESP Pico Controller v4
// ПРОТОКОЛ (исправлен):
//   Pico→ESP32: [0xAA][0x42][btns][~btns]        — 4 байта, каждые 16мс
//   ESP32→Pico: [0xAA][cmd][data][cmd^data]       — 4 байта
//     0x01 = PING
//     0x20 = MOT1  data=duration×10ms (1-255, 0=стоп)
//     0x21 = MOT2  data=duration×10ms

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

#define BIT_A      0x01
#define BIT_B      0x02
#define BIT_SEL    0x04
#define BIT_STA    0x08
#define BIT_UP     0x10
#define BIT_DOWN   0x20
#define BIT_LEFT   0x40
#define BIT_RIGHT  0x80

// Моторы — простое управление длительностью
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

// Парсер входящих пакетов ESP32→Pico [0xAA][cmd][data][chk]
static uint8_t rxBuf[3]; 
static uint8_t rxIdx = 0;
static bool    inPkt = false;

void rxByte(uint8_t b) {
    if (!inPkt) {
        if (b == 0xAA) { inPkt = true; rxIdx = 0; }
        return;
    }
    rxBuf[rxIdx++] = b;
    if (rxIdx < 3) return;
    // Полный пакет: rxBuf[0]=cmd, rxBuf[1]=data, rxBuf[2]=checksum
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
        case 0x20: // MOT1
            if (data == 0) { analogWrite(PIN_MOT1, 0); mot1_end = 0; }
            else motorRun(1, data);
            break;
        case 0x21: // MOT2
            if (data == 0) { analogWrite(PIN_MOT2, 0); mot2_end = 0; }
            else motorRun(2, data);
            break;
    }
}

// Дебаунс
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

void setup() {
    Serial1.begin(115200);
    const int btns[] = {PIN_UP,PIN_DOWN,PIN_LEFT,PIN_RIGHT,
                        PIN_A,PIN_B,PIN_SELECT,PIN_START};
    for (int p : btns) pinMode(p, INPUT_PULLUP);
    pinMode(PIN_MOT1, OUTPUT); analogWrite(PIN_MOT1, 0);
    pinMode(PIN_MOT2, OUTPUT); analogWrite(PIN_MOT2, 0);
    pinMode(LED_PIN,  OUTPUT);
    // Старт-сигнал: оба мотора 80мс
    motorRun(1, 8); motorRun(2, 8);
}

void loop() {
    while (Serial1.available()) rxByte((uint8_t)Serial1.read());
    motorUpdate();

    static uint32_t lastSend = 0;
    if (millis() - lastSend >= 16) {
        uint8_t b = readButtons();
        uint8_t pkt[4] = {0xAA, 0x42, b, (uint8_t)(~b)};
        Serial1.write(pkt, 4);
        lastSend = millis();
        digitalWrite(LED_PIN, b != 0);
    }
}
