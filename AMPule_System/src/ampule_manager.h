#ifndef AMPULE_AMPULE_MANAGER_H
#define AMPULE_AMPULE_MANAGER_H

#include <Arduino.h>
#include "models.h"

// Owns the registered-ampule database (/ampules.json) and the activity
// history log (/history.json). Handles UID lookup, expiry evaluation and
// persistent used-status tracking.
namespace AmpuleManager {

    bool begin();

    int count();
    bool getByIndex(int index, AmpuleRecord &out);
    bool findByUID(const String &uid, AmpuleRecord &out);

    // Parses an "YYYY-MM-DD" expiry string and compares against the current
    // epoch (see system_manager's clock). Returns true if expired.
    // If `currentEpochAvailable` is false, this returns false (cannot
    // determine) — callers must check that flag before trusting the result.
    bool isExpired(const String &expiryDate, time_t currentEpoch, bool currentEpochAvailable);

    // Marks the ampule (by UID) as used and persists it. Returns false if
    // the UID is not found.
    bool markUsed(const String &uid);

    // Admin operations
    bool addAmpule(const String &uid, const String &medicineId, const String &batch, const String &expiry);
    bool deleteAmpule(const String &uid);
    bool resetUsed(const String &uid);

    bool save();

    // ---- Activity history -------------------------------------------------
    void addHistoryEntry(const String &timestamp, const String &uid, const String &medicine,
                          const String &weight, int dose, const String &status);
    int historyCount();
    bool getHistoryByIndex(int index, HistoryEntry &out); // 0 = most recent
}

#endif // AMPULE_AMPULE_MANAGER_H
