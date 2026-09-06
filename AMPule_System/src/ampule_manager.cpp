#include "ampule_manager.h"
#include "storage_manager.h"
#include "config.h"
#include <time.h>
#include <stdio.h>

namespace AmpuleManager {

static AmpuleRecord ampules[AMPULE_MAX_RECORDS];
static int ampuleCount = 0;

static HistoryEntry history[HISTORY_MAX_ENTRIES];
static int historyCountVal = 0; // number of valid entries, index 0 = most recent

// ---------------------------------------------------------------------------
// Ampule database
// ---------------------------------------------------------------------------

static bool loadAmpules() {
    StaticJsonDocument<4096> doc;
    if (!StorageManager::readJson(FILE_AMPULES, doc)) {
        Serial.println("[AMPULE] Failed to load ampules.json");
        return false;
    }
    ampuleCount = 0;
    for (JsonObject obj : doc.as<JsonArray>()) {
        if (ampuleCount >= AMPULE_MAX_RECORDS) break;
        AmpuleRecord &a = ampules[ampuleCount];
        a.uid = obj["uid"].as<String>();
        a.uid.toUpperCase();
        a.medicineId = obj["medicineId"].as<String>();
        a.batch = obj["batch"].as<String>();
        a.expiry = obj["expiry"].as<String>();
        a.used = obj["used"] | false;
        ampuleCount++;
    }
    Serial.printf("[AMPULE] Loaded %d ampule records\n", ampuleCount);
    return true;
}

static bool loadHistory() {
    StaticJsonDocument<4096> doc;
    if (!StorageManager::readJson(FILE_HISTORY, doc)) {
        return false;
    }
    historyCountVal = 0;
    for (JsonObject obj : doc.as<JsonArray>()) {
        if (historyCountVal >= HISTORY_MAX_ENTRIES) break;
        HistoryEntry &h = history[historyCountVal];
        h.timestamp = obj["time"].as<String>();
        h.uid = obj["uid"].as<String>();
        h.medicine = obj["medicine"].as<String>();
        h.weight = obj["weight"].as<String>();
        h.dose = obj["dose"] | 0;
        h.status = obj["status"].as<String>();
        historyCountVal++;
    }
    Serial.printf("[AMPULE] Loaded %d history entries\n", historyCountVal);
    return true;
}

bool begin() {
    loadHistory();
    return loadAmpules();
}

int count() { return ampuleCount; }

bool getByIndex(int index, AmpuleRecord &out) {
    if (index < 0 || index >= ampuleCount) return false;
    out = ampules[index];
    return true;
}

bool findByUID(const String &uid, AmpuleRecord &out) {
    String u = uid;
    u.toUpperCase();
    for (int i = 0; i < ampuleCount; i++) {
        if (ampules[i].uid == u) {
            out = ampules[i];
            return true;
        }
    }
    return false;
}

bool isExpired(const String &expiryDate, time_t currentEpoch, bool currentEpochAvailable) {
    if (!currentEpochAvailable) {
        return false; // caller must treat "unknown" specially, not as valid
    }
    int y, m, d;
    if (sscanf(expiryDate.c_str(), "%d-%d-%d", &y, &m, &d) != 3) {
        // Malformed expiry data is treated as expired (fail safe).
        return true;
    }
    struct tm expiryTm = {};
    expiryTm.tm_year = y - 1900;
    expiryTm.tm_mon = m - 1;
    expiryTm.tm_mday = d;
    expiryTm.tm_hour = 23;
    expiryTm.tm_min = 59;
    expiryTm.tm_sec = 59;
    time_t expiryEpoch = mktime(&expiryTm);
    return currentEpoch > expiryEpoch;
}

bool markUsed(const String &uid) {
    String u = uid;
    u.toUpperCase();
    for (int i = 0; i < ampuleCount; i++) {
        if (ampules[i].uid == u) {
            ampules[i].used = true;
            return save();
        }
    }
    return false;
}

bool addAmpule(const String &uid, const String &medicineId, const String &batch, const String &expiry) {
    String u = uid;
    u.toUpperCase();
    if (u.length() == 0 || medicineId.length() == 0) return false;
    for (int i = 0; i < ampuleCount; i++) {
        if (ampules[i].uid == u) return false; // already registered
    }
    if (ampuleCount >= AMPULE_MAX_RECORDS) return false;
    AmpuleRecord &a = ampules[ampuleCount];
    a.uid = u;
    a.medicineId = medicineId;
    a.batch = batch;
    a.expiry = expiry;
    a.used = false;
    ampuleCount++;
    return save();
}

bool deleteAmpule(const String &uid) {
    String u = uid;
    u.toUpperCase();
    for (int i = 0; i < ampuleCount; i++) {
        if (ampules[i].uid == u) {
            for (int j = i; j < ampuleCount - 1; j++) {
                ampules[j] = ampules[j + 1];
            }
            ampuleCount--;
            return save();
        }
    }
    return false;
}

bool resetUsed(const String &uid) {
    String u = uid;
    u.toUpperCase();
    for (int i = 0; i < ampuleCount; i++) {
        if (ampules[i].uid == u) {
            ampules[i].used = false;
            return save();
        }
    }
    return false;
}

bool save() {
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < ampuleCount; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["uid"] = ampules[i].uid;
        obj["medicineId"] = ampules[i].medicineId;
        obj["batch"] = ampules[i].batch;
        obj["expiry"] = ampules[i].expiry;
        obj["used"] = ampules[i].used;
    }
    return StorageManager::writeJson(FILE_AMPULES, doc);
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

static bool saveHistory() {
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < historyCountVal; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["time"] = history[i].timestamp;
        obj["uid"] = history[i].uid;
        obj["medicine"] = history[i].medicine;
        obj["weight"] = history[i].weight;
        obj["dose"] = history[i].dose;
        obj["status"] = history[i].status;
    }
    return StorageManager::writeJson(FILE_HISTORY, doc);
}

void addHistoryEntry(const String &timestamp, const String &uid, const String &medicine,
                      const String &weight, int dose, const String &status) {
    // Shift everything down by one (newest stays at index 0), dropping the oldest.
    int last = (historyCountVal >= HISTORY_MAX_ENTRIES) ? HISTORY_MAX_ENTRIES - 1 : historyCountVal;
    for (int i = last; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0].timestamp = timestamp;
    history[0].uid = uid;
    history[0].medicine = medicine;
    history[0].weight = weight;
    history[0].dose = dose;
    history[0].status = status;
    if (historyCountVal < HISTORY_MAX_ENTRIES) historyCountVal++;
    saveHistory();
}

int historyCount() { return historyCountVal; }

bool getHistoryByIndex(int index, HistoryEntry &out) {
    if (index < 0 || index >= historyCountVal) return false;
    out = history[index];
    return true;
}

} // namespace AmpuleManager
