#pragma once
// ntp_manager.h — NTP time synchronisation via WiFi
//
// Порядок работы:
//   1. detectTimezoneByIP()  — GET http://ip-api.com/json/?fields=offset
//                              определяет UTC-смещение автоматически по IP
//   2. configTime()          — передаёт смещение в SNTP-стек ESP32
//   3. getLocalTime()        — ждёт синхронизации и обновляет settings + timeSet()

#include <Arduino.h>

// Синхронизировать время.
// Сначала определяет часовой пояс по IP, затем запрашивает NTP.
// Вызывать после WiFi.status() == WL_CONNECTED.
void ntpSync();

// true если последняя синхронизация прошла успешно.
bool ntpIsSynced();

// Возвращает UTC-смещение в секундах, определённое при последнем ntpSync().
// Например, UTC+3 → 10800.
long ntpGetUtcOffset();
