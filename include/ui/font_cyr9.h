#pragma once
/*  font_cyr9.h — Кириллический шрифт 5×7 на базе DejaVu9.
    Покрывает ASCII 0x20-0x7E + Кириллица 0x0410-0x044F (А-Я а-я).
    Определён в src/ui/font_cyr9.cpp.
    Использование: lcd.setFont(&CyrDejaVu9);                          */

#ifdef __cplusplus
#include <lgfx/v1/lgfx_fonts.hpp>

extern const lgfx::GFXfont CyrDejaVu9;

#endif /* __cplusplus */
