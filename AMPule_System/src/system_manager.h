#ifndef AMPULE_SYSTEM_MANAGER_H
#define AMPULE_SYSTEM_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "models.h"

// The central state machine. Owns the active AmpuleSession and coordinates
// every other manager (RFID, display, buttons, ampule/medicine/dose,
// storage) plus the web server. system_manager.h/.cpp is the only place
// that knows the full workflow end-to-end.
namespace SystemManager {

    // Full boot sequence: GPIO, OLED, I2C, PN532, LittleFS, data load,
    // Wi-Fi AP (+ optional STA/NTP), web server. Logs progress to Serial.
    void begin();

    // Call once per loop() iteration. Runs the state machine, services
    // buttons/RFID, and pumps the web server.
    void update();

    SystemState getState();
    const AmpuleSession &getSession();

    // Hardware / subsystem health, for the dashboard's status indicators.
    bool isRfidReady();
    bool isOledReady();
    bool isStorageReady();

    // Time source used for expiry checks: "NTP" or "BUILD_FALLBACK".
    String getTimeSource();

    String getApIp();
    String getApSsid();

    // Error state detail (title/message) matching whatever error state is
    // currently active; empty strings if not in an error state.
    String getErrorTitle();
    String getErrorMessage();

    // Lets the web dashboard drive the same state machine physical buttons
    // do. Routed through the identical update() logic each tick so the
    // dashboard can never produce a state inconsistent with the hardware.
    // which: "up" | "down" | "enter"
    void triggerVirtualButton(const String &which);

    // Populates a JSON object with the full live status (used by /api/status).
    void fillStatusJson(JsonDocument &doc);

    // Populates a JSON object with subsystem health (used by /api/system).
    void fillSystemJson(JsonDocument &doc);
}

#endif // AMPULE_SYSTEM_MANAGER_H
