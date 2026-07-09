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

// ── HTML части (PROGMEM) ───────────────────────────────────────
static const char _CSS[] PROGMEM = R"css(
body{background:#1a1a1a;color:#eee;font-family:sans-serif;margin:0;padding:12px;max-width:640px;margin:auto}
h1{color:#f80;margin:0 0 4px;font-size:20px}
.ver{color:#666;font-size:11px;margin-bottom:16px}
.card{background:#252525;border-radius:8px;padding:14px;margin:12px 0}
h2{color:#f80;margin:0 0 10px;font-size:15px}
select,input[type=file]{width:100%;box-sizing:border-box;padding:8px;margin:6px 0;
 background:#333;color:#eee;border:1px solid #555;border-radius:4px;font-size:14px}
.btn{display:block;width:100%;box-sizing:border-box;padding:10px;background:#e67e00;
 color:#fff;border:none;border-radius:4px;font-size:15px;cursor:pointer;margin-top:8px}
.btn:hover{background:#f90}
.del{background:#8b0000;color:#fff;border:none;padding:4px 10px;
 border-radius:4px;cursor:pointer;font-size:12px}
.del:hover{background:#b00}
.fi{display:flex;align-items:center;padding:6px 2px;border-top:1px solid #2e2e2e}
.fn{flex:1;font-size:13px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;max-width:360px}
.sz{color:#888;font-size:11px;margin:0 8px}
.ok{color:#2ecc71;font-weight:bold}
.err{color:#e74c3c;font-weight:bold}
)css";

// ── Главная страница ───────────────────────────────────────────
static void handleRoot() {
    _srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _srv.send(200, "text/html; charset=utf-8", "");

    // head
    _srv.sendContent(F("<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>RetroESP</title><style>"));
    _srv.sendContent(_CSS);
    _srv.sendContent(F("</style></head><body>"
        "<h1>&#127918; RetroESP File Manager</h1>"
        "<div class='ver'>"));
    _srv.sendContent(FIRMWARE_VERSION);
    _srv.sendContent(F("</div>"));

    // Форма загрузки
    _srv.sendContent(F(
        "<div class='card'><h2>Upload File</h2>"
        "<form method='POST' action='/upload' enctype='multipart/form-data'>"
        "<select name='dir'>"));
    for (int i = 0; i < _ndirs; i++) {
        _srv.sendContent("<option value='");
        _srv.sendContent(_dirs[i].path);
        _srv.sendContent("'>");
        _srv.sendContent(_dirs[i].name);
        _srv.sendContent(" (");
        _srv.sendContent(_dirs[i].path);
        _srv.sendContent(")</option>");
    }
    _srv.sendContent(F(
        "</select>"
        "<input type='file' name='file' multiple>"
        "<button class='btn' type='submit'>&#8593; Upload</button>"
        "</form></div>"));

    // Список файлов по директориям
    for (int d = 0; d < _ndirs; d++) {
        File dir = SD.open(_dirs[d].path);
        if (!dir || !dir.isDirectory()) continue;

        _srv.sendContent("<div class='card'><h2>");
        _srv.sendContent(_dirs[d].name);
        _srv.sendContent(" <small style='color:#666'>");
        _srv.sendContent(_dirs[d].path);
        _srv.sendContent("</small></h2>");

        int cnt = 0;
        while (true) {
            File f = dir.openNextFile();
            if (!f) break;
            if (f.isDirectory()) { f.close(); continue; }

            String name = String(f.name());
            // basename
            int sl = name.lastIndexOf('/');
            if (sl >= 0) name = name.substring(sl + 1);
            size_t sz = f.size();
            f.close();

            _srv.sendContent("<div class='fi'><span class='fn'>");
            _srv.sendContent(name);
            _srv.sendContent("</span><span class='sz'>");
            _srv.sendContent(_fmtSize(sz));
            _srv.sendContent("</span>"
                "<form method='GET' action='/delete' style='margin:0'>"
                "<input type='hidden' name='path' value='");
            _srv.sendContent(_dirs[d].path);
            _srv.sendContent("/");
            _srv.sendContent(name);
            _srv.sendContent("'>"
                "<button class='del' type='submit' "
                "onclick=\"return confirm('Delete " + name + "?')\">&#10005;</button>"
                "</form></div>");
            cnt++;
        }
        dir.close();

        if (cnt == 0) {
            _srv.sendContent("<p style='color:#666;font-size:13px;margin:4px 0'>Empty</p>");
        }
        _srv.sendContent("</div>");
    }

    _srv.sendContent(F("</body></html>"));
    _srv.sendContent("");  // закрываем chunked
}

// ── Загрузка файла (multipart) ─────────────────────────────────
static void handleUploadPage() {
    if (_uploadOk)
        _srv.send(200, "text/html; charset=utf-8",
            "<html><head><meta charset='utf-8'>"
            "<meta http-equiv='refresh' content='2;url=/'></head><body>"
            "<p class='ok' style='color:#2ecc71;font-family:sans-serif;padding:16px'>"
            "&#10003; Uploaded successfully. Redirecting...</p></body></html>");
    else
        _srv.send(500, "text/html; charset=utf-8",
            "<html><body style='font-family:sans-serif;background:#1a1a1a;color:#e74c3c;padding:16px'>"
            "Upload failed. <a href='/' style='color:#f80'>Back</a></body></html>");
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
    if (path.length() > 0 && SD.exists(path)) {
        SD.remove(path);
        Serial.printf("[WEB] Deleted: %s\n", path.c_str());
    }
    _srv.sendHeader("Location", "/");
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
