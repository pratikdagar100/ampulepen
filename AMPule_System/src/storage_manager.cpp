#include "storage_manager.h"
#include <LittleFS.h>
#include "config.h"

namespace StorageManager {

bool begin() {
    if (!LittleFS.begin(true)) { // true = format on mount failure
        Serial.println("[STORAGE] LittleFS mount FAILED");
        return false;
    }
    Serial.println("[STORAGE] LittleFS mounted");
    return true;
}

bool exists(const char *path) {
    return LittleFS.exists(path);
}

bool readJson(const char *path, JsonDocument &doc) {
    doc.clear();
    if (!LittleFS.exists(path)) {
        return false;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        Serial.printf("[STORAGE] Failed to open %s for read\n", path);
        return false;
    }
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[STORAGE] JSON parse error in %s: %s\n", path, err.c_str());
        doc.clear();
        return false;
    }
    return true;
}

bool writeJson(const char *path, JsonDocument &doc) {
    String tmpPath = String(path) + ".tmp";
    File f = LittleFS.open(tmpPath, "w");
    if (!f) {
        Serial.printf("[STORAGE] Failed to open %s for write\n", tmpPath.c_str());
        return false;
    }
    size_t written = serializeJson(doc, f);
    f.close();
    if (written == 0) {
        Serial.printf("[STORAGE] Zero bytes written to %s\n", tmpPath.c_str());
        LittleFS.remove(tmpPath);
        return false;
    }
    // Atomic-ish swap: remove old file, rename temp into place.
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
    }
    if (!LittleFS.rename(tmpPath, path)) {
        Serial.printf("[STORAGE] Failed to rename %s -> %s\n", tmpPath.c_str(), path);
        return false;
    }
    return true;
}

static void seedMedicines() {
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();

    JsonObject pcm = arr.createNestedObject();
    pcm["id"] = "PCM";
    pcm["name"] = "Paracetamol (PCM)";
    pcm["under40"] = 100;
    pcm["kg41to60"] = 200;
    pcm["kg61to80"] = 300;
    pcm["over81"] = 400;

    JsonObject pan = arr.createNestedObject();
    pan["id"] = "PAN";
    pan["name"] = "Pantoprazole";
    pan["under40"] = 50;
    pan["kg41to60"] = 100;
    pan["kg61to80"] = 150;
    pan["over81"] = 200;

    JsonObject rif = arr.createNestedObject();
    rif["id"] = "RIF";
    rif["name"] = "Rifampicin";
    rif["under40"] = 75;
    rif["kg41to60"] = 150;
    rif["kg61to80"] = 225;
    rif["over81"] = 300;

    writeJson(FILE_MEDICINES, doc);
    Serial.println("[STORAGE] Seeded demo medicines.json");
}

static void seedAmpules() {
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();

    JsonObject a1 = arr.createNestedObject();
    a1["uid"] = "A4327B19";
    a1["medicineId"] = "PCM";
    a1["batch"] = "PCM001";
    a1["expiry"] = "2027-12-31";
    a1["used"] = false;

    JsonObject a2 = arr.createNestedObject();
    a2["uid"] = "8391427A";
    a2["medicineId"] = "PAN";
    a2["batch"] = "PAN001";
    a2["expiry"] = "2027-08-20";
    a2["used"] = false;

    JsonObject a3 = arr.createNestedObject();
    a3["uid"] = "29185C11";
    a3["medicineId"] = "RIF";
    a3["batch"] = "RIF001";
    a3["expiry"] = "2027-10-15";
    a3["used"] = false;

    writeJson(FILE_AMPULES, doc);
    Serial.println("[STORAGE] Seeded demo ampules.json");
}

static void seedHistory() {
    StaticJsonDocument<64> doc;
    doc.to<JsonArray>();
    writeJson(FILE_HISTORY, doc);
    Serial.println("[STORAGE] Initialized empty history.json");
}

static void seedConfig() {
    StaticJsonDocument<256> doc;
    doc["apSsid"] = WIFI_AP_SSID_DEFAULT;
    doc["apPassword"] = WIFI_AP_PASSWORD_DEFAULT;
    doc["staSsid"] = WIFI_STA_SSID_DEFAULT;
    doc["staPassword"] = WIFI_STA_PASSWORD_DEFAULT;
    doc["adminPin"] = ADMIN_PIN_DEFAULT;
    writeJson(FILE_CONFIG, doc);
    Serial.println("[STORAGE] Seeded default config.json");
}

void ensureSeedData() {
    if (!exists(FILE_MEDICINES)) seedMedicines();
    if (!exists(FILE_AMPULES))   seedAmpules();
    if (!exists(FILE_HISTORY))   seedHistory();
    if (!exists(FILE_CONFIG))    seedConfig();
}

} // namespace StorageManager
