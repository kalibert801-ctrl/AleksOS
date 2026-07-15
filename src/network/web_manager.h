#pragma once

// webMgrStart() — запустить HTTP-сервер на порту 80 (полный файловый менеджер).
// webMgrStop()  — остановить сервер.
// webMgrHandle()— вызывать каждый loop() в состоянии S_WEBMGR.
// webMgrRunning()— true пока сервер запущен (в режиме файлового менеджера).
// webMgrIP()   — текстовый IP адрес.

void        webMgrStart();
void        webMgrStop();
void        webMgrHandle();
bool        webMgrRunning();
const char* webMgrIP();

// webDbgStart() — запустить минимальный HTTP сервер (только /console).
//   Требует активного WiFi. Работает в фоне во всех режимах.
//   Автоматически останавливается при запуске webMgrStart().
// webDbgStop()   — остановить debug-сервер.
// webDbgRunning()— true если debug-сервер запущен.
// webDbgHandle() — вызывать каждый loop() пока webDbgRunning().
// webDbgIP()    — IP адрес (пустая строка если не запущен).

bool        webDbgStart();
void        webDbgStop();
bool        webDbgRunning();
void        webDbgHandle();
const char* webDbgIP();
