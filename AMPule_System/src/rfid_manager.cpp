#include "rfid_manager.h"
#include <Wire.h>
#include <Adafruit_PN532.h>
#include "pins.h"
#include "config.h"

namespace RfidManager {

static Adafruit_PN532 nfc(PIN_PN532_IRQ, PIN_PN532_RESET);

static bool hardwareOk = false;

static unsigned long lastPollMs = 0;
static String trackedUid = "";
static int missCount = 0;

static String uidToHexString(uint8_t *uid, uint8_t len) {
    String s;
    s.reserve(len * 2);
    for (uint8_t i = 0; i < len; i++) {
        if (uid[i] < 0x10) s += '0';
        s += String(uid[i], HEX);
    }
    s.toUpperCase();
    return s;
}

bool begin() {
    // PN532 + OLED share the bus; Wire.begin() is called once centrally by
    // the display manager (or the .ino) before this runs.
    nfc.begin();

    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("[RFID] PN532 not found on I2C bus");
        hardwareOk = false;
        return false;
    }

    Serial.printf("[RFID] Found PN5%02X, firmware v%d.%d\n",
                  (versiondata >> 24) & 0xFF,
                  (versiondata >> 16) & 0xFF,
                  (versiondata >> 8) & 0xFF);

    nfc.SAMConfig(); // Configure the Secure Access Module for normal reading
    hardwareOk = true;
    Serial.println("[RFID] PN532 initialized");
    return true;
}

bool isHardwareOk() { return hardwareOk; }

bool tick(String &uidOut) {
    if (!hardwareOk) return false;

    unsigned long now = millis();
    if (now - lastPollMs < RFID_POLL_INTERVAL_MS) {
        return false; // throttle - not time for another hardware read yet
    }
    lastPollMs = now;

    uint8_t uid[7];
    uint8_t uidLength;
    bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, RFID_READ_TIMEOUT_MS);

    if (!found) {
        if (trackedUid.length() > 0) {
            missCount++;
            if (missCount >= RFID_REMOVAL_MISS_THRESHOLD) {
                Serial.printf("[RFID] Tag removed: %s\n", trackedUid.c_str());
                trackedUid = "";
                missCount = 0;
            }
        }
        return false;
    }

    String uidStr = uidToHexString(uid, uidLength);
    missCount = 0;

    if (uidStr == trackedUid) {
        return false; // same tag already being tracked, not a new detection
    }

    trackedUid = uidStr;
    uidOut = uidStr;
    Serial.printf("[RFID] UID: %s\n", uidStr.c_str());
    return true;
}

bool isPresent(const String &uid) {
    return trackedUid.length() > 0 && trackedUid == uid && missCount < RFID_REMOVAL_MISS_THRESHOLD;
}

void resetTracking() {
    trackedUid = "";
    missCount = 0;
}

} // namespace RfidManager
