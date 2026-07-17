// web_manager.cpp — HTTP-сервер для загрузки файлов на SD карту.
//
// Интерфейс: http://<ip>/
//   Загрузить ROM  → /FomiCon/
//   Загрузить музыку → /Music/
//   Загрузить звуки → /sounds/
// Удалить файл: GET /delete?path=/FomiCon/game.nes
// Список файлов: автоматически на главной странице.
//
// Сервер работает на Core 1 в главном loop() — без отдельной задачи.

#include "web_manager.h"
#include "web_console.h"
#include "config.h"
#include "settings.h"
#include "display/display_manager.h"
#include "input/button_handler.h"
#include <WebServer.h>
#include <SD.h>
#include <WiFi.h>
#include <Arduino.h>

// NES framebuffer accessors (defined in osd.cpp)
extern const void* osdFramePtr();
extern size_t      osdFrameSize();
extern uint16_t    osdFrameW();
extern uint16_t    osdFrameH();

static WebServer    _srv(80);
static bool         _running   = false;
static bool         _debugOnly = false;   // true = только /console (не полный менеджер)
static char         _ip[20]    = {};
static TaskHandle_t _dbgTask   = nullptr; // фоновый task для debug-сервера (работает пока emu_run блокирует loop)

// ── Директории доступные для загрузки ─────────────────────────
struct UploadDir { const char *name; const char *path; const char *accept; };
static const UploadDir _dirs[] = {
    { "ROMs",        "/FomiCon",     ".nes"           },
    { "Music",       "/Music",       ".mp3,.wav"       },
    { "Sounds",      "/sounds",      ".wav"            },
    { "Cover art",   "/covers",      ".raw,.jpg,.png"  },
    { "Firmware",    "/update",      ".bin"            },
};
static const int _ndirs = (int)(sizeof(_dirs) / sizeof(_dirs[0]));

// ── Стейт загрузки ─────────────────────────────────────────────
static File   _uploadFile;
static String _uploadPath;
static bool   _uploadOk = false;

// ── Вспомогательная: размер файла в читаемом виде ──────────────
static String _fmtSize(size_t b) {
    if (b >= 1048576) { char s[16]; snprintf(s, sizeof(s), "%.1f MB", b / 1048576.0f); return s; }
    if (b >= 1024)    { char s[16]; snprintf(s, sizeof(s), "%.0f KB", b / 1024.0f);    return s; }
    return String(b) + " B";
}

// ── Файл с SD (audio player + image/RAW viewer) ────────────────────────────────
static void handleSDFile() {
    String path = _srv.arg("path");
    if (path.length() == 0) { _srv.send(400, "text/plain", "No path"); return; }
    if (!SD.exists(path))   { _srv.send(404, "text/plain", "Not found"); return; }
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        if (f) f.close();
        _srv.send(404, "text/plain", "Not found");
        return;
    }
    size_t sz = f.size();
    String lp = path; lp.toLowerCase();
    const char *ct = "application/octet-stream";
    if      (lp.endsWith(".mp3"))                          ct = "audio/mpeg";
    else if (lp.endsWith(".wav"))                          ct = "audio/wav";
    else if (lp.endsWith(".jpg") || lp.endsWith(".jpeg")) ct = "image/jpeg";
    else if (lp.endsWith(".png"))                          ct = "image/png";
    else if (lp.endsWith(".bmp"))                          ct = "image/bmp";
    else if (lp.endsWith(".gif"))                          ct = "image/gif";
    _srv.sendHeader("Accept-Ranges", "none");
    _srv.sendHeader("Cache-Control", "no-cache");
    _srv.sendHeader("Access-Control-Allow-Origin", "*");
    _srv.setContentLength(sz);
    _srv.send(200, ct, "");
    static uint8_t _sdBounce[2048];
    while (f.available() > 0) {
        int n = f.read(_sdBounce, sizeof(_sdBounce));
        if (n > 0) _srv.sendContent((const char*)_sdBounce, n);
    }
    f.close();
}

// ── Переименование файла или папки ─────────────────────────────────────────────
static void handleRename() {
    String path = _srv.arg("path");
    String name = _srv.arg("name");
    // Санитизация: убираем слеши чтобы нельзя было выйти из текущей директории
    name.replace("/", ""); name.replace("\\", "");
    int sl = path.lastIndexOf('/');
    String dir = (sl > 0) ? path.substring(0, sl) : "/";
    if (path.length() > 0 && name.length() > 0) {
        // sl==0 → корневой файл ("/config.json"): dir="/", npath="/newname" (без двойного слеша)
        String npath = (sl > 0) ? dir + "/" + name : "/" + name;
        if (SD.rename(path.c_str(), npath.c_str()))
            Serial.printf("[WEB] Renamed: %s -> %s\n", path.c_str(), npath.c_str());
    }
    _srv.sendHeader("Location", "/?dir=" + dir);
    _srv.send(302, "text/plain", "");
}

