#pragma once

// webMgrStart() — запустить HTTP-сервер на порту 80.
//   Требует активного WiFi. Рисует экран с IP адресом.
// webMgrStop()  — остановить сервер и освободить ресурсы.
// webMgrHandle()— вызывать каждый loop() в состоянии S_WEBMGR.
// webMgrRunning()— true пока сервер запущен.
// webMgrIP()   — текстовый IP адрес (пустая строка если не запущен).

void        webMgrStart();
void        webMgrStop();
void        webMgrHandle();
bool        webMgrRunning();
const char* webMgrIP();
