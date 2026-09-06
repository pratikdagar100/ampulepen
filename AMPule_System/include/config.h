#ifndef AMPULE_CONFIG_H
#define AMPULE_CONFIG_H

#include <Arduino.h>

// ============================================================================
// Central, editable configuration. No GPIOs here — see pins.h for those.
// ============================================================================

// ---------------------------------------------------------------------------
// OLED display
// ---------------------------------------------------------------------------
// Change these two if a different sized OLED is used (e.g. 128x32).
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_RESET_PIN -1   // Shares the ESP32 reset line, no dedicated pin

// The two typical SSD1306 I2C addresses. The firmware auto-scans and uses
// whichever one responds, so no manual selection is normally required.
static const uint8_t OLED_I2C_ADDR_CANDIDATES[] = { 0x3C, 0x3D };
#define OLED_I2C_ADDR_CANDIDATE_COUNT 2

// ---------------------------------------------------------------------------
// PN532 / RFID
// ---------------------------------------------------------------------------
// The Adafruit_PN532 driver negotiates the PN532's I2C address internally
// (7-bit 0x24 / 8-bit 0x48, matching the DFR0231-H default) — no address
// constant is needed here.
#define RFID_POLL_INTERVAL_MS      150   // Minimum spacing between PN532 reads
#define RFID_READ_TIMEOUT_MS       80    // Per-attempt timeout passed to the driver
#define RFID_REMOVAL_MISS_THRESHOLD 4    // Consecutive failed reads = tag removed

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------
#define BUTTON_DEBOUNCE_MS 40

// ---------------------------------------------------------------------------
// Wi-Fi Access Point (always on — this is how the dashboard is reached)
// ---------------------------------------------------------------------------
#define WIFI_AP_SSID_DEFAULT     "AMPule-System"
#define WIFI_AP_PASSWORD_DEFAULT "ampule1234"     // WPA2, min 8 chars
#define WIFI_AP_IP               "192.168.4.1"
#define HTTP_PORT                80

// Optional STA (station) mode so the ESP32 can also join an existing Wi-Fi
// network purely to obtain accurate time via NTP. Leave the SSID empty to
// disable — the system works fully standalone without this.
#define WIFI_STA_SSID_DEFAULT     ""
#define WIFI_STA_PASSWORD_DEFAULT ""
#define WIFI_STA_CONNECT_TIMEOUT_MS 8000

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"
#define NTP_GMT_OFFSET_SEC 0
#define NTP_DAYLIGHT_OFFSET_SEC 0

// ---------------------------------------------------------------------------
// Admin
// ---------------------------------------------------------------------------
// Simple local PIN gate for mutating REST endpoints (add/delete ampule,
// edit dose config, reset used-status). Not a substitute for real auth —
// this is a local, offline prototype.
#define ADMIN_PIN_DEFAULT "1234"

// ---------------------------------------------------------------------------
// History log
// ---------------------------------------------------------------------------
#define HISTORY_MAX_ENTRIES 50

// ---------------------------------------------------------------------------
// Ampule database
// ---------------------------------------------------------------------------
#define AMPULE_MAX_RECORDS 32
#define MEDICINE_MAX_RECORDS 8

// ---------------------------------------------------------------------------
// Storage file paths (LittleFS)
// ---------------------------------------------------------------------------
#define FILE_MEDICINES  "/medicines.json"
#define FILE_AMPULES    "/ampules.json"
#define FILE_HISTORY    "/history.json"
#define FILE_CONFIG     "/config.json"

// ---------------------------------------------------------------------------
// Dashboard polling (informational — actual polling interval lives in app.js)
// ---------------------------------------------------------------------------
#define DASHBOARD_POLL_INTERVAL_MS 750

#endif // AMPULE_CONFIG_H
