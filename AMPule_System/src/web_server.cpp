#include "web_server.h"
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage_manager.h"
#include "medicine_manager.h"
#include "ampule_manager.h"
#include "system_manager.h"

namespace WebServerManager {

static WebServer server(HTTP_PORT);

// ---------------------------------------------------------------------------
// Static file serving from LittleFS (the data/ folder is uploaded here)
// ---------------------------------------------------------------------------

static String getContentType(const String &path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".ico"))  return "image/x-icon";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    return "text/plain";
}

static bool handleFileRead(String path) {
    if (path.endsWith("/")) path += "index.html";
    if (!LittleFS.exists(path)) return false;

    File file = LittleFS.open(path, "r");
    if (!file) return false;
    server.streamFile(file, getContentType(path));
    file.close();
    return true;
}

static void handleNotFound() {
    if (handleFileRead(server.uri())) return;
    server.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------
// Admin PIN check
// ---------------------------------------------------------------------------

static String getAdminPin() {
    StaticJsonDocument<256> cfg;
    if (StorageManager::readJson(FILE_CONFIG, cfg)) {
        return String((const char *)(cfg["adminPin"] | ADMIN_PIN_DEFAULT));
    }
    return ADMIN_PIN_DEFAULT;
}

static bool checkAdminPin(JsonDocument &body) {
    String provided = body["pin"] | "";
    return provided.length() > 0 && provided == getAdminPin();
}

static void sendJson(JsonDocument &doc, int code = 200) {
    String out;
    serializeJson(doc, out);
    server.send(code, "application/json", out);
}

static void sendError(int code, const String &message) {
    StaticJsonDocument<128> doc;
    doc["error"] = message;
    sendJson(doc, code);
}

static bool parseBody(JsonDocument &doc) {
    if (!server.hasArg("plain")) return false;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    return !err;
}

// ---------------------------------------------------------------------------
// REST API handlers
// ---------------------------------------------------------------------------

static void handleGetStatus() {
    StaticJsonDocument<768> doc;
    SystemManager::fillStatusJson(doc);
    sendJson(doc);
}

static void handleGetAmpule() {
    const AmpuleSession &s = SystemManager::getSession();
    StaticJsonDocument<512> doc;
    doc["uid"] = s.uid;
    doc["medicine"] = s.medicineName;
    doc["batch"] = s.batch;
    doc["expiry"] = s.expiry;
    doc["verified"] = s.verified;
    doc["expired"] = s.expired;
    doc["used"] = s.used;
    sendJson(doc);
}

static void handleGetSystem() {
    StaticJsonDocument<512> doc;
    SystemManager::fillSystemJson(doc);
    sendJson(doc);
}

static void handleGetMedicines() {
    StaticJsonDocument<1536> doc;
    JsonArray arr = doc.to<JsonArray>();
    MedicineRecord m;
    for (int i = 0; MedicineManager::getByIndex(i, m); i++) {
        JsonObject o = arr.createNestedObject();
        o["id"] = m.id;
        o["name"] = m.name;
        o["under40"] = m.doses.under40;
        o["kg41to60"] = m.doses.kg41to60;
        o["kg61to80"] = m.doses.kg61to80;
        o["over81"] = m.doses.over81;
        o["demo"] = true;
    }
    sendJson(doc);
}

static void handlePostMedicines() {
    StaticJsonDocument<512> body;
    if (!parseBody(body)) { sendError(400, "Invalid JSON body"); return; }
    if (!checkAdminPin(body)) { sendError(401, "Invalid admin PIN"); return; }

    String id = body["id"] | "";
    if (id.length() == 0) { sendError(400, "Missing medicine id"); return; }

    DoseProfile p;
    p.under40  = body["under40"]  | -1;
    p.kg41to60 = body["kg41to60"] | -1;
    p.kg61to80 = body["kg61to80"] | -1;
    p.over81   = body["over81"]   | -1;

    if (p.under40 < 0 || p.kg41to60 < 0 || p.kg61to80 < 0 || p.over81 < 0) {
        sendError(400, "Dose values must be non-negative numbers");
        return;
    }

    if (!MedicineManager::updateDoseProfile(id, p)) {
        sendError(404, "Unknown medicine id");
        return;
    }

    StaticJsonDocument<64> ok;
    ok["ok"] = true;
    sendJson(ok);
}

static void handleGetHistory() {
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    HistoryEntry h;
    for (int i = 0; AmpuleManager::getHistoryByIndex(i, h); i++) {
        JsonObject o = arr.createNestedObject();
        o["time"] = h.timestamp;
        o["uid"] = h.uid;
        o["medicine"] = h.medicine;
        o["weight"] = h.weight;
        o["dose"] = h.dose;
        o["status"] = h.status;
    }
    sendJson(doc);
}

static void handleGetAmpules() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    AmpuleRecord a;
    for (int i = 0; AmpuleManager::getByIndex(i, a); i++) {
        MedicineRecord m;
        MedicineManager::getById(a.medicineId, m);
        JsonObject o = arr.createNestedObject();
        o["uid"] = a.uid;
        o["medicineId"] = a.medicineId;
        o["medicineName"] = m.name.length() ? m.name : a.medicineId;
        o["batch"] = a.batch;
        o["expiry"] = a.expiry;
        o["used"] = a.used;
    }
    sendJson(doc);
}

