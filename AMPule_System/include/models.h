#ifndef AMPULE_MODELS_H
#define AMPULE_MODELS_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// Weight categories
// ============================================================================
enum WeightCategory {
    WEIGHT_UNDER_40 = 0,
    WEIGHT_41_60    = 1,
    WEIGHT_61_80    = 2,
    WEIGHT_81_PLUS  = 3,
    WEIGHT_COUNT    = 4
};

inline const char *weightCategoryLabel(WeightCategory w) {
    switch (w) {
        case WEIGHT_UNDER_40: return "UNDER 40 kg";
        case WEIGHT_41_60:    return "41-60 kg";
        case WEIGHT_61_80:    return "61-80 kg";
        case WEIGHT_81_PLUS:  return "81 kg+";
        default:              return "UNKNOWN";
    }
}

// ============================================================================
// System state machine
// ============================================================================
enum SystemState {
    STATE_BOOT = 0,
    STATE_READY,                    // Waiting for an ampule to be inserted/scanned
    STATE_SCANNING,                 // Tag detected, reading/looking up UID
    STATE_VERIFYING,                // Checking expiry / used status
    STATE_AMPULE_ACTIVE,            // Verified, medicine pinned, waiting for ENTER
    STATE_WEIGHT_SELECTION,
    STATE_DOSE_DISPLAY,
    STATE_COMPLETED,                // Dose confirmed, ampule marked used, waiting for removal
    STATE_ERROR_UNREGISTERED,
    STATE_ERROR_EXPIRED,
    STATE_ERROR_USED,
    STATE_ERROR_RFID,
    STATE_ERROR_TIME_UNAVAILABLE
};

inline const char *systemStateName(SystemState s) {
    switch (s) {
        case STATE_BOOT:                    return "SYSTEM_BOOT";
        case STATE_READY:                   return "WAITING_FOR_AMPULE";
        case STATE_SCANNING:                return "RFID_SCANNING";
        case STATE_VERIFYING:               return "VERIFYING_AMPULE";
        case STATE_AMPULE_ACTIVE:           return "AMPULE_ACTIVE";
        case STATE_WEIGHT_SELECTION:        return "WEIGHT_SELECTION";
        case STATE_DOSE_DISPLAY:            return "DOSE_DISPLAY";
        case STATE_COMPLETED:               return "COMPLETED";
        case STATE_ERROR_UNREGISTERED:      return "ERROR_UNREGISTERED";
        case STATE_ERROR_EXPIRED:           return "ERROR_EXPIRED";
        case STATE_ERROR_USED:              return "ERROR_USED";
        case STATE_ERROR_RFID:              return "ERROR_RFID";
        case STATE_ERROR_TIME_UNAVAILABLE:  return "ERROR_TIME_UNAVAILABLE";
        default:                            return "UNKNOWN";
    }
}

inline bool isErrorState(SystemState s) {
    return s == STATE_ERROR_UNREGISTERED || s == STATE_ERROR_EXPIRED ||
           s == STATE_ERROR_USED || s == STATE_ERROR_RFID ||
           s == STATE_ERROR_TIME_UNAVAILABLE;
}

// ============================================================================
// Dose profile — DEMO VALUES ONLY. Edit freely; used by dose_manager.
// ============================================================================
struct DoseProfile {
    int under40;
    int kg41to60;
    int kg61to80;
    int over81;
};

inline int doseForWeight(const DoseProfile &p, WeightCategory w) {
    switch (w) {
        case WEIGHT_UNDER_40: return p.under40;
        case WEIGHT_41_60:    return p.kg41to60;
        case WEIGHT_61_80:    return p.kg61to80;
        case WEIGHT_81_PLUS:  return p.over81;
        default:              return 0;
    }
}

// ============================================================================
// Medicine record (identity + demo dose configuration)
// ============================================================================
struct MedicineRecord {
    String id;          // short internal key, e.g. "PCM"
    String name;        // exact user-facing name, e.g. "Paracetamol (PCM)"
    DoseProfile doses;
};

// ============================================================================
// Registered ampule record (persisted database entry)
// ============================================================================
struct AmpuleRecord {
    String uid;         // normalized uppercase hex, no separators
    String medicineId;  // foreign key into medicine list
    String batch;
    String expiry;      // "YYYY-MM-DD"
    bool used;
};

// ============================================================================
// Active ampule session (in-memory, cleared on removal/reset)
// ============================================================================
struct AmpuleSession {
    bool active = false;
    String uid;
    String medicineId;
    String medicineName;
    String batch;
    String expiry;
    bool verified = false;
    bool expired = false;
    bool used = false;
    bool weightSelected = false;
    WeightCategory weight = WEIGHT_UNDER_40;
    bool doseCalculated = false;
    int dose = 0;

    void clear() {
        active = false;
        uid = "";
        medicineId = "";
        medicineName = "";
        batch = "";
        expiry = "";
        verified = false;
        expired = false;
        used = false;
        weightSelected = false;
        weight = WEIGHT_UNDER_40;
        doseCalculated = false;
        dose = 0;
    }
};

// ============================================================================
// History log entry
// ============================================================================
struct HistoryEntry {
    String timestamp;
    String uid;
    String medicine;
    String weight;
    int dose;
    String status;
};

#endif // AMPULE_MODELS_H
