// ============================================================================
// Ampule Verification & Dose Selection System
// ESP32 + DFR0231-H (PN532) + I2C OLED + 3 buttons + local Wi-Fi dashboard
//
// PROTOTYPE / ACADEMIC DEMONSTRATION ONLY.
// Dose values are DEMO VALUES and must never be used for real medical dosing.
//
// All real logic lives in src/*.cpp, orchestrated by system_manager.
// This file only performs the standard Arduino setup()/loop() handoff.
// ============================================================================

#include <Arduino.h>
#include "system_manager.h"

void setup() {
    Serial.begin(115200);
    delay(200); // let USB-serial settle so the first boot logs aren't lost
    SystemManager::begin();
}

void loop() {
    SystemManager::update();
}
