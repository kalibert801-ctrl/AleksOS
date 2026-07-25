// sdmgr.cpp — общий файловый браузер SD-карты
// Два режима: список файлов + просмотрщик текста (.cfg/.txt/.json/.ini)

#include "ui/sdmgr.h"
#include "display/display_manager.h"
#include "ui/font_cyr9.h"
#include "ui/ui.h"
#include "settings.h"
#include "lang.h"
#include <SD.h>
#include <Arduino.h>

// ── Геометрия ─────────────────────────────────────────────────
#define SDM_HDR_H   30
#define SDM_BAR_H   40
#define SDM_ROW_H   28
#define SDM_LIST_Y  SDM_HDR_H
#define SDM_LIST_H  (SCREEN_H - SDM_HDR_H - SDM_BAR_H)   // 170
#define SDM_ROWS    (SDM_LIST_H / SDM_ROW_H)               // 6

// ── Данные ────────────────────────────────────────────────────
// Большие буферы хранятся в PSRAM, только указатели в DRAM
#define SDM_MAX      24
#define SDM_NLEN     24
#define SDM_VBUF_SZ  512

static char     _sdmDir[80]  = "/";
static char    *_sdmNames    = nullptr;  // PSRAM: SDM_MAX * SDM_NLEN
static bool    *_sdmIsDir    = nullptr;  // PSRAM: SDM_MAX
static char    *_sdmVBuf     = nullptr;  // PSRAM: SDM_VBUF_SZ
static char     _sdmVName[16] = "";      // короткое имя файла
static int16_t  _sdmTotal  = 0;
static int16_t  _sdmOffset = 0;
static int16_t  _sdmSel    = 0;
static bool     _sdmView   = false;

static inline char* sdmName(int i) { return _sdmNames + i * SDM_NLEN; }