// ── Главная страница — двухпанельный файл-менеджер ────────────
static void handleRoot() {
    // Выбранная папка из GET-параметра ?dir=
    String curPath = _srv.hasArg("dir") ? _srv.arg("dir") : String(_dirs[0].path);
    // Найдём её индекс (точное совпадение или подпапка)
    int curIdx = 0;
    bool curFound = false;
    for (int i = 0; i < _ndirs; i++) {
        if (curPath == _dirs[i].path || curPath.startsWith(String(_dirs[i].path) + "/")) {
            curIdx = i; curFound = true; break;
        }
    }

    _srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _srv.send(200, "text/html; charset=utf-8", "");

    _srv.sendContent(F("<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>AleksOS Web Manager</title><style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{background:#111;color:#ddd;font-family:sans-serif;height:100vh;display:flex;flex-direction:column}"
        ".hdr{background:#1e1e1e;border-bottom:2px solid #f80;padding:8px 14px;display:flex;align-items:center;gap:12px}"
        ".hdr h1{color:#f80;font-size:17px;flex:1}"
        ".hdr .ver{color:#555;font-size:11px}"
        ".breadcrumb{color:#aaa;font-size:13px}"
        ".breadcrumb span{color:#f80}"
        ".main{display:flex;flex:1;overflow:hidden}"
        ".sidebar{width:160px;background:#181818;border-right:1px solid #2a2a2a;overflow-y:auto;padding:8px 0}"
        ".sidebar a{display:block;padding:9px 14px;color:#bbb;text-decoration:none;font-size:13px;border-left:3px solid transparent}"
        ".sidebar a:hover{background:#252525;color:#eee}"
        ".sidebar a.active{background:#252525;color:#f80;border-left-color:#f80}"
        ".sidebar .ico{margin-right:6px}"
        ".content{flex:1;display:flex;flex-direction:column;overflow:hidden}"
        ".toolbar{background:#1e1e1e;padding:8px 12px;border-bottom:1px solid #2a2a2a;display:flex;gap:8px;align-items:center;flex-wrap:wrap}"
        ".toolbar form{display:flex;gap:6px;align-items:center}"
        "input[type=file]{background:#2a2a2a;color:#ddd;border:1px solid #444;border-radius:4px;padding:4px 8px;font-size:12px}"
        "input[type=text]{background:#2a2a2a;color:#ddd;border:1px solid #444;border-radius:4px;padding:5px 8px;font-size:12px;width:130px}"
        ".btn-up{background:#d4700a;color:#fff;border:none;border-radius:4px;padding:6px 14px;cursor:pointer;font-size:13px}"
        ".btn-up:hover{background:#f80}"
        ".btn-new{background:#1a4a1a;color:#8f8;border:1px solid #2d7a2d;border-radius:4px;padding:6px 10px;cursor:pointer;font-size:12px}"
        ".btn-new:hover{background:#234a23}"
        ".files{flex:1;overflow-y:auto;padding:4px 0}"
        ".fi{display:flex;align-items:center;padding:7px 14px;border-bottom:1px solid #1e1e1e}"
        ".fi:hover{background:#1e1e1e}"
        ".fi:nth-child(even){background:#161616}"
        ".fi:nth-child(even):hover{background:#1e1e1e}"
        ".fn{flex:1;font-size:13px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}"
        ".sz{color:#666;font-size:11px;min-width:60px;text-align:right;margin-right:10px}"
        ".del{background:#5a0000;color:#f88;border:1px solid #8b0000;padding:3px 10px;"
        "border-radius:3px;cursor:pointer;font-size:11px}"
        ".del:hover{background:#8b0000;color:#fff}"
        ".empty{color:#555;padding:20px 14px;font-size:13px}"
        ".act{display:flex;align-items:center;gap:3px}"
        ".play{background:#1a3a1a;color:#8f8;border:1px solid #2d7a2d;padding:3px 8px;"
        "border-radius:3px;cursor:pointer;font-size:11px}"
        ".play:hover{background:#234a23}"
        ".view{background:#1a2a3a;color:#8af;border:1px solid #2d5a7a;padding:3px 8px;"
        "border-radius:3px;cursor:pointer;font-size:11px}"
        ".view:hover{background:#1e3a5a}"
        ".ren{background:#2a2a1a;color:#ff8;border:1px solid #6a6a2d;padding:3px 8px;"
        "border-radius:3px;cursor:pointer;font-size:11px}"
        ".ren:hover{background:#3a3a20}"
        ".modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.88);"
        "z-index:100;align-items:center;justify-content:center}"
        ".modal.open{display:flex}"
        ".mbox{background:#1e1e1e;border:1px solid #444;border-radius:8px;"
        "padding:16px 20px;max-width:92vw;max-height:90vh;"
        "display:flex;flex-direction:column;gap:12px;min-width:280px}"
        ".mcls{color:#888;cursor:pointer;font-size:22px;align-self:flex-end;"
        "line-height:1;margin-bottom:-4px}"
        ".mcls:hover{color:#fff}"
        ".mtit{color:#f80;font-size:13px;word-break:break-all}"
        "audio{width:100%;min-width:260px}"
        "#imgV{max-width:80vw;max-height:70vh;image-rendering:pixelated;display:block}"
        "#rawC{max-width:80vw;max-height:70vh;image-rendering:pixelated}"
        "</style></head><body>"));

    // ── Шапка
    _srv.sendContent(F("<div class='hdr'><h1>&#127918; AleksOS Web Manager</h1>"
        "<span class='ver'>"));
    _srv.sendContent(FIRMWARE_VERSION);
    // SD свободное место
    {
        uint64_t tot = SD.totalBytes(), used = SD.usedBytes();
        if (tot > 0) {
            uint64_t fr = tot - used;
            char s[24];
            if      (fr >= 1073741824ULL) snprintf(s, sizeof(s), "%.1f GB free", fr / 1073741824.0f);
            else if (fr >= 1048576ULL)    snprintf(s, sizeof(s), "%.0f MB free", fr / 1048576.0f);
            else                          snprintf(s, sizeof(s), "%.0f KB free", fr / 1024.0f);
            _srv.sendContent(F("</span><span class='ver' style='color:#4a4'>"));
            _srv.sendContent(s);
        }
    }
    _srv.sendContent(F("</span></div>"));

    // ── Основной лайаут
    _srv.sendContent(F("<div class='main'>"));

    // ── Левая панель: навигация по папкам
    _srv.sendContent(F("<nav class='sidebar'>"));
    for (int i = 0; i < _ndirs; i++) {
        bool active = (i == curIdx);
        _srv.sendContent("<a href='/?dir=");
        _srv.sendContent(_dirs[i].path);
        _srv.sendContent(active ? "' class='active'>" : "'>");
        _srv.sendContent("<span class='ico'>&#128193;</span>");
        _srv.sendContent(_dirs[i].name);
        _srv.sendContent("<br><small style='color:#555;font-size:10px'>");
        _srv.sendContent(_dirs[i].path);
        _srv.sendContent("</small></a>");
    }
    _srv.sendContent(F("<a href='/console'><span class='ico'>&#9654;</span>Console</a>"
        "<a href='/remote'><span class='ico'>&#128247;</span>Remote</a>"));
    _srv.sendContent(F("</nav>"));

    // ── Правая панель
    _srv.sendContent(F("<div class='content'>"));

    // Тулбар: Upload + New Folder
    _srv.sendContent(F("<div class='toolbar'>"));

    // Upload form (accept = типы файлов текущей папки)
    _srv.sendContent(F("<form method='POST' action='/upload' enctype='multipart/form-data'>"));
    _srv.sendContent("<input type='hidden' name='dir' value='");
    _srv.sendContent(curPath);
    _srv.sendContent("'><input type='file' name='file' multiple");
    if (curFound) {
        _srv.sendContent(" accept='");
        _srv.sendContent(_dirs[curIdx].accept);
        _srv.sendContent("'");
    }
    _srv.sendContent(F("><button class='btn-up' type='submit'>&#8593; Upload</button>"
        "</form>"));

    // New folder form
    _srv.sendContent(F("<form method='GET' action='/mkdir'>"
        "<input type='hidden' name='base' value='"));
    _srv.sendContent(curPath);
    _srv.sendContent(F("'>"
        "<input type='text' name='name' placeholder='New folder...'>"
        "<button class='btn-new' type='submit'>+ New</button>"
        "</form></div>"));

    // Список файлов — хлебная крошка с текущим путём
    _srv.sendContent(F("<div style='padding:4px 14px;background:#161616;border-bottom:1px solid #2a2a2a;"
        "font-size:11px;color:#555;white-space:nowrap;overflow:hidden;text-overflow:ellipsis'>"
        "&#128193; "));
    _srv.sendContent(curPath);
    _srv.sendContent(F("</div><div class='files'>"));
    // Кнопка «Назад» в подпапке
    if (curFound && curPath != _dirs[curIdx].path) {
        int ups = curPath.lastIndexOf('/');
        String parent = (ups > 0) ? curPath.substring(0, ups) : _dirs[curIdx].path;
        _srv.sendContent("<div class='fi'><span class='fn'>"
            "<a href='/?dir=" + parent + "' style='color:#8cf;text-decoration:none'>"
            "&#128194; ..</a></span></div>");
    }
    File dir = SD.open(curPath);
    int cnt = 0;
    if (dir && dir.isDirectory()) {
        while (true) {
            File f = dir.openNextFile();
            if (!f) break;
            bool isDir = f.isDirectory();
            String name = String(f.name());
            int sl = name.lastIndexOf('/');
            if (sl >= 0) name = name.substring(sl + 1);
            size_t sz = isDir ? 0 : f.size();
            f.close();

            _srv.sendContent("<div class='fi'>");
            if (isDir) {
                // Вложенная папка — кликабельна
                _srv.sendContent("<span class='fn'>&#128193; <a href='/?dir=");
                _srv.sendContent(curPath + "/" + name);
                _srv.sendContent("' style='color:#8cf;text-decoration:none'>");
                _srv.sendContent(name);
                _srv.sendContent("/</a></span><span class='sz'>—</span>");
                // Кнопки папки: переименовать + удалить
                String delPath = curPath + "/" + name;
                _srv.sendContent(
                    "<span class='act'>"
                    "<button class='ren' data-path=\"" + delPath + "\" data-name=\"" + name + "\">&#9998;</button>"
                    "<form method='GET' action='/rmdir' style='margin:0'>"
                    "<input type='hidden' name='path' value='" + delPath + "'>"
                    "<button class='del' onclick=\"return confirm('Delete folder " + name + "?')\">&#10005;</button>"
                    "</form></span>");
            } else {
                _srv.sendContent("<span class='fn'>&#128196; ");
                _srv.sendContent(name);
                _srv.sendContent("</span><span class='sz'>");
                _srv.sendContent(_fmtSize(sz));
                _srv.sendContent("</span>");
                String filePath = curPath + "/" + name;
                String lname = name; lname.toLowerCase();
                bool isAudio = lname.endsWith(".mp3") || lname.endsWith(".wav");
                bool isImage = lname.endsWith(".jpg") || lname.endsWith(".jpeg") ||
                               lname.endsWith(".png") || lname.endsWith(".bmp") ||
                               lname.endsWith(".raw");
                _srv.sendContent("<span class='act'>");
                if (isAudio)
                    _srv.sendContent("<button class='play' data-path=\"" + filePath + "\" data-name=\"" + name + "\">&#9654;</button>");
                if (isImage)
                    _srv.sendContent("<button class='view' data-path=\"" + filePath + "\" data-name=\"" + name + "\">&#128247;</button>");
                _srv.sendContent(
                    "<button class='ren' data-path=\"" + filePath + "\" data-name=\"" + name + "\">&#9998;</button>"
                    "<form method='GET' action='/delete' style='margin:0'>"
                    "<input type='hidden' name='path' value='" + filePath + "'>"
                    "<button class='del' onclick=\"return confirm('Delete " + name + "?')\">&#10005;</button>"
                    "</form></span>");
            }
            _srv.sendContent("</div>");
            cnt++;
        }
        dir.close();
    }
    if (cnt == 0) _srv.sendContent(F("<p class='empty'>Folder is empty</p>"));

    _srv.sendContent(F("</div></div></div>"));

    // Audio player modal
    _srv.sendContent(F(
        "<div class='modal' id='audMod'>"
          "<div class='mbox'>"
            "<span class='mcls' id='audX'>&#10005;</span>"
            "<div class='mtit' id='audT'></div>"
            "<audio id='audEl' controls></audio>"
          "</div>"
        "</div>"));

    // Image / RAW viewer modal
    _srv.sendContent(F(
        "<div class='modal' id='imgMod'>"
          "<div class='mbox' style='align-items:center'>"
            "<span class='mcls' id='imgX'>&#10005;</span>"
            "<div class='mtit' id='imgT'></div>"
            "<img id='imgV'>"
            "<canvas id='rawC' style='display:none'></canvas>"
          "</div>"
        "</div>"));

    // JS: modal open/close + event delegation (play / view / rename)
    _srv.sendContent(F("<script>"
      "var aM=document.getElementById('audMod'),"
          "iM=document.getElementById('imgMod'),"
          "aE=document.getElementById('audEl'),"
          "iV=document.getElementById('imgV'),"
          "rC=document.getElementById('rawC');"
      "function cA(){aM.classList.remove('open');aE.pause();aE.src='';aE.load();}"
      "function cI(){iM.classList.remove('open');iV.src='';}"
      "document.getElementById('audX').onclick=cA;"
      "document.getElementById('imgX').onclick=cI;"
      "document.addEventListener('keydown',function(e){if(e.key==='Escape'){cA();cI();}});"
      "document.addEventListener('click',function(e){"
        "var b=e.target.closest?e.target.closest('.play,.view,.ren'):null;if(!b)return;"
        "var p=b.dataset.path,n=b.dataset.name;if(!p||!n)return;"
        "if(b.classList.contains('play')){"
          "document.getElementById('audT').textContent=n;"
          "aE.src='/sd?path='+encodeURIComponent(p);"
          "aM.classList.add('open');"
          "aE.play().catch(function(){});"
        "}else if(b.classList.contains('view')){"
          "document.getElementById('imgT').textContent=n;"
          "if(p.toLowerCase().endsWith('.raw')){"
            "iV.style.display='none';rC.style.display='block';"
            "fetch('/sd?path='+encodeURIComponent(p))"
            ".then(function(r){return r.arrayBuffer();})"
            ".then(function(ab){"
              "if(ab.byteLength<4)return;"
              "var v=new DataView(ab),w=v.getUint16(0,true),h=v.getUint16(2,true);"
              "if(!w||!h||ab.byteLength<4+w*h*2)return;"
              "rC.width=w;rC.height=h;"
              "var ctx=rC.getContext('2d'),px=new Uint16Array(ab,4,w*h);"
              "var id=ctx.createImageData(w,h),d=id.data;"
              "for(var i=0;i<w*h;i++){var c=px[i];"
                "d[i*4]=(c>>8)&248;d[i*4+1]=(c>>3)&252;d[i*4+2]=(c<<3)&248;d[i*4+3]=255;}"
              "ctx.putImageData(id,0,0);"
            "}).catch(function(){});"
          "}else{"
            "iV.style.display='block';rC.style.display='none';"
            "iV.src='/sd?path='+encodeURIComponent(p);"
          "}"
          "iM.classList.add('open');"
        "}else if(b.classList.contains('ren')){"
          "var nn=prompt('Rename to:',n);"
          "if(nn&&nn!==n)window.location='/rename?path='+encodeURIComponent(p)+'&name='+encodeURIComponent(nn);"
        "}"
      "});"
      "</script></body></html>"));
    _srv.sendContent("");
}

