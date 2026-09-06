#ifndef AMPULE_RFID_MANAGER_H
#define AMPULE_RFID_MANAGER_H

#include <Arduino.h>

// Wraps the DFRobot DFR0231-H (PN532, I2C mode) via the Adafruit_PN532
// driver. Provides edge-triggered "new tag" detection and best-effort
// "tag still present" tracking so the caller never has to poll the raw
// hardware directly or worry about re-triggering on the same tag.
namespace RfidManager {

    bool begin();
    bool isHardwareOk();

    // Call every loop() iteration. Internally throttled to
    // RFID_POLL_INTERVAL_MS so it never blocks the caller for long.
    // Returns true exactly once when a *new* tag UID appears (i.e. one that
    // was not already being tracked). `uidOut` receives the normalized
    // (uppercase hex, no separators) UID in that case.
    bool tick(String &uidOut);

    // True if `uid` is still the tag currently being tracked as present.
    // Used for removal detection during an active/error session.
    bool isPresent(const String &uid);

    // Forget the currently tracked tag so the next detection is treated as new.
    void resetTracking();
}

#endif // AMPULE_RFID_MANAGER_H
