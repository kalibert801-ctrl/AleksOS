// wifi_manager.cpp — WiFi connection management for RetroESP
#include "network/wifi_manager.h"
#include "settings.h"
#include <WiFi.h>

WiFiManager wifiMgr;

void WiFiManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);

    if (settings.wifiEnabled && settings.wifiSSID[0] != '\0') {
        Serial.printf("[WiFi] Auto-connecting to '%s'...\n", settings.wifiSSID);
        connect(settings.wifiSSID, settings.wifiPass, 10000);
    }
}

bool WiFiManager::connect(const char *ssid, const char *pass, uint32_t timeoutMs) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid, pass);

    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected — IP=%s RSSI=%d\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }
        delay(250);
    }
    Serial.println("[WiFi] Connection timeout.");
    WiFi.disconnect(true);
    return false;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    Serial.println("[WiFi] Disconnected.");
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getSSID() const {
    return WiFi.SSID();
}

int WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

int WiFiManager::scan() {
    WiFi.disconnect(true);
    delay(100);
    _scanCount = WiFi.scanNetworks(false, true);  // blocking, show hidden
    if (_scanCount < 0) _scanCount = 0;
    Serial.printf("[WiFi] Scan found %d networks.\n", _scanCount);
    return _scanCount;
}

String WiFiManager::getScanSSID(int i) const {
    if (i < 0 || i >= _scanCount) return "";
    return WiFi.SSID(i);
}

int WiFiManager::getScanRSSI(int i) const {
    if (i < 0 || i >= _scanCount) return -100;
    return WiFi.RSSI(i);
}

bool WiFiManager::getScanEncrypted(int i) const {
    if (i < 0 || i >= _scanCount) return false;
    return WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
}