// ── Загрузка файла (multipart) ─────────────────────────────────
static void handleUploadPage() {
    String redir = "/?dir=" + _uploadPath.substring(0, _uploadPath.lastIndexOf('/'));
    if (_uploadOk) {
        _srv.sendHeader("Location", redir);
        _srv.send(302, "text/plain", "");
    } else {
        _srv.send(500, "text/html; charset=utf-8",
            "<html><body style='font-family:sans-serif;background:#111;color:#e74c3c;padding:16px'>"
            "Upload failed. <a href='/' style='color:#f80'>Back</a></body></html>");
    }
}

static void handleUpload() {
    HTTPUpload &up = _srv.upload();

    if (up.status == UPLOAD_FILE_START) {
        _uploadOk = false;
        String dir = _srv.arg("dir");
        if (dir.length() == 0) dir = "/FomiCon";

        // Создаём директорию если не существует
        if (!SD.exists(dir)) SD.mkdir(dir);

        // Получаем чистый basename из имени файла браузера
        String fname = up.filename;
        int sl = fname.lastIndexOf('/');
        if (sl >= 0) fname = fname.substring(sl + 1);
        // Убираем нежелательные символы
        for (int i = 0; i < (int)fname.length(); i++) {
            char c = fname[i];
            if (c == ' ') fname[i] = '_';
        }

        _uploadPath = dir + "/" + fname;
        Serial.printf("[WEB] Upload start: %s\n", _uploadPath.c_str());
        _uploadFile = SD.open(_uploadPath.c_str(), FILE_WRITE);
        if (!_uploadFile) Serial.println("[WEB] Failed to open file for write");

    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (_uploadFile) _uploadFile.write(up.buf, up.currentSize);

    } else if (up.status == UPLOAD_FILE_END) {
        if (_uploadFile) {
            _uploadFile.close();
            _uploadOk = true;
            Serial.printf("[WEB] Upload done: %s (%u bytes)\n",
                _uploadPath.c_str(), (unsigned)up.totalSize);
        }
    }
}

