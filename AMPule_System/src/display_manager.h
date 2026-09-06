#ifndef AMPULE_DISPLAY_MANAGER_H
#define AMPULE_DISPLAY_MANAGER_H

#include <Arduino.h>
#include "models.h"

// Wraps the SSD1306-style OLED (Adafruit_SSD1306 + Adafruit_GFX). Every
// screen keeps the active medicine name pinned at the top whenever a
// session is active, per the "medicine must not disappear" requirement.
namespace DisplayManager {

    bool begin();
    bool isHardwareOk();

    void showBootMessage(const String &line);

    void showReady();                 // "INSERT AMPULE / RFID READY"
    void showScanning();               // "RFID DETECTED / SCANNING..."

    void showVerified(const String &medicineName);

    void showWeightSelection(const String &medicineName, WeightCategory selected);

    void showDose(const String &medicineName, WeightCategory weight, int doseMg);

    void showComplete(const String &medicineName, int doseMg);

    // title e.g. "UNREGISTERED AMPULE", message = recovery instruction
    void showError(const String &title, const String &message);

    void showTimeUnavailable();
}

#endif // AMPULE_DISPLAY_MANAGER_H