static void sdmAlloc() {
    if (!_sdmNames) _sdmNames = (char*)heap_caps_malloc(SDM_MAX * SDM_NLEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_sdmIsDir) _sdmIsDir = (bool*)heap_caps_malloc(SDM_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_sdmVBuf)  _sdmVBuf  = (char*)heap_caps_malloc(SDM_VBUF_SZ, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

// ── Шрифты ───────────────────────────────────────────────────
static void sdFsm() { lcd.setFont(settings.language==LANG_RU ? &CyrDejaVu9 : &lgfx::fonts::DejaVu9); }
static void sdFmd() { lcd.setFont(settings.language==LANG_RU ? &CyrDejaVu9 : &lgfx::fonts::DejaVu12); }
static void sdFlg() { lcd.setFont(&lgfx::fonts::DejaVu18); }

// ── Сканирование директории ───────────────────────────────────
static void sdmScan() {
    _sdmTotal = 0;
    // ".." если не в корне
    if (strcmp(_sdmDir, "/") != 0) {
        strncpy(sdmName(_sdmTotal), "..", SDM_NLEN-1);
        sdmName(_sdmTotal)[SDM_NLEN-1] = '\0';
        _sdmIsDir[_sdmTotal] = true;
        _sdmTotal++;
    }
    File dir = SD.open(_sdmDir);
    if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
    while (_sdmTotal < SDM_MAX) {
        File f = dir.openNextFile();
        if (!f) break;
        String nm = String(f.name());
        int sl = nm.lastIndexOf('/');
        if (sl >= 0) nm = nm.substring(sl+1);
        bool isD = f.isDirectory();
        f.close();
        if (nm.isEmpty() || nm[0] == '.') continue;
        strncpy(sdmName(_sdmTotal), nm.c_str(), SDM_NLEN-1);
        sdmName(_sdmTotal)[SDM_NLEN-1] = '\0';
        _sdmIsDir[_sdmTotal] = isD;
        _sdmTotal++;
    }
    dir.close();
}

// ── Отрисовка ─────────────────────────────────────────────────
static void sdmDrawHeader() {
    const Theme565 &t = getTheme();
    lcd.fillRect(0,0,SCREEN_W,SDM_HDR_H,t.header);
    lcd.drawFastHLine(0,SDM_HDR_H-1,SCREEN_W,t.accent);
    sdFlg(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(0xFD20u);
    lcd.drawString("SD", 6, SDM_HDR_H/2);
    // Путь (усечённый)
    const char* d = _sdmDir;
    int dlen = strlen(d);
    char dispPath[30]; strncpy(dispPath, dlen>26 ? d+dlen-26 : d, sizeof(dispPath)-1);
    sdFsm(); lcd.setTextColor(getTheme().textSec); lcd.setTextDatum(MC_DATUM);
    lcd.drawString(dispPath, SCREEN_W/2+12, SDM_HDR_H/2);
}

static void sdmDrawFooter() {
    const Theme565 &t = getTheme();
    bool ru=(settings.language==LANG_RU);
    int fy = SCREEN_H-SDM_BAR_H;
    lcd.fillRect(0,fy,SCREEN_W,SDM_BAR_H,t.header);
    lcd.drawFastHLine(0,fy,SCREEN_W,t.accent);
    lcd.drawFastVLine(SCREEN_W/2,fy+4,SDM_BAR_H-8,t.accent);
    sdFsm(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(t.textPri);
    lcd.drawString(ru ? cyrStr("НАЗАД") : "BACK", SCREEN_W/4, fy+SDM_BAR_H/2);
    char cnt[10]; snprintf(cnt,sizeof(cnt),"%d",_sdmTotal);
    lcd.setTextColor(t.textSec);
    lcd.drawString(cnt, SCREEN_W*3/4, fy+SDM_BAR_H/2);
}

static void sdmDrawRow(int idx, int slot) {
    const Theme565 &t = getTheme();
    int y = SDM_LIST_Y + slot*SDM_ROW_H;
    bool sel = (idx==_sdmSel);
    lcd.fillRect(0,y,SCREEN_W,SDM_ROW_H, sel ? t.selected : (slot%2 ? t.rowOdd : t.rowEven));
    if (sel) lcd.fillRect(0,y,3,SDM_ROW_H,t.accent);
    if (slot>0) lcd.drawFastHLine(4,y,SCREEN_W-8,0x2104u);

    bool isUp = (_sdmIsDir[idx] && strcmp(sdmName(idx),"..")==0);
    uint16_t ic = sel ? (uint16_t)0xFFFFu : (_sdmIsDir[idx] ? (uint16_t)0x07FFu : t.textPri);

    sdFsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(ic);
    // Иконка
    lcd.drawString(_sdmIsDir[idx] ? (isUp ? ".." : "/ ") : "  ", 6, y+SDM_ROW_H/2);

    // Имя
    String nm = String(sdmName(idx));
    if (_sdmIsDir[idx] && !isUp) nm += "/";
    if (nm.length()>24) nm = nm.substring(0,22)+"..";
    lcd.drawString(nm.c_str(), 28, y+SDM_ROW_H/2);
}

static void sdmDrawList() {
    const Theme565 &t = getTheme();
    lcd.fillRect(0,SDM_LIST_Y,SCREEN_W,SDM_LIST_H,t.bg);

    if (_sdmTotal==0) {
        sdFsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
        bool ru=(settings.language==LANG_RU);
        lcd.drawString(ru?cyrStr("Пусто"):"Empty", SCREEN_W/2, SDM_LIST_Y+SDM_LIST_H/2);
        return;
    }
    if (_sdmSel < _sdmOffset) _sdmOffset = _sdmSel;
    if (_sdmSel >= _sdmOffset+SDM_ROWS) _sdmOffset = _sdmSel-SDM_ROWS+1;
    if (_sdmOffset<0) _sdmOffset=0;

    int end = min((int)_sdmOffset+SDM_ROWS, (int)_sdmTotal);
    for (int i=_sdmOffset; i<end; i++) sdmDrawRow(i, i-_sdmOffset);

    if (_sdmOffset>0) {
        lcd.fillTriangle(SCREEN_W-14,SDM_LIST_Y+6, SCREEN_W-2,SDM_LIST_Y+6, SCREEN_W-8,SDM_LIST_Y, t.textSec);
    }
    if (_sdmOffset+SDM_ROWS<_sdmTotal) {
        int by=SCREEN_H-SDM_BAR_H-8;
        lcd.fillTriangle(SCREEN_W-14,by, SCREEN_W-2,by, SCREEN_W-8,by+8, t.textSec);
    }
}

// ── Просмотрщик текста ────────────────────────────────────────
static void sdmDrawTextView() {
    const Theme565 &t = getTheme();
    bool ru=(settings.language==LANG_RU);
    lcd.fillScreen(t.bg);

    // Шапка
    lcd.fillRect(0,0,SCREEN_W,SDM_HDR_H,t.header);
    lcd.drawFastHLine(0,SDM_HDR_H-1,SCREEN_W,t.accent);
    sdFsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0xFD20u);
    String hn = String(_sdmVName);
    if (hn.length()>28) hn = hn.substring(0,26)+"..";
    lcd.drawString(hn.c_str(), SCREEN_W/2, SDM_HDR_H/2);

    // Содержимое
    int cy = SDM_HDR_H+6, lh=16;
    sdFsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(t.textPri);
    const char* p = _sdmVBuf;
    while (*p && cy < SCREEN_H-SDM_BAR_H-4) {
        const char* nl = strchr(p,'\n');
        int len = nl ? (int)(nl-p) : (int)strlen(p);
        if (len>40) len=40;
        char lb[42]; strncpy(lb,p,len); lb[len]='\0';
        lcd.drawString(lb, 6, cy);
        cy += lh;
        p = nl ? nl+1 : p+strlen(p);
    }

    // Подвал
    int fy=SCREEN_H-SDM_BAR_H;
    lcd.fillRect(0,fy,SCREEN_W,SDM_BAR_H,t.header);
    lcd.drawFastHLine(0,fy,SCREEN_W,t.accent);
    sdFsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textPri);
    lcd.drawString(ru?cyrStr("НАЗАД"):"BACK", SCREEN_W/2, fy+SDM_BAR_H/2);
}

// ── Активация строки ──────────────────────────────────────────
static void sdmActivate(int idx) {
    if (idx<0 || idx>=_sdmTotal) return;
    if (_sdmIsDir[idx]) {
        if (strcmp(sdmName(idx),"..")==0) {
            // Переход в родительскую директорию
            int sl=(int)strlen(_sdmDir)-1;
            while (sl>0 && _sdmDir[sl]!='/') sl--;
            if (sl<=0) strcpy(_sdmDir,"/");
            else { _sdmDir[sl]='\0'; }
        } else {
            char nd[84];
            if (strcmp(_sdmDir,"/")==0) snprintf(nd,sizeof(nd),"/%s",sdmName(idx));
            else snprintf(nd,sizeof(nd),"%s/%s",_sdmDir,sdmName(idx));
            strncpy(_sdmDir,nd,sizeof(_sdmDir)-1);
        }
        _sdmOffset=0; _sdmSel=0;
        sdmScan();
        const Theme565 &t = getTheme();
        lcd.fillScreen(t.bg);
        sdmDrawHeader(); sdmDrawList(); sdmDrawFooter();
    } else {
        // Открываем текстовые файлы
        if (!_sdmVBuf) return;
        String nm = String(sdmName(idx)); nm.toLowerCase();
        if (nm.endsWith(".cfg")||nm.endsWith(".txt")||nm.endsWith(".json")||
            nm.endsWith(".ini")||nm.endsWith(".log")) {
            char path[80];
            if (strcmp(_sdmDir,"/")==0) snprintf(path,sizeof(path),"/%s",sdmName(idx));
            else snprintf(path,sizeof(path),"%s/%s",_sdmDir,sdmName(idx));
            strncpy(_sdmVName,sdmName(idx),sizeof(_sdmVName)-1);
            File f = SD.open(path, FILE_READ);
            if (f) { int n=f.readBytes(_sdmVBuf,SDM_VBUF_SZ-1); _sdmVBuf[n]='\0'; f.close(); }
            else    strncpy(_sdmVBuf,"[Error]",SDM_VBUF_SZ-1);
            _sdmView = true;
            sdmDrawTextView();
        }
    }
}

// ══════════════════════════════════════════════════════════════
// PUBLIC API
// ══════════════════════════════════════════════════════════════

void sdMgrOpen() {
    sdmAlloc();
    strcpy(_sdmDir, "/");
    _sdmOffset=0; _sdmSel=0; _sdmView=false;
    sdmScan();
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    sdmDrawHeader(); sdmDrawList(); sdmDrawFooter();
}

uint8_t sdMgrHandleTouch(int x, int y) {
    if (_sdmView) {
        if (y >= SCREEN_H-SDM_BAR_H) {
            _sdmView = false;
            const Theme565 &t = getTheme();
            lcd.fillScreen(t.bg);
            sdmDrawHeader(); sdmDrawList(); sdmDrawFooter();
        }
        return 0;
    }
    int fy = SCREEN_H-SDM_BAR_H;
    if (y >= fy) { if (x < SCREEN_W/2) return BTN_B; return 0; }
    if (y >= SDM_LIST_Y && y < fy) {
        int slot=(y-SDM_LIST_Y)/SDM_ROW_H;
        int idx=_sdmOffset+slot;
        if (idx>=0 && idx<_sdmTotal) {
            if (idx==_sdmSel) { sdmActivate(idx); }
            else {
                int prev=_sdmSel; _sdmSel=idx;
                if (prev>=_sdmOffset && prev<_sdmOffset+SDM_ROWS) sdmDrawRow(prev,prev-_sdmOffset);
                sdmDrawRow(idx,slot);
            }
        }
    }
    return 0;
}

uint8_t sdMgrNavBtn(uint8_t btn) {
    if (_sdmView) {
        if (btn & BTN_B) {
            _sdmView=false;
            const Theme565 &t = getTheme();
            lcd.fillScreen(t.bg);
            sdmDrawHeader(); sdmDrawList(); sdmDrawFooter();
        }
        return 0;
    }
    if (btn & BTN_B) return BTN_B;
    if (btn & BTN_UP) {
        if (_sdmSel>0) { _sdmSel--; sdmDrawList(); }
    }
    if (btn & BTN_DOWN) {
        if (_sdmSel<_sdmTotal-1) { _sdmSel++; sdmDrawList(); }
    }
    if ((btn & BTN_A) || (btn & BTN_STA)) sdmActivate(_sdmSel);
    if (btn & BTN_LEFT) {
        // Быстро в родительскую директорию
        if (strcmp(_sdmDir,"/")!=0) sdmActivate(0); // ".." всегда первый
    }
    return 0;
}