// ── Удаление файла ─────────────────────────────────────────────
static void handleDelete() {
    String path = _srv.arg("path");
    String back = "/";
    int sl = path.lastIndexOf('/');
    if (sl > 0) back = "/?dir=" + path.substring(0, sl);
    if (path.length() > 0 && SD.exists(path)) {
        SD.remove(path);
        Serial.printf("[WEB] Deleted: %s\n", path.c_str());
    }
    _srv.sendHeader("Location", back);
    _srv.send(302, "text/plain", "");
}

// ── Создание папки ─────────────────────────────────────────────
static void handleMkdir() {
    String base = _srv.arg("base");
    String name = _srv.arg("name");
    if (name.length() > 0 && base.length() > 0) {
        String full = base + "/" + name;
        SD.mkdir(full);
        Serial.printf("[WEB] mkdir: %s\n", full.c_str());
    }
    _srv.sendHeader("Location", "/?dir=" + base);
    _srv.send(302, "text/plain", "");
}

// ── Удаление папки ─────────────────────────────────────────────
static void handleRmdir() {
    String path = _srv.arg("path");
    String back = "/";
    int sl = path.lastIndexOf('/');
    if (sl > 0) back = "/?dir=" + path.substring(0, sl);
    if (path.length() > 0 && SD.exists(path)) {
        SD.rmdir(path);
        Serial.printf("[WEB] rmdir: %s\n", path.c_str());
    }
    _srv.sendHeader("Location", back);
    _srv.send(302, "text/plain", "");
}