static void handlePostAmpules() {
    StaticJsonDocument<512> body;
    if (!parseBody(body)) { sendError(400, "Invalid JSON body"); return; }
    if (!checkAdminPin(body)) { sendError(401, "Invalid admin PIN"); return; }

    String uid = body["uid"] | "";
    String medicineId = body["medicineId"] | "";
    String batch = body["batch"] | "";
    String expiry = body["expiry"] | "";

    if (uid.length() == 0 || medicineId.length() == 0 || expiry.length() == 0) {
        sendError(400, "uid, medicineId and expiry are required");
        return;
    }

    MedicineRecord m;
    if (!MedicineManager::getById(medicineId, m)) {
        sendError(400, "Unknown medicineId");
        return;
    }

    if (!AmpuleManager::addAmpule(uid, medicineId, batch, expiry)) {
        sendError(409, "UID already registered or database full");
        return;
    }

    StaticJsonDocument<64> ok;
    ok["ok"] = true;
    sendJson(ok);
}

static void handleDeleteAmpule() {
    StaticJsonDocument<256> body;
    parseBody(body); // DELETE with JSON body is allowed by ESP32 WebServer
    String uid = server.hasArg("uid") ? server.arg("uid") : (body["uid"] | "");
    if (!checkAdminPin(body)) { sendError(401, "Invalid admin PIN"); return; }
    if (uid.length() == 0) { sendError(400, "Missing uid"); return; }
    if (!AmpuleManager::deleteAmpule(uid)) { sendError(404, "UID not found"); return; }
    StaticJsonDocument<64> ok;
    ok["ok"] = true;
    sendJson(ok);
}

static void handlePostResetUsed() {
    StaticJsonDocument<256> body;
    if (!parseBody(body)) { sendError(400, "Invalid JSON body"); return; }
    if (!checkAdminPin(body)) { sendError(401, "Invalid admin PIN"); return; }
    String uid = body["uid"] | "";
    if (uid.length() == 0) { sendError(400, "Missing uid"); return; }
    if (!AmpuleManager::resetUsed(uid)) { sendError(404, "UID not found"); return; }
    StaticJsonDocument<64> ok;
    ok["ok"] = true;
    sendJson(ok);
}

// Dashboard "virtual button" control — mirrors the physical UP/DOWN/ENTER
// buttons through the exact same state machine tick, so it can never desync
// from the hardware. Clearly a secondary/testing input, not a replacement.
static void handlePostControl() {
    StaticJsonDocument<128> body;
    if (!parseBody(body)) { sendError(400, "Invalid JSON body"); return; }
    String action = body["action"] | "";
    if (action != "up" && action != "down" && action != "enter") {
        sendError(400, "action must be up, down or enter");
        return;
    }
    SystemManager::triggerVirtualButton(action);
    StaticJsonDocument<64> ok;
    ok["ok"] = true;
    sendJson(ok);
}

static void handleGetSettings() {
    StaticJsonDocument<256> cfg;
    StaticJsonDocument<256> doc;
    if (StorageManager::readJson(FILE_CONFIG, cfg)) {
        doc["apSsid"] = cfg["apSsid"] | WIFI_AP_SSID_DEFAULT;
        String staSsid = cfg["staSsid"] | "";
        doc["staSsid"] = staSsid;
        doc["staConfigured"] = staSsid.length() > 0;
    }
    sendJson(doc);
}

static void handlePostSettings() {
    StaticJsonDocument<512> body;
    if (!parseBody(body)) { sendError(400, "Invalid JSON body"); return; }
    if (!checkAdminPin(body)) { sendError(401, "Invalid admin PIN"); return; }

    StaticJsonDocument<512> cfg;
    StorageManager::readJson(FILE_CONFIG, cfg);

    if (body.containsKey("apSsid"))      cfg["apSsid"] = body["apSsid"];
    if (body.containsKey("apPassword"))  cfg["apPassword"] = body["apPassword"];
    if (body.containsKey("staSsid"))     cfg["staSsid"] = body["staSsid"];
    if (body.containsKey("staPassword")) cfg["staPassword"] = body["staPassword"];
    if (body.containsKey("newPin"))      cfg["adminPin"] = body["newPin"];

    if (!StorageManager::writeJson(FILE_CONFIG, cfg)) {
        sendError(500, "Failed to save settings");
        return;
    }

    StaticJsonDocument<128> ok;
    ok["ok"] = true;
    ok["note"] = "Wi-Fi changes take effect after reboot";
    sendJson(ok);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

static void setupRoutes() {
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/ampule", HTTP_GET, handleGetAmpule);
    server.on("/api/system", HTTP_GET, handleGetSystem);

    server.on("/api/medicines", HTTP_GET, handleGetMedicines);
    server.on("/api/medicines", HTTP_POST, handlePostMedicines);

    server.on("/api/history", HTTP_GET, handleGetHistory);

    server.on("/api/ampules", HTTP_GET, handleGetAmpules);
    server.on("/api/ampules", HTTP_POST, handlePostAmpules);
    server.on("/api/ampules", HTTP_DELETE, handleDeleteAmpule);

    server.on("/api/reset-used", HTTP_POST, handlePostResetUsed);
    server.on("/api/control", HTTP_POST, handlePostControl);

    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/settings", HTTP_POST, handlePostSettings);

    server.onNotFound(handleNotFound);
}

void begin() {
    setupRoutes();
    server.begin();
}

void handleClient() {
    server.handleClient();
}

} // namespace WebServerManager
