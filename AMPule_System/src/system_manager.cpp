#include "system_manager.h"
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include "pins.h"
#include "config.h"
#include "version.h"
#include "storage_manager.h"
#include "medicine_manager.h"
#include "ampule_manager.h"
#include "dose_manager.h"
#include "rfid_manager.h"
#include "display_manager.h"
#include "button_manager.h"
#include "web_server.h"

namespace SystemManager {

static SystemState state = STATE_BOOT;
static AmpuleSession session;

static bool rfidReady = false;
static bool oledReady = false;
static bool storageReady = false;
static bool timeSourceIsNtp = false;

static String errorTitle;
static String errorMessage;

static String pendingUid;              // UID being verified in STATE_SCANNING
static unsigned long scanUntilMs = 0;  // brief "SCANNING..." dwell time
static const unsigned long SCAN_DISPLAY_MS = 400;

static int weightIndex = 0; // current cursor position during WEIGHT_SELECTION

static bool virtualUp = false, virtualDown = false, virtualEnter = false;

// ---------------------------------------------------------------------------
// Time handling
// ---------------------------------------------------------------------------

// Parses the compiler-supplied __DATE__ ("Mmm dd yyyy") / __TIME__
// ("hh:mm:ss") into epoch seconds. Used as a soft-RTC fallback baseline so
// expiry checking works immediately after flashing even with no network —
// see README "Time & Expiry Limitations" for why this is a fallback, not a
// substitute for NTP or a DS3231 in a real deployment.
static time_t parseBuildTimeEpoch() {
    const char *monthNames = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char monStr[4] = {0};
    int day, year, hour, min, sec;
    sscanf(__DATE__, "%3s %d %d", monStr, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

    int month = 0;
    for (int i = 0; i < 12; i++) {
        if (strncmp(monthNames + i * 3, monStr, 3) == 0) {
            month = i;
            break;
        }
    }

    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    return mktime(&t);
}

static void seedFallbackClock() {
    time_t buildEpoch = parseBuildTimeEpoch();
    struct timeval tv = { buildEpoch, 0 };
    settimeofday(&tv, nullptr);
    timeSourceIsNtp = false;
    Serial.println("[TIME] Fallback clock seeded from firmware build time");
}

static void attemptNtpSync() {
    StaticJsonDocument<256> cfg;
    String staSsid = WIFI_STA_SSID_DEFAULT;
    String staPass = WIFI_STA_PASSWORD_DEFAULT;
    if (StorageManager::readJson(FILE_CONFIG, cfg)) {
        staSsid = cfg["staSsid"] | staSsid;
        staPass = cfg["staPassword"] | staPass;
    }

    if (staSsid.length() == 0) {
        Serial.println("[TIME] No STA Wi-Fi configured, skipping NTP");
        return;
    }

    Serial.printf("[TIME] Attempting STA connect to \"%s\" for NTP...\n", staSsid.c_str());
    WiFi.begin(staSsid.c_str(), staPass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_STA_CONNECT_TIMEOUT_MS) {
        delay(200); // one-time boot-only wait, does not affect runtime loop responsiveness
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[TIME] STA connect failed, using fallback clock");
        return;
    }

    Serial.println("[TIME] STA connected, requesting NTP time...");
    configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

    start = millis();
    while (time(nullptr) < 1700000000 && (millis() - start) < 5000) {
        delay(200);
    }

    if (time(nullptr) >= 1700000000) {
        timeSourceIsNtp = true;
        Serial.println("[TIME] NTP sync successful");
    } else {
        Serial.println("[TIME] NTP sync timed out, using fallback clock");
    }
}

static time_t getCurrentEpoch(bool &available) {
    available = true;
    return time(nullptr);
}

// ---------------------------------------------------------------------------
// Wi-Fi AP + web server
// ---------------------------------------------------------------------------

static void startAccessPoint() {
    StaticJsonDocument<256> cfg;
    String ssid = WIFI_AP_SSID_DEFAULT;
    String pass = WIFI_AP_PASSWORD_DEFAULT;
    if (StorageManager::readJson(FILE_CONFIG, cfg)) {
        ssid = cfg["apSsid"] | ssid;
        pass = cfg["apPassword"] | pass;
    }

    WiFi.mode(WIFI_AP_STA); // AP always on; STA used opportunistically for NTP
    WiFi.softAP(ssid.c_str(), pass.c_str());

    Serial.println("[WIFI] AP started");
    Serial.print("[WIFI] SSID: ");
    Serial.println(ssid);
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.softAPIP());
}

// ---------------------------------------------------------------------------
// History helper
// ---------------------------------------------------------------------------

static String nowTimestamp() {
    time_t t = time(nullptr);
    struct tm *lt = localtime(&t);
    char buf[24];
    strftime(buf, sizeof(buf), "%H:%M:%S", lt);
    return String(buf);
}

static void logHistory(const String &status) {
    AmpuleManager::addHistoryEntry(
        nowTimestamp(),
        session.uid,
        session.medicineName.length() ? session.medicineName : String("-"),
        session.weightSelected ? String(weightCategoryLabel(session.weight)) : String("-"),
        session.doseCalculated ? session.dose : 0,
        status
    );
}

// ---------------------------------------------------------------------------
// State transition helpers
// ---------------------------------------------------------------------------

static void enterReady() {
    state = STATE_READY;
    session.clear();
    errorTitle = "";
    errorMessage = "";
    RfidManager::resetTracking();
    DisplayManager::showReady();
    Serial.println("[STATE] WAITING_FOR_AMPULE");
}

static void enterErrorState(SystemState errState, const String &title, const String &message, const String &historyStatus) {
    state = errState;
    errorTitle = title;
    errorMessage = message;
    logHistory(historyStatus);
    DisplayManager::showError(title, message);
    Serial.printf("[STATE] %s - %s\n", systemStateName(errState), title.c_str());
}

static void beginScan(const String &uid) {
    pendingUid = uid;
    scanUntilMs = millis() + SCAN_DISPLAY_MS;
    state = STATE_SCANNING;
    DisplayManager::showScanning();
    Serial.println("[STATE] RFID_SCANNING");
}

static void resolveScan() {
    AmpuleRecord record;
    session.clear();
    session.uid = pendingUid;

    if (!AmpuleManager::findByUID(pendingUid, record)) {
        enterErrorState(STATE_ERROR_UNREGISTERED, "UNREGISTERED AMPULE",
                         "This ampule is not\nregistered. Remove\nand try another.",
                         "Rejected: unregistered");
        return;
    }

    MedicineRecord med;
    MedicineManager::getById(record.medicineId, med);

    session.medicineId = record.medicineId;
    session.medicineName = med.name.length() ? med.name : record.medicineId;
    session.batch = record.batch;
    session.expiry = record.expiry;
    session.used = record.used;

    Serial.printf("[AMPULE] Medicine: %s\n", session.medicineName.c_str());

    if (record.used) {
        enterErrorState(STATE_ERROR_USED, "AMPULE ALREADY USED",
                         "This ampule was\nalready used. Insert\na different ampule.",
                         "Rejected: already used");
        return;
    }
    Serial.println("[VERIFY] Not used");

    bool timeAvailable;
    time_t nowEpoch = getCurrentEpoch(timeAvailable);
    if (!timeAvailable) {
        enterErrorState(STATE_ERROR_TIME_UNAVAILABLE, "TIME NOT AVAILABLE",
                         "Cannot verify expiry\nwithout a valid clock.",
                         "Rejected: time unavailable");
        return;
    }

    bool expired = AmpuleManager::isExpired(record.expiry, nowEpoch, timeAvailable);
    session.expired = expired;
    if (expired) {
        enterErrorState(STATE_ERROR_EXPIRED, "EXPIRED AMPULE",
                         "This ampule has\nexpired. Remove and\ntry another.",
                         "Rejected: expired");
        return;
    }
    Serial.println("[VERIFY] Not expired");
    Serial.println("[VERIFY] Verified");

    session.verified = true;
    session.active = true;
    weightIndex = 0;
    state = STATE_AMPULE_ACTIVE;
    logHistory("Verified");
    DisplayManager::showVerified(session.medicineName);
    Serial.println("[STATE] AMPULE_ACTIVE");
}

// ---------------------------------------------------------------------------
// Button edge helpers (physical OR virtual dashboard trigger)
// ---------------------------------------------------------------------------

void triggerVirtualButton(const String &which) {
    if (which == "up") virtualUp = true;
    else if (which == "down") virtualDown = true;
    else if (which == "enter") virtualEnter = true;
}

// ---------------------------------------------------------------------------
// Boot sequence
// ---------------------------------------------------------------------------

void begin() {
    Serial.println();
    Serial.println("[BOOT] ESP32 starting...");
    Serial.printf("[BOOT] %s v%s (%s)\n", FIRMWARE_NAME, FIRMWARE_VERSION, FIRMWARE_BUILD);

    oledReady = DisplayManager::begin();
    if (oledReady) {
        DisplayManager::showBootMessage("Initializing...");
    } else {
        Serial.println("[ERROR] OLED initialization failed - continuing without display");
    }

    rfidReady = RfidManager::begin();
    if (!rfidReady) {
        Serial.println("[ERROR] PN532 initialization failed - RFID disabled");
    }

    storageReady = StorageManager::begin();
    if (storageReady) {
        StorageManager::ensureSeedData();
        MedicineManager::begin();
        AmpuleManager::begin();
    } else {
        Serial.println("[ERROR] Storage initialization failed - data will not persist");
    }

    seedFallbackClock();
    startAccessPoint();
    attemptNtpSync(); // safe even without STA configured; AP stays up regardless

    ButtonManager::begin();
    WebServerManager::begin();
    Serial.println("[WEB] Server started on port 80");

    if (!rfidReady) {
        // The core workflow is impossible without the reader. Surface this
        // as a persistent, honest error rather than showing "RFID READY"
        // and silently never detecting any ampule. Recovering requires a
        // physical wiring fix plus a reboot, so this state does not attempt
        // any automatic (tag-removal-based) recovery — see its case in
        // update().
        state = STATE_ERROR_RFID;
        errorTitle = "RFID READ ERROR";
        errorMessage = "PN532 not detected.\nCheck wiring and\nrestart device.";
        DisplayManager::showError(errorTitle, errorMessage);
        Serial.println("[STATE] ERROR_RFID - PN532 not detected at boot");
    } else {
        enterReady();
    }
    Serial.println("[READY] System ready");
}

// ---------------------------------------------------------------------------
// Main update loop
// ---------------------------------------------------------------------------

void update() {
    ButtonManager::update();

    // Drain every button edge exactly once per tick, regardless of state.
    // This prevents a press made in an irrelevant state (e.g. ENTER while
    // still on READY) from staying "queued" and firing unexpectedly once
    // the state machine later reaches a state that reads that button.
    bool upEdge    = ButtonManager::upPressed()    || virtualUp;
    bool downEdge  = ButtonManager::downPressed()  || virtualDown;
    bool enterEdge = ButtonManager::enterPressed() || virtualEnter;
    virtualUp = false;
    virtualDown = false;
    virtualEnter = false;

    String newUid;
    bool newTagDetected = RfidManager::tick(newUid);

    switch (state) {

        case STATE_READY:
            if (newTagDetected) {
                beginScan(newUid);
            }
            break;

        case STATE_SCANNING:
            if (millis() >= scanUntilMs) {
                resolveScan();
            }
            break;

        case STATE_AMPULE_ACTIVE:
            if (!RfidManager::isPresent(session.uid)) {
                enterReady();
                break;
            }
            if (enterEdge) {
                weightIndex = 0;
                session.weight = (WeightCategory)weightIndex;
                state = STATE_WEIGHT_SELECTION;
                DisplayManager::showWeightSelection(session.medicineName, session.weight);
                Serial.println("[STATE] WEIGHT_SELECTION");
            }
            break;

        case STATE_WEIGHT_SELECTION:
            if (!RfidManager::isPresent(session.uid)) {
                enterReady();
                break;
            }
            if (upEdge) {
                weightIndex = (weightIndex - 1 + WEIGHT_COUNT) % WEIGHT_COUNT;
                session.weight = (WeightCategory)weightIndex;
                DisplayManager::showWeightSelection(session.medicineName, session.weight);
            } else if (downEdge) {
                weightIndex = (weightIndex + 1) % WEIGHT_COUNT;
                session.weight = (WeightCategory)weightIndex;
                DisplayManager::showWeightSelection(session.medicineName, session.weight);
            } else if (enterEdge) {
                session.weight = (WeightCategory)weightIndex;
                session.weightSelected = true;
                int dose = DoseManager::getDose(session.medicineId, session.weight);
                session.dose = (dose >= 0) ? dose : 0;
                session.doseCalculated = true;
                state = STATE_DOSE_DISPLAY;
                logHistory("Weight selected: " + String(weightCategoryLabel(session.weight)));
                logHistory("Demo dose displayed: " + String(session.dose) + " mg");
                DisplayManager::showDose(session.medicineName, session.weight, session.dose);
                Serial.println("[STATE] DOSE_DISPLAY");
            }
            break;

        case STATE_DOSE_DISPLAY:
            if (!RfidManager::isPresent(session.uid)) {
                enterReady();
                break;
            }
            if (enterEdge) {
                AmpuleManager::markUsed(session.uid);
                session.used = true;
                state = STATE_COMPLETED;
                logHistory("Ampule marked used");
                DisplayManager::showComplete(session.medicineName, session.dose);
                Serial.println("[STATE] COMPLETED");
            }
            break;

        case STATE_COMPLETED:
        case STATE_ERROR_UNREGISTERED:
        case STATE_ERROR_EXPIRED:
        case STATE_ERROR_USED:
        case STATE_ERROR_TIME_UNAVAILABLE:
            if (!RfidManager::isPresent(session.uid)) {
                enterReady();
            }
            break;

        case STATE_ERROR_RFID:
            // Hardware fault detected at boot (no PN532 found). There is no
            // ampule session to wait on, and the reader can't be probed
            // again without re-initializing it — this state persists until
            // the wiring is fixed and the device is rebooted.
            break;

        case STATE_BOOT:
        default:
            break;
    }

    WebServerManager::handleClient();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

SystemState getState() { return state; }
const AmpuleSession &getSession() { return session; }

bool isRfidReady() { return rfidReady; }
bool isOledReady() { return oledReady; }
bool isStorageReady() { return storageReady; }

String getTimeSource() { return timeSourceIsNtp ? "NTP" : "BUILD_FALLBACK"; }

String getApIp() { return WiFi.softAPIP().toString(); }

String getApSsid() {
    StaticJsonDocument<256> cfg;
    if (StorageManager::readJson(FILE_CONFIG, cfg)) {
        return String((const char *)(cfg["apSsid"] | WIFI_AP_SSID_DEFAULT));
    }
    return WIFI_AP_SSID_DEFAULT;
}

String getErrorTitle() { return errorTitle; }
String getErrorMessage() { return errorMessage; }

void fillStatusJson(JsonDocument &doc) {
    doc["state"] = systemStateName(state);
    doc["demoMode"] = true;
    doc["medicine"] = session.medicineName;
    doc["ampuleUid"] = session.uid;
    doc["batch"] = session.batch;
    doc["expiry"] = session.expiry;
    doc["ampuleVerified"] = session.verified;
    doc["expired"] = session.expired;
    doc["used"] = session.used;
    doc["weightSelected"] = session.weightSelected;
    doc["weight"] = session.weightSelected ? String(weightCategoryLabel(session.weight)) : String("");
    doc["weightIndex"] = session.weightSelected ? (int)session.weight : -1;
    // Live cursor position (updates as UP/DOWN move it, before ENTER confirms)
    // so the dashboard can mirror the OLED's ">" marker in real time.
    doc["weightCursor"] = (int)session.weight;
    doc["doseCalculated"] = session.doseCalculated;
    doc["dose"] = session.dose;
    doc["errorTitle"] = errorTitle;
    doc["errorMessage"] = errorMessage;
    doc["isError"] = isErrorState(state);
}

void fillSystemJson(JsonDocument &doc) {
    doc["firmwareName"] = FIRMWARE_NAME;
    doc["firmwareVersion"] = FIRMWARE_VERSION;
    doc["firmwareBuild"] = FIRMWARE_BUILD;
    doc["demoMode"] = true;
    doc["espOnline"] = true;
    doc["rfidReady"] = rfidReady;
    doc["oledReady"] = oledReady;
    doc["databaseReady"] = storageReady;
    doc["timeSource"] = getTimeSource();
    doc["apSsid"] = getApSsid();
    doc["apIp"] = getApIp();
    doc["uptimeMs"] = millis();
}

} // namespace SystemManager