// ── Веб-консоль: страница ─────────────────────────────────────────────────────
static void handleConsole() {
    _srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _srv.send(200, "text/html; charset=utf-8", "");

    _srv.sendContent(F("<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>AleksOS Console</title><style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{background:#111;color:#ddd;font-family:sans-serif;height:100vh;display:flex;flex-direction:column}"
        ".hdr{background:#1e1e1e;border-bottom:2px solid #f80;padding:8px 14px;display:flex;align-items:center;gap:12px}"
        ".hdr h1{color:#f80;font-size:17px;flex:1}"
        ".hdr .ver{color:#555;font-size:11px}"
        ".main{display:flex;flex:1;overflow:hidden}"
        ".sidebar{width:160px;background:#181818;border-right:1px solid #2a2a2a;overflow-y:auto;padding:8px 0}"
        ".sidebar a{display:block;padding:9px 14px;color:#bbb;text-decoration:none;font-size:13px;border-left:3px solid transparent}"
        ".sidebar a:hover{background:#252525;color:#eee}"
        ".sidebar a.active{background:#252525;color:#f80;border-left-color:#f80}"
        ".sidebar .ico{margin-right:6px}"
        ".content{flex:1;display:flex;flex-direction:column;overflow:hidden;padding:10px}"
        ".toolbar{display:flex;gap:8px;align-items:center;margin-bottom:8px}"
        ".btn{background:#1a3a1a;color:#8f8;border:1px solid #2d7a2d;border-radius:4px;padding:6px 14px;cursor:pointer;font-size:12px}"
        ".btn:hover{background:#234a23}"
        ".btn-red{background:#3a1a1a;color:#f88;border-color:#7a2d2d}"
        ".btn-red:hover{background:#4a2323}"
        ".status{color:#666;font-size:11px;margin-left:auto}"
        "#term{flex:1;background:#0a0a0a;color:#0f0;font-family:monospace;font-size:12px;"
        "padding:10px;overflow-y:auto;white-space:pre-wrap;word-break:break-all;"
        "border:1px solid #1e1e1e;border-radius:4px;line-height:1.4}"
        "</style></head><body>"));

    _srv.sendContent(F("<div class='hdr'><h1>&#127918; AleksOS Web Manager</h1>"
        "<span class='ver'>"));
    _srv.sendContent(FIRMWARE_VERSION);
    _srv.sendContent(F("</span></div><div class='main'>"));

    // Сайдбар
    _srv.sendContent(F("<nav class='sidebar'>"
        "<a href='/'><span class='ico'>&#128193;</span>Files</a>"
        "<a href='/console' class='active'><span class='ico'>&#9654;</span>Console</a>"
        "<a href='/remote'><span class='ico'>&#128247;</span>Remote</a>"
        "</nav>"));

    // Консольная панель
    _srv.sendContent(F("<div class='content'>"
        "<div class='toolbar'>"
        "<button class='btn' onclick='autoScroll=!autoScroll;this.textContent=autoScroll?\"Auto-scroll ON\":\"Auto-scroll OFF\"'>Auto-scroll ON</button>"
        "<button class='btn btn-red' onclick='term.textContent=\"\";'>Clear view</button>"
        "<span class='status' id='st'>Connecting...</span>"
        "</div>"
        "<div id='term'></div>"
        "</div></div>"));

    _srv.sendContent(F("<script>"
        "var pos=0,autoScroll=true;"
        "var term=document.getElementById('term');"
        "var st=document.getElementById('st');"
        "function poll(){"
        "  var x=new XMLHttpRequest();"
        "  x.open('GET','/console/log?pos='+pos,true);"
        "  x.onload=function(){"
        "    if(x.status==200){"
        "      var r=JSON.parse(x.responseText);"
        "      if(r.data&&r.data.length){"
        "        term.textContent+=r.data;"
        "        if(autoScroll)term.scrollTop=term.scrollHeight;"
        "      }"
        "      pos=r.pos;"
        "      st.textContent='Connected | pos:'+pos;"
        "    }"
        "  };"
        "  x.onerror=function(){st.textContent='Error';};"
        "  x.send();"
        "}"
        "setInterval(poll,800);poll();"
        "</script></body></html>"));
    _srv.sendContent("");
}

// ── Веб-консоль: данные (polling) ─────────────────────────────────────────────
static void handleConsoleLog() {
    size_t fromPos = 0;
    if (_srv.hasArg("pos")) fromPos = (size_t)_srv.arg("pos").toInt();

    static char _jsonBuf[2048];
    char dataBuf[1800];
    size_t newPos = fromPos;
    size_t got = webConsoleRead(fromPos, dataBuf, sizeof(dataBuf) - 1, &newPos);
    dataBuf[got] = '\0';

    // JSON-escape: все спецсимволы должны быть экранированы внутри JSON-строки
    int ji = 0;
    _jsonBuf[ji++] = '{';
    _jsonBuf[ji++] = '"'; _jsonBuf[ji++] = 'd'; _jsonBuf[ji++] = 'a';
    _jsonBuf[ji++] = 't'; _jsonBuf[ji++] = 'a'; _jsonBuf[ji++] = '"';
    _jsonBuf[ji++] = ':'; _jsonBuf[ji++] = '"';
    for (size_t i = 0; i < got && ji < (int)sizeof(_jsonBuf) - 20; i++) {
        unsigned char c = (unsigned char)dataBuf[i];
        if      (c == '\\') { _jsonBuf[ji++]='\\'; _jsonBuf[ji++]='\\'; }
        else if (c == '"')  { _jsonBuf[ji++]='\\'; _jsonBuf[ji++]='"';  }
        else if (c == '\n') { _jsonBuf[ji++]='\\'; _jsonBuf[ji++]='n';  }
        else if (c == '\r') { _jsonBuf[ji++]='\\'; _jsonBuf[ji++]='r';  }
        else if (c == '\t') { _jsonBuf[ji++]='\\'; _jsonBuf[ji++]='t';  }
        else if (c < 0x20) {
            static const char hex[] = "0123456789ABCDEF";
            _jsonBuf[ji++]='\\'; _jsonBuf[ji++]='u';
            _jsonBuf[ji++]='0';  _jsonBuf[ji++]='0';
            _jsonBuf[ji++]=hex[c>>4]; _jsonBuf[ji++]=hex[c&0xF];
        }
        else _jsonBuf[ji++] = (char)c;
    }
    _jsonBuf[ji++] = '"'; _jsonBuf[ji++] = ',';
    ji += snprintf(_jsonBuf + ji, sizeof(_jsonBuf) - ji - 2, "\"pos\":%zu}", newPos);
    _jsonBuf[ji] = '\0';

    _srv.sendHeader("Cache-Control", "no-cache");
    _srv.send(200, "application/json", _jsonBuf);
}

