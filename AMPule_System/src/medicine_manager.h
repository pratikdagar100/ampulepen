#ifndef AMPULE_MEDICINE_MANAGER_H
#define AMPULE_MEDICINE_MANAGER_H

#include <Arduino.h>
#include "models.h"

// Owns the in-memory list of medicines (identity + DEMO dose profile) and
// persists it to /medicines.json. This is the single place dose values are
// configured, satisfying the "extremely easy to change" requirement.
namespace MedicineManager {

    bool begin(); // loads from storage (seeding is handled by StorageManager)

    int count();
    bool getByIndex(int index, MedicineRecord &out);
    bool getById(const String &id, MedicineRecord &out);

    // Validates (non-negative integers) and persists an updated dose profile
    // for an existing medicine id. Returns false on invalid input or unknown id.
    bool updateDoseProfile(const String &id, const DoseProfile &newDoses);

    bool save();
}

#endif // AMPULE_MEDICINE_MANAGER_H
