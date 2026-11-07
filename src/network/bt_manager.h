#pragma once
#include <stdint.h>

void    btMgrReleaseMemory(bool keepBle);  // вызвать в setup() после cfgLoad(): память BT → в кучу
bool    btMgrAvailable();   // false — память BLE отдана при старте, для BT нужна перезагрузка
void    btMgrStart();       // запустить SPP сервер (вызывать перед emu_run)
void    btMgrStop();        // остановить SPP сервер (вызывать после emu_run)
bool    btMgrConnected();   // есть ли подключённый клиент
uint8_t btMgrGetPad();      // прочитать последнее состояние кнопок от BT клиента