// ── Remote: бинарный кадр (4-byte header + RGB565) ────────────────────────────
// PSRAM-буфер для снимка экрана меню (выделяется один раз по требованию).
static uint16_t* _uiCapBuf = nullptr;

static void handleScreen() {
    const void* frame = osdFramePtr();
    size_t      fsz   = osdFrameSize();
    uint16_t    fw    = osdFrameW();
    uint16_t    fh    = osdFrameH();

    // Если NES не запущена — снимаем экран меню через LCD readback.
    // readRect() читает GRAM из ST7789 по MISO. Оба (loop и этот task) на Core 0,
    // поэтому SPI-bus свободен когда нас вызывают (preemption между завершёнными операциями).
    if (!frame || !fsz) {
        if (!_uiCapBuf)
            _uiCapBuf = (uint16_t*)ps_malloc((size_t)SCREEN_W * SCREEN_H * 2);
        if (_uiCapBuf) {
            lcd.readRect(0, 0, SCREEN_W, SCREEN_H, _uiCapBuf);
            frame = _uiCapBuf;
            fsz   = (size_t)SCREEN_W * SCREEN_H * 2;
            fw    = SCREEN_W;
            fh    = SCREEN_H;
        }
    }

    if (!frame || !fsz || !fw || !fh) {
        _srv.send(204, "application/octet-stream", "");
        return;
    }
    _srv.sendHeader("Cache-Control", "no-store");
    _srv.sendHeader("Access-Control-Allow-Origin", "*");
    _srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _srv.send(200, "application/octet-stream", "");
    // 4-byte dimension header (little-endian uint16 w, h)
    uint8_t hdr[4] = { (uint8_t)(fw & 0xFF), (uint8_t)(fw >> 8),
                        (uint8_t)(fh & 0xFF), (uint8_t)(fh >> 8) };
    _srv.sendContent((const char*)hdr, 4);
    // Frame in 2 KB chunks: bounce PSRAM→DRAM before sending
    static uint8_t _scrnBounce[2048];
    const uint8_t* src = (const uint8_t*)frame;
    size_t remain = fsz;
    while (remain > 0) {
        size_t n = remain < sizeof(_scrnBounce) ? remain : sizeof(_scrnBounce);
        memcpy(_scrnBounce, src, n);
        _srv.sendContent((const char*)_scrnBounce, n);
        src    += n;
        remain -= n;
    }
    _srv.sendContent("");
}

// ── Remote: инжект кнопок (?mask=N) ──────────────────────────────────────────
static void handleInput() {
    uint8_t mask = 0;
    if (_srv.hasArg("mask")) mask = (uint8_t)_srv.arg("mask").toInt();
    buttons.webInject(mask);
    _srv.sendHeader("Cache-Control", "no-cache");
    _srv.sendHeader("Access-Control-Allow-Origin", "*");
    _srv.send(200, "text/plain", "ok");
}

