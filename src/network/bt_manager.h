#pragma once
#include <stdint.h>

void    btMgrStart();       // запустить SPP сервер (вызывать перед emu_run)
void    btMgrStop();        // остановить SPP сервер (вызывать после emu_run)
bool    btMgrConnected();   // есть ли подключённый клиент
uint8_t btMgrGetPad();      // прочитать последнее состояние кнопок от BT клиента
