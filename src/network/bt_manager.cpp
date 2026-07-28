#include "network/bt_manager.h"
#include "config.h"
#include <NimBLEDevice.h>

// Nordic UART Service — widely supported by BLE serial apps on Android
#define NUS_SVC "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // phone WRITES here

static volatile uint8_t _pad = 0;
static bool _running   = false;
static bool _connected = false;

class _SrvCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*) override {
        _connected = true;
    }
    void onDisconnect(NimBLEServer* s) override {
        _connected = false;
        _pad = 0;
        s->startAdvertising();  // restart so next client can connect
    }
};

class _CharCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        if (c->getDataLength() >= 1) _pad = (uint8_t)c->getValue()[0];
    }
};

void btMgrStart() {
    if (_running) return;
    _pad = 0; _connected = false;

    NimBLEDevice::init(BT_DEVICE_NAME);
    NimBLEDevice::setSecurityAuth(false);  // no pairing/bonding required
    NimBLEServer* srv = NimBLEDevice::createServer();
    srv->setCallbacks(new _SrvCB());

    NimBLEService* svc = srv->createService(NUS_SVC);
    NimBLECharacteristic* ch = svc->createCharacteristic(
        NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    ch->setCallbacks(new _CharCB());
    svc->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SVC);
    adv->setScanResponse(true);
    NimBLEDevice::startAdvertising();
    _running = true;
}

void btMgrStop() {
    if (!_running) return;
    NimBLEDevice::deinit(true);
    _running = false; _connected = false; _pad = 0;
}

bool btMgrConnected() { return _running && _connected; }
uint8_t btMgrGetPad() { return _pad; }