// ── Remote: HTML-страница с canvas + gamepad ──────────────────────────────────
static void handleRemote() {
    _srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _srv.send(200, "text/html; charset=utf-8", "");

    _srv.sendContent(F("<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>AleksOS Remote</title><style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{background:#111;color:#ddd;font-family:sans-serif;"
          "height:100vh;display:flex;flex-direction:column;user-select:none;overflow:hidden}"
        ".hdr{background:#1e1e1e;border-bottom:2px solid #f80;padding:8px 14px;"
          "display:flex;align-items:center;gap:12px}"
        ".hdr h1{color:#f80;font-size:17px;flex:1}"
        ".hdr .ver{color:#555;font-size:11px}"
        ".main{display:flex;flex:1;overflow:hidden}"
        ".sb{width:120px;background:#181818;border-right:1px solid #2a2a2a;"
          "overflow-y:auto;padding:8px 0}"
        ".sb a{display:block;padding:9px 14px;color:#bbb;text-decoration:none;"
          "font-size:13px;border-left:3px solid transparent}"
        ".sb a:hover{background:#252525;color:#eee}"
        ".sb a.active{background:#252525;color:#f80;border-left-color:#f80}"
        ".sb .ico{margin-right:6px}"
        ".wrap{flex:1;display:flex;flex-direction:column;align-items:center;"
          "justify-content:center;padding:10px;gap:10px;overflow:hidden}"
        "#scr{border:2px solid #333;border-radius:4px;"
          "image-rendering:pixelated;image-rendering:crisp-edges;cursor:crosshair}"
        "#scr.live{border-color:#4c4}"
        "#nf{color:#444;font-size:13px;display:none}"
        ".pad{display:flex;align-items:center;justify-content:center;"
          "gap:14px;flex-wrap:wrap}"
        ".dp{display:grid;grid-template-columns:42px 42px 42px;"
          "grid-template-rows:42px 42px 42px;gap:3px}"
        ".db{background:#2a2a2a;color:#ccc;border:none;border-radius:6px;"
          "cursor:pointer;font-size:15px;font-weight:bold;width:42px;height:42px;"
          "transition:background .05s;touch-action:none}"
        ".db.on,.db:active{background:#f80;color:#000}"
        ".dp .ct{background:#1e1e1e;border-radius:6px}"
        ".ab{width:46px;height:46px;border-radius:50%;border:2px solid;"
          "cursor:pointer;font-size:13px;font-weight:bold;"
          "touch-action:none;transition:filter .05s}"
        ".ab.on,.ab:active{filter:brightness(1.6)}"
        ".btnA{background:#1a4020;color:#7f7;border-color:#3d8b3d}"
        ".btnB{background:#401a1a;color:#f77;border-color:#8b3d3d}"
        ".syswrap{display:flex;flex-direction:column;gap:8px;align-items:center}"
        ".sys{display:flex;gap:8px}"
        ".sysb{background:#1e1e1e;color:#aaa;border:1px solid #444;border-radius:4px;"
          "padding:5px 10px;font-size:11px;cursor:pointer;touch-action:none}"
        ".sysb.on,.sysb:active{background:#444;color:#fff}"
        ".hint{color:#333;font-size:9px;text-align:center}"
        "</style></head>"));

    _srv.sendContent(F("<body><div class='hdr'><h1>&#127918; AleksOS Remote</h1>"
        "<span class='ver'>"));
    _srv.sendContent(FIRMWARE_VERSION);
    _srv.sendContent(F("</span><span class='ver' id='fp'>&#11044; connecting</span></div>"
        "<div class='main'>"
        "<nav class='sb'>"
        "<a href='/'><span class='ico'>&#128193;</span>Files</a>"
        "<a href='/console'><span class='ico'>&#9654;</span>Console</a>"
        "<a href='/remote' class='active'><span class='ico'>&#128247;</span>Remote</a>"
        "</nav>"
        "<div class='wrap'>"
        "<canvas id='scr' width='256' height='224'></canvas>"
        "<div id='nf'>&#9888; Start a NES game to see the screen</div>"
        "<div class='pad'>"
        // D-pad
        "<div class='dp'>"
        "<div></div>"
        "<button class='db' data-m='16'>&#9650;</button>"
        "<div></div>"
        "<button class='db' data-m='64'>&#9664;</button>"
        "<div class='ct'></div>"
        "<button class='db' data-m='128'>&#9654;</button>"
        "<div></div>"
        "<button class='db' data-m='32'>&#9660;</button>"
        "<div></div>"
        "</div>"
        // SELECT + START
        "<div class='syswrap'>"
        "<div class='sys'>"
        "<button class='sysb' data-m='2'>SEL</button>"
        "<button class='sysb' data-m='1'>STA</button>"
        "</div>"
        "<div class='hint'>WASD/arrows=dpad&nbsp;&nbsp;Z=A&nbsp;&nbsp;X=B<br>Enter=START&nbsp;&nbsp;Shift=SELECT</div>"
        "</div>"
        // A / B
        "<div style='display:flex;gap:10px;align-items:center'>"
        "<button class='ab btnB' data-m='8'>B</button>"
        "<button class='ab btnA' data-m='4'>A</button>"
        "</div>"
        "</div></div></div>")); // .pad .wrap .main

    _srv.sendContent(F("<script>"
        "var C=document.getElementById('scr'),ctx=C.getContext('2d');"
        "var NF=document.getElementById('nf'),fpEl=document.getElementById('fp');"
        "var fc=0,ft=0,busy=0,mask=0,stimer=null;"
        // Масштаб canvas под контейнер
        "function rsz(){"
          "var p=C.parentElement;"
          "var s=Math.max(1,Math.min(3,"
            "Math.floor((p.offsetWidth-24)/C.width),"
            "Math.floor((p.offsetHeight-120)/C.height)));"
          "C.style.width=(C.width*s)+'px';C.style.height=(C.height*s)+'px';}"
        "rsz();window.addEventListener('resize',rsz);"
        // Опрос экрана
        "var menuW=320,menuH=240;"   // SCREEN_W × SCREEN_H
        "function poll(){"
          "if(busy)return;busy=1;"
          "fetch('/screen').then(function(r){"
            "if(r.status===204){"
              "NF.style.display='';C.classList.remove('live');"
              "fpEl.textContent='&#9679; no signal';"
              "busy=0;setTimeout(poll,2000);return null;"
            "}"
            "return r.arrayBuffer();"
          "}).then(function(b){"
            "if(!b)return;"
            "if(b.byteLength<4){busy=0;setTimeout(poll,500);return;}"
            "var v=new DataView(b);"
            "var w=v.getUint16(0,true),h=v.getUint16(2,true);"
            "var isMenu=(w===menuW&&h===menuH);"
            "if(w>0&&h>0&&b.byteLength>=4+w*h*2){"
              "var px=new Uint16Array(b,4,w*h);"
              "if(C.width!==w||C.height!==h){C.width=w;C.height=h;rsz();}"
              "var id=ctx.createImageData(w,h),d=id.data;"
              "for(var i=0;i<w*h;i++){"
                "var p=px[i];"
                "d[i*4]=(p>>8)&248;"
                "d[i*4+1]=(p>>3)&252;"
                "d[i*4+2]=(p<<3)&248;"
                "d[i*4+3]=255;}"
              "ctx.putImageData(id,0,0);"
              "C.classList.add('live');NF.style.display='none';"
              "fc++;var n=Date.now();"
              "if(n-ft>=1000){"
                "fpEl.textContent=(isMenu?'&#128193; menu ':'')+fc+'fps';"
                "fc=0;ft=n;}"
            "}"
            "busy=0;setTimeout(poll,isMenu?500:200);"
          "}).catch(function(){"
            "busy=0;fpEl.textContent='&#10005; error';setTimeout(poll,2000);});"
        "}"
        "poll();"
        // Отправка кнопок
        "function sendM(){"
          "fetch('/input?mask='+mask).catch(function(){});"
          "if(mask)stimer=setTimeout(sendM,150);else stimer=null;"
        "}"
        "function setMask(m){"
          "mask=m;if(stimer)clearTimeout(stimer);sendM();"
        "}"
        // Обработчики кнопок
        "document.querySelectorAll('[data-m]').forEach(function(b){"
          "var m=+b.dataset.m;"
          "var pr=function(){mask|=m;b.classList.add('on');setMask(mask);};"
          "var rl=function(){mask&=~m;b.classList.remove('on');setMask(mask);};"
          "b.addEventListener('mousedown',pr);"
          "b.addEventListener('touchstart',function(e){e.preventDefault();pr();},{passive:false});"
          "document.addEventListener('mouseup',rl);"
          "document.addEventListener('touchend',rl);"
        "});"
        // Клавиатура
        "var KM={"
          "'ArrowUp':16,'ArrowDown':32,'ArrowLeft':64,'ArrowRight':128,"
          "'w':16,'s':32,'a':64,'d':128,"
          "'z':4,'x':8,'Enter':1,'Shift':2};"
        "document.addEventListener('keydown',function(e){"
          "var m=KM[e.key];if(!m)return;e.preventDefault();"
          "mask|=m;setMask(mask);"
          "var b=document.querySelector('[data-m=\"'+m+'\"]');if(b)b.classList.add('on');});"
        "document.addEventListener('keyup',function(e){"
          "var m=KM[e.key];if(!m)return;"
          "mask&=~m;setMask(mask);"
          "var b=document.querySelector('[data-m=\"'+m+'\"]');if(b)b.classList.remove('on');});"
        "</script></body></html>"));
    _srv.sendContent("");
}

