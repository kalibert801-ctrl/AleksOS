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
#include "config.h"
#include "settings.h"
#include "display/display_manager.h"
#include <WebServer.h>
#include <SD.h>
#include <WiFi.h>
#include <Arduino.h>

static WebServer _srv(80);
static bool      _running = false;
static char      _ip[20]  = {};

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

// ── Главная страница — двухпанельный файл-менеджер ────────────
static void handleRoot() {
    // Выбранная папка из GET-параметра ?dir=
    String curPath = _srv.hasArg("dir") ? _srv.arg("dir") : String(_dirs[0].path);
    // Найдём её индекс
    int curIdx = 0;
    for (int i = 0; i < _ndirs; i++) {
        if (curPath == _dirs[i].path) { curIdx = i; break; }
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
        "</style></head><body>"));

    // ── Шапка
    _srv.sendContent(F("<div class='hdr'><h1>&#127918; AleksOS Web Manager</h1>"
        "<span class='ver'>"));
    _srv.sendContent(FIRMWARE_VERSION);
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
    _srv.sendContent(F("</nav>"));

    // ── Правая панель
    _srv.sendContent(F("<div class='content'>"));

    // Тулбар: Upload + New Folder
    _srv.sendContent(F("<div class='toolbar'>"));

    // Upload form
    _srv.sendContent(F("<form method='POST' action='/upload' enctype='multipart/form-data'>"));
    _srv.sendContent("<input type='hidden' name='dir' value='");
    _srv.sendContent(curPath);
    _srv.sendContent(F("'>"
        "<input type='file' name='file' multiple>"
        "<button class='btn-up' type='submit'>&#8593; Upload</button>"
        "</form>"));

    // New folder form
    _srv.sendContent(F("<form method='GET' action='/mkdir'>"
        "<input type='hidden' name='base' value='"));
    _srv.sendContent(curPath);
    _srv.sendContent(F("'>"
        "<input type='text' name='name' placeholder='New folder...'>"
        "<button class='btn-new' type='submit'>+ New</button>"
        "</form></div>"));

    // Список файлов
    _srv.sendContent(F("<div class='files'>"));
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
                // Кнопка удаления папки
                String delPath = curPath + "/" + name;
                _srv.sendContent("<form method='GET' action='/rmdir' style='margin:0'>"
                    "<input type='hidden' name='path' value='" + delPath + "'>"
                    "<button class='del' onclick=\"return confirm('Delete folder " + name + "?')\">&#10005;</button>"
                    "</form>");
            } else {
                _srv.sendContent("<span class='fn'>&#128196; ");
                _srv.sendContent(name);
                _srv.sendContent("</span><span class='sz'>");
                _srv.sendContent(_fmtSize(sz));
                _srv.sendContent("</span>");
                String delPath = curPath + "/" + name;
                _srv.sendContent("<form method='GET' action='/delete' style='margin:0'>"
                    "<input type='hidden' name='path' value='" + delPath + "'>"
                    "<button class='del' onclick=\"return confirm('Delete " + name + "?')\">&#10005;</button>"
                    "</form>");
            }
            _srv.sendContent("</div>");
            cnt++;
        }
        dir.close();
    }
    if (cnt == 0) _srv.sendContent(F("<p class='empty'>Folder is empty</p>"));

    _srv.sendContent(F("</div></div></div></body></html>"));
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

// ── 404 ────────────────────────────────────────────────────────
static void handleNotFound() {
    _srv.send(404, "text/plain", "Not found");
}

// ══════════════════════════════════════════════════════════════
// PUBLIC API
// ══════════════════════════════════════════════════════════════

void webMgrStart() {
    if (_running) return;

    IPAddress ip = WiFi.localIP();
    snprintf(_ip, sizeof(_ip), "%d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);

    _srv.on("/",       HTTP_GET,  handleRoot);
    _srv.on("/upload", HTTP_POST, handleUploadPage, handleUpload);
    _srv.on("/delete", HTTP_GET,  handleDelete);
    _srv.on("/mkdir",  HTTP_GET,  handleMkdir);
    _srv.on("/rmdir",  HTTP_GET,  handleRmdir);
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
