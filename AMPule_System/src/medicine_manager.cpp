#include "medicine_manager.h"
#include "storage_manager.h"
#include "config.h"

namespace MedicineManager {

static MedicineRecord medicines[MEDICINE_MAX_RECORDS];
static int medicineCount = 0;

bool begin() {
    StaticJsonDocument<1536> doc;
    if (!StorageManager::readJson(FILE_MEDICINES, doc)) {
        Serial.println("[MEDICINE] Failed to load medicines.json");
        return false;
    }
    medicineCount = 0;
    for (JsonObject obj : doc.as<JsonArray>()) {
        if (medicineCount >= MEDICINE_MAX_RECORDS) break;
        MedicineRecord &m = medicines[medicineCount];
        m.id = obj["id"].as<String>();
        m.name = obj["name"].as<String>();
        m.doses.under40  = obj["under40"]  | 0;
        m.doses.kg41to60 = obj["kg41to60"] | 0;
        m.doses.kg61to80 = obj["kg61to80"] | 0;
        m.doses.over81   = obj["over81"]   | 0;
        medicineCount++;
    }
    Serial.printf("[MEDICINE] Loaded %d medicines\n", medicineCount);
    return true;
}

int count() { return medicineCount; }

bool getByIndex(int index, MedicineRecord &out) {
    if (index < 0 || index >= medicineCount) return false;
    out = medicines[index];
    return true;
}

bool getById(const String &id, MedicineRecord &out) {
    for (int i = 0; i < medicineCount; i++) {
        if (medicines[i].id == id) {
            out = medicines[i];
            return true;
        }
    }
    return false;
}

bool updateDoseProfile(const String &id, const DoseProfile &newDoses) {
    if (newDoses.under40 < 0 || newDoses.kg41to60 < 0 ||
        newDoses.kg61to80 < 0 || newDoses.over81 < 0) {
        return false;
    }
    for (int i = 0; i < medicineCount; i++) {
        if (medicines[i].id == id) {
            medicines[i].doses = newDoses;
            return save();
        }
    }
    return false;
}

bool save() {
    StaticJsonDocument<1536> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < medicineCount; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = medicines[i].id;
        obj["name"] = medicines[i].name;
        obj["under40"] = medicines[i].doses.under40;
        obj["kg41to60"] = medicines[i].doses.kg41to60;
        obj["kg61to80"] = medicines[i].doses.kg61to80;
        obj["over81"] = medicines[i].doses.over81;
    }
    return StorageManager::writeJson(FILE_MEDICINES, doc);
}

} // namespace MedicineManager