// ── 404 ────────────────────────────────────────────────────────
static void handleNotFound() {
    _srv.send(404, "text/plain", "Not found");
}

// ══════════════════════════════════════════════════════════════
// PUBLIC API
// ══════════════════════════════════════════════════════════════

// ── Фоновый task: handleClient без зависимости от loop() ─────────────────────
// Нужен потому что emu_run() блокирует loop() на всё время игры,
// из-за чего webDbgHandle() не вызывается и браузер не получает ответов.
static void _dbgServerTask(void*) {
    while (_running && _debugOnly) {
        _srv.handleClient();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    _dbgTask = nullptr;
    vTaskDelete(nullptr);
}

// ── Web Debug: минимальный сервер только с /console ───────────────────────────
bool webDbgStart() {
    if (_running) return false;   // уже запущен (менеджер или debug)
    IPAddress ip = WiFi.localIP();
    snprintf(_ip, sizeof(_ip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    _srv.on("/console",     HTTP_GET, handleConsole);
    _srv.on("/console/log", HTTP_GET, handleConsoleLog);
    _srv.on("/remote",      HTTP_GET, handleRemote);
    _srv.on("/screen",      HTTP_GET, handleScreen);
    _srv.on("/input",       HTTP_GET, handleInput);
    _srv.onNotFound([](){ _srv.send(200, "text/html",
        "<meta http-equiv='refresh' content='0;url=/remote'>"); });
    _srv.begin();
    _running   = true;
    _debugOnly = true;
    // Запускаем фоновый task на Core 0 (приоритет 1) —
    // обрабатывает HTTP пока emu_run() блокирует loop().
    xTaskCreatePinnedToCore(_dbgServerTask, "webdbg", 6144, nullptr, 1, &_dbgTask, 0);
    webConsolePrintf("[DBG] Remote at http://%s/remote\n", _ip);
    return true;
}

void webDbgStop() {
    if (!_running || !_debugOnly) return;
    _running   = false;  // сигнал task-у завершиться
    _debugOnly = false;
    // Ждём завершения task-а (максимум 200 мс)
    for (int i = 0; i < 40 && _dbgTask; i++) vTaskDelay(pdMS_TO_TICKS(5));
    if (_dbgTask) { vTaskDelete(_dbgTask); _dbgTask = nullptr; }
    _srv.stop();
    _ip[0] = '\0';
    webConsolePrintf("[DBG] Console stopped\n");
}

bool webDbgRunning() { return _running && _debugOnly; }

void webDbgHandle() {
    // Task теперь обрабатывает клиентов самостоятельно.
    // Этот вызов из loop() — дополнительная обработка между тактами task-а.
    if (_running && _debugOnly && !_dbgTask) _srv.handleClient();
}

const char* webDbgIP() { return _ip; }

// ══════════════════════════════════════════════════════════════
void webMgrStart() {
    if (_running && _debugOnly) webDbgStop();  // освобождаем порт под полный менеджер
    if (_running) return;

    IPAddress ip = WiFi.localIP();
    snprintf(_ip, sizeof(_ip), "%d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);

    _srv.on("/",            HTTP_GET,  handleRoot);
    _srv.on("/upload",      HTTP_POST, handleUploadPage, handleUpload);
    _srv.on("/delete",      HTTP_GET,  handleDelete);
    _srv.on("/mkdir",       HTTP_GET,  handleMkdir);
    _srv.on("/rmdir",       HTTP_GET,  handleRmdir);
    _srv.on("/sd",          HTTP_GET,  handleSDFile);
    _srv.on("/rename",      HTTP_GET,  handleRename);
    _srv.on("/console",     HTTP_GET,  handleConsole);
    _srv.on("/console/log", HTTP_GET,  handleConsoleLog);
    _srv.on("/remote",      HTTP_GET,  handleRemote);
    _srv.on("/screen",      HTTP_GET,  handleScreen);
    _srv.on("/input",       HTTP_GET,  handleInput);
    _srv.onNotFound(handleNotFound);
    _srv.begin();
    _running = true;

    Serial.printf("[WEB] Server started at http://%s\n", _ip);
}

void webMgrStop() {
    if (!_running) return;
    _srv.stop();
    _running = false;
    _ip[0] = '\0';
    Serial.println("[WEB] Server stopped");
}

void webMgrHandle() {
    if (_running) _srv.handleClient();
}

bool webMgrRunning() { return _running; }

const char* webMgrIP() { return _ip; }
