# AMPule Verification & Dose Selection System — Firmware

A complete, working ESP32 firmware + local web dashboard implementing the
AMPule prototype workflow: RFID ampule identification, verification
(registration / expiry / used-status), weight-based DEMO dose lookup, an
OLED UI, three physical buttons, and a self-hosted dashboard — with no
laptop, Raspberry Pi, or Internet connection required after flashing.

> **This is an engineering prototype / academic demonstration.**
> All dose values are **DEMO VALUES ONLY** and must never be used for real
> medical dosing, treatment, or clinical decisions.

---

## Is this ready to flash onto my ESP32? (read this first)

**Software: yes, fully ready.** Every file in this repo is complete, real
code — no `TODO`s, no stubs, no pseudocode. It compiles as a normal
PlatformIO project and implements the entire workflow end-to-end: RFID
read → verify → weight select → DEMO dose → mark used → dashboard, all
persisted to flash.

**What you still need to do, because no AI can do it for you:**

1. **Wire the actual hardware** to the exact pins in §2/§3 below (I cannot
   verify your soldering/breadboard from here).
2. **Open the project in PlatformIO and build it once** — this downloads
   the four libraries in `platformio.ini` and will surface any
   toolchain-specific compile issue on your machine (different ESP32 core
   versions occasionally shift a library API by a line or two; §17
   Troubleshooting covers the likely ones).
3. **Flash the firmware AND upload the filesystem** — two separate steps
   (§15), both required.
4. **Walk through the testing procedure in §16** on your real hardware.
   I've reasoned through the logic carefully, but it has not run on a
   physical DFR0231-H/OLED/ESP32 in front of me — you are the first real
   hardware test.

Nothing here is placeholder or "left for you to implement" — what's left
is exactly the part that requires a soldering iron and a serial monitor,
which is unavoidable for any embedded project.

---

## Quick start — from a blank ESP32 to a working scan

This is the condensed path; each linked section has full detail.

1. **Wire it up** per the table in §2 (5 wires shared between the OLED and
   PN532 on the I2C bus, 3 buttons to GND with `INPUT_PULLUP`, no external
   resistors needed).
2. **Install VS Code + the PlatformIO extension** (§15 Step 1).
3. **Open the `AMPule_System/` folder** in VS Code — the one containing
   `platformio.ini` (§15 Step 2). PlatformIO will detect it automatically
   and offer to install the ESP32 platform + the 4 libraries the first
   time you build.
4. **Plug in the ESP32 via USB.**
5. **Build → Upload → Upload Filesystem Image**, in that order (§15 Steps
   4–6). These are three separate PlatformIO actions in the sidebar (or
   `pio run`, `pio run --target upload`, `pio run --target uploadfs`).
6. **Open the Serial Monitor at 115200 baud** and reset the board — you
   should see the full boot log ending in `[READY] System ready`.
7. **Connect your phone/laptop to the `AMPule-System` Wi-Fi network**
   (password `ampule1234` unless you changed it) and open
   `http://192.168.4.1`.
8. **Scan one of the 3 seeded demo tags** (or a real tag you register
   from the dashboard's Settings tab) and follow the on-screen/OLED flow.

Full step-by-step verification of every screen and edge case is in §16
"Testing procedure."

---

## 1. Project overview

On power-up the ESP32 boots into `WAITING_FOR_AMPULE`. Scanning a
registered RFID tag identifies the medicine, verifies the ampule
(registered / not expired / not used), pins the medicine name on the OLED,
and lets the operator select a weight range with the UP/DOWN/ENTER
buttons. A DEMO dose is displayed, the operator confirms with ENTER, and
the ampule is marked **used** (persisted, survives reboot). The whole
device also hosts a Wi-Fi access point and a live dashboard at
`http://192.168.4.1` that mirrors the physical device in real time.

The full state machine (`include/models.h`, `src/system_manager.cpp`):

```
SYSTEM_BOOT → WAITING_FOR_AMPULE → RFID_SCANNING → VERIFYING_AMPULE
  → AMPULE_ACTIVE → WEIGHT_SELECTION → DOSE_DISPLAY → COMPLETED
  → (tag removed) → WAITING_FOR_AMPULE

  VERIFYING_AMPULE can instead land on:
  ERROR_UNREGISTERED | ERROR_EXPIRED | ERROR_USED | ERROR_TIME_UNAVAILABLE
  (all wait for the ampule to be removed, then return to WAITING_FOR_AMPULE)
```

The active medicine name is pinned on every OLED screen and in the
dashboard's "Current Ampule" card from the moment of successful
verification until the ampule is physically removed or the session is
reset — there is **no** separate medicine-selection step.

---

## 2. Hardware

| Component | Notes |
|---|---|
| ESP32 DevKitC-32 | Main controller |
| DFRobot **DFR0231-H** (PN532) | RFID/NFC reader, **I2C mode** — do not substitute MFRC522 |
| 0.96"/1.3" SSD1306-style I2C OLED | 128×64 by default (configurable) |
| 3× tactile push buttons | UP / DOWN / ENTER, wired to GND, `INPUT_PULLUP` |
| Breadboard + jumper wires | |
| RFID/NFC tags | ISO14443A (Mifare Classic/Ultralight etc.) |

### Wiring table

| ESP32 pin | Connects to |
|---|---|
| 3.3V | OLED VCC, DFR0231-H VCC |
| GND | OLED GND, DFR0231-H GND, all 3 buttons (other leg) |
| GPIO 21 | OLED SDA, DFR0231-H SDA (shared I2C bus) |
| GPIO 22 | OLED SCL, DFR0231-H SCL (shared I2C bus) |
| GPIO 27 | DFR0231-H IRQ |
| GPIO 25 | UP button |
| GPIO 26 | DOWN button |
| GPIO 33 | ENTER button |

Set the DFR0231-H's interface switch to **I2C** and power-cycle it after
changing the switch.

**Note on PN532 reset:** the DFR0231-H's RSTPDN line is held high on-board
and is not part of this wiring table. The Adafruit_PN532 driver still
requires a reset-pin argument even in I2C mode, so `include/pins.h` assigns
`PIN_PN532_RESET = GPIO 4` as a placeholder that is **not physically
wired**. This is safe — toggling an unconnected GPIO has no effect on the
module. If your specific DFR0231-H unit needs a real hardware reset for
reliable initialization, wire GPIO 4 to its RSTPDN pad.

### I2C addresses (no conflicts)

- DFR0231-H / PN532: negotiated internally by the Adafruit_PN532 driver
  (7-bit `0x24`, i.e. 8-bit `0x48` — matches the DFR0231-H default). No
  address constant is needed in application code.
- OLED: firmware auto-scans `0x3C` then `0x3D` at boot and uses whichever
  responds.

---

## 3. Changing GPIOs

Every GPIO assignment lives in **`include/pins.h`** — nowhere else in the
codebase hardcodes a pin number. Edit that file and re-flash.

---

## 4. Changing the DEMO dose values

Edit **`include/models.h`**'s `DoseProfile` shape if you need new weight
brackets, or simply change the numbers shipped in the seed data at
`src/storage_manager.cpp` (`seedMedicines()`) for the *first-boot* values.
Once the device has booted once, the live values live in LittleFS at
`/medicines.json` and are edited from the dashboard's **Medicines** tab
(admin PIN required) — no re-flash needed. To reset to the shipped demo
numbers, delete `/medicines.json` from the filesystem (see §12) and reboot.

Current DEMO table (mg):

| Medicine | Under 40 kg | 41-60 kg | 61-80 kg | 81 kg+ |
|---|---:|---:|---:|---:|
| Paracetamol (PCM) | 100 | 200 | 300 | 400 |
| Pantoprazole | 50 | 100 | 150 | 200 |
| Rifampicin | 75 | 150 | 225 | 300 |

---

## 5. Demo ampule database

Seeded on first boot into `/ampules.json`:

| RFID UID | Medicine | Batch | Expiry |
|---|---|---|---|
| `A4327B19` | Paracetamol (PCM) | PCM001 | 2027-12-31 |
| `8391427A` | Pantoprazole | PAN001 | 2027-08-20 |
| `29185C11` | Rifampicin | RIF001 | 2027-10-15 |

These UIDs are placeholders. Register real tags from the dashboard's
**Settings → Register New Ampule** form, or see §11.

---

## 6. Project structure

```
AMPule_System/
├── platformio.ini
├── include/
│   ├── config.h        — all editable settings (Wi-Fi, timing, limits, file paths)
│   ├── pins.h           — all GPIO assignments
│   ├── models.h         — shared enums/structs (state machine, dose profile, session)
│   └── version.h
├── src/
│   ├── AMPule_System.ino   — setup()/loop() only
│   ├── system_manager.*    — the state machine; orchestrates every module below
│   ├── rfid_manager.*      — DFR0231-H / PN532 (Adafruit_PN532), edge + removal detection
│   ├── display_manager.*   — SSD1306 OLED screens
│   ├── button_manager.*    — debounced UP/DOWN/ENTER
│   ├── ampule_manager.*    — ampule database + expiry/used logic + history log
│   ├── medicine_manager.*  — medicine identity + DEMO dose profiles
│   ├── dose_manager.*      — dose lookup
│   ├── storage_manager.*   — LittleFS JSON read/write + first-boot seeding
│   └── web_server.*        — REST API + static dashboard hosting
├── data/                — uploaded to LittleFS (dashboard)
│   ├── index.html
│   ├── style.css
│   └── app.js
└── README.md
```

> **PlatformIO note:** PlatformIO's default `src_dir` is `src/`, so
> `AMPule_System.ino` lives inside `src/` alongside the `.cpp` files rather
> than at the project root — this is the standard, well-supported way to
> mix a `.ino` entry point with `.cpp` implementation files under
> PlatformIO. `include/` is auto-added to the compiler's include path.
>
> **Arduino IDE alternative:** Arduino IDE expects one flat sketch folder.
> Create a folder named `AMPule_System`, copy `src/AMPule_System.ino` plus
> every `src/*.cpp`/`src/*.h` and every `include/*.h` directly into it
> (flat, no subfolders), open `AMPule_System.ino`, install the libraries in
> §8, and build/upload as normal — no code changes are required since all
> `#include "x.h"` references resolve by filename.

---

## 7. Uncertainty called out explicitly

Per the request to flag rather than guess at hardware/library details:

1. **PN532 library choice.** The DFR0231-H is DFRobot's breakout for the
   NXP PN532. DFRobot also publishes a `DFRobot_PN532` Arduino library, but
   it is a much thinner wrapper with less mature I2C error handling and
   sparser documentation than **Adafruit's `Adafruit_PN532`**, which is
   the de-facto standard driver for PN532 I2C breakouts (Adafruit's own
   PN532 board, and community reports, confirm it works with DFRobot's
   DFR0231-H since both expose a standard PN532 I2C/HSU/SPI interface).
   This project uses `Adafruit_PN532` for that reason. If you have
   DFRobot's own library installed and prefer it, `src/rfid_manager.cpp`
   is the only file that touches the PN532 API — swap the include and the
   three calls (`begin`, `getFirmwareVersion`/equivalent, `SAMConfig`,
   `readPassiveTargetID`) there.
2. **PN532 reset pin** — see §2 above; it is a required-but-unwired driver
   argument, not a real hardware connection in this wiring table.
3. **Offline expiry checking.** The ESP32 has no hardware RTC and this
   build's Wi-Fi is AP-only by default (no Internet), so genuine NTP time
   sync is not guaranteed. See §9 for exactly how this firmware handles
   that rather than silently pretending ampules are valid.

---

## 8. Required libraries

Installed automatically by PlatformIO via `platformio.ini`:

- `adafruit/Adafruit PN532` (I2C driver for the DFR0231-H)
- `adafruit/Adafruit SSD1306` + `adafruit/Adafruit GFX Library` + `adafruit/Adafruit BusIO` (OLED)
- `bblanchon/ArduinoJson` (v6.x API — pinned, since v7 changes the API)
- ESP32 Arduino core built-ins: `Wire`, `WiFi`, `WebServer`, `LittleFS`

**Arduino IDE:** install the same four library names via
*Sketch → Include Library → Manage Libraries…*, and install the
**esp32 by Espressif Systems** board package via *Boards Manager* if you
haven't already.

---

## 9. Time & expiry — limitations (read this)

Expiry checking needs a real clock. Two sources are supported:

- **NTP**, if you fill in `staSsid`/`staPassword` (Settings tab, or
  `WIFI_STA_SSID_DEFAULT` in `config.h`) for a Wi-Fi network with Internet
  access. The ESP32 runs in `WIFI_AP_STA` mode, so the dashboard AP stays
  up the whole time this is attempted at boot.
- **Build-time fallback**: if no STA network is configured or it fails to
  connect, the firmware seeds its software clock from the firmware's own
  compile timestamp (`__DATE__`/`__TIME__`) so the demo works immediately
  after flashing, completely offline. The dashboard's `/api/system`
  reports which source is active (`"timeSource": "NTP"` or
  `"BUILD_FALLBACK"`) and shows it as a status pill.

**This is a deliberate prototype trade-off**, made so the system is usable
out-of-the-box without Internet, per the "no laptop/Internet required for
normal operation" requirement. It is **not** a reliable clock for a real
deployment — the firmware's clock will drift and resets to the build
timestamp on every power cycle unless NTP is reachable. For a real
offline-reliable deployment, add a **DS3231 RTC** module (I2C, so it can
share GPIO 21/22) and replace `seedFallbackClock()` in
`src/system_manager.cpp` with a read from the RTC. The rest of the expiry
logic (`AmpuleManager::isExpired`) is unaffected by where the epoch comes
from.

If the clock cannot be determined at all (should not happen given the
fallback above, but is handled defensively), the system enters
`ERROR_TIME_UNAVAILABLE` rather than reporting an ampule as valid.

---

## 10. Wi-Fi & the dashboard

- **SSID:** `AMPule-System` (default; change in Settings tab or
  `config.h`)
- **Password:** `ampule1234` (default — **change this**, it's WPA2 and
  world-readable in this repo)
- **IP:** `192.168.4.1`, port `80`

Connect a phone/tablet/laptop to the `AMPule-System` Wi-Fi network, then
open **http://192.168.4.1** in a browser. No Internet connection is used
or required for the dashboard itself — every asset (`index.html`,
`style.css`, `app.js`) is served from the ESP32's own LittleFS, with zero
external CDN dependencies.

The dashboard polls `/api/status` every 750 ms and `/api/system` every
3 s (`fetch`-based, no WebSocket/SSE in this first version — both
`web_server.cpp` and `app.js` are structured so a push-based transport
could be added later without touching the state machine).

**Dashboard data honesty:** the UI uses a dense, dark "telemetry" visual
style (monospace readouts, glowing status dots, a connected-node process
timeline, a terminal-style event log), but every number and label on it
maps to a real field this firmware actually reports — reader type
(PN532/DFR0231-H), display driver (SSD1306), storage (LittleFS/JSON),
clock source (NTP vs. the build-time fallback from §9), uptime, and the
live weight-selection cursor. It does not fabricate sensors, telemetry,
or security properties (e.g. there is no load cell, no SQLite, no TLS) —
if a future revision adds real hardware like a DS3231 RTC or a load cell,
wire its real reading into `/api/system`/`/api/status` rather than
hardcoding a number in `app.js`.

---

## 11. Registering real RFID tags

**From the dashboard (recommended):**
1. Open `http://192.168.4.1` → **Settings** tab.
2. Scan/place the new tag near the DFR0231-H once — its UID will be
   printed to the Serial Monitor (`[RFID] UID: ...`) if you have one
   connected, or you can read the UID off any Mifare tag's printed label.
3. Fill in **Register New Ampule**: RFID UID, medicine, batch, expiry.
4. Enter the admin PIN (default `1234`, see §13) and submit.

**Serial Monitor fallback:** if you don't want to use the dashboard form,
open the Serial Monitor at 115200 baud, scan the new tag, and copy the
`[RFID] UID: XXXXXXXX` value it prints — then use that UID in the
dashboard form (or call `POST /api/ampules` directly, see §14).

---

## 12. Resetting data

All persistent state lives in four small JSON files on LittleFS:
`/medicines.json`, `/ampules.json`, `/history.json`, `/config.json`.

- **Reset everything to demo defaults:** erase the whole filesystem and
  let the firmware reseed it:
  ```
  pio run --target erase        # erases the whole flash, including WiFi/PIN changes
  pio run --target uploadfs     # re-upload data/ (optional, LittleFS auto-formats anyway)
  pio run --target upload
  ```
  Simpler: delete just the four JSON files if you have filesystem access
  via a tool of your choice, or call `pio run --target erase` and reflash.
- **Reset a single ampule's used status:** dashboard → Settings →
  "Reset Used" button on that row (admin PIN required), or
  `POST /api/reset-used {"uid":"...","pin":"..."}`.
- **Un-register an ampule:** dashboard "Delete" button, or
  `DELETE /api/ampules?uid=...` with `{"pin":"..."}` body.

---

## 13. Security notes

This is a **local, offline prototype**, not a production medical device:

- The Wi-Fi AP uses WPA2 (`WIFI_AP_PASSWORD_DEFAULT` in `config.h` —
  change it).
- Mutating REST endpoints (`POST`/`DELETE` on medicines, ampules, settings)
  require a simple 4-digit admin PIN (`ADMIN_PIN_DEFAULT`, default
  `1234`) checked in the request body. This is a basic local safeguard,
  **not** real authentication — do not expose this device to an untrusted
  network.
- `GET` endpoints (status, history, medicine list, ampule list) are
  unauthenticated by design, matching "the dashboard should just work" for
  a local demo; do not reuse this pattern for a real deployment.
- A real medical device would require proper authentication, encrypted
  transport, clinical validation, hardware validation, and regulatory
  compliance — none of which this prototype attempts.

---

## 14. REST API reference

All responses are JSON. Mutating endpoints require `"pin"` in the JSON
body matching the current admin PIN.

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/status` | Full live workflow status (state, medicine, verification, weight, dose, errors) |
| GET | `/api/ampule` | Current ampule session detail only |
| GET | `/api/system` | Subsystem health, firmware version, time source, AP info |
| GET | `/api/medicines` | List of medicines + DEMO dose profiles |
| POST | `/api/medicines` | `{id, under40, kg41to60, kg61to80, over81, pin}` — update a dose profile |
| GET | `/api/history` | Activity log, most recent first |
| GET | `/api/ampules` | All registered ampule records |
| POST | `/api/ampules` | `{uid, medicineId, batch, expiry, pin}` — register a new ampule |
| DELETE | `/api/ampules?uid=...` | `{pin}` — remove an ampule record |
| POST | `/api/reset-used` | `{uid, pin}` — clear an ampule's used flag |
| POST | `/api/control` | `{action:"up"\|"down"\|"enter"}` — dashboard virtual button (mirrors physical buttons through the same state machine, cannot desync) |
| GET | `/api/settings` | Current AP SSID / STA SSID (no passwords returned) |
| POST | `/api/settings` | `{apSsid?, apPassword?, staSsid?, staPassword?, newPin?, pin}` — update config (Wi-Fi changes need a reboot) |

Example `/api/status` response:

```json
{
  "state": "WEIGHT_SELECTION",
  "medicine": "Paracetamol (PCM)",
  "ampuleUid": "A4327B19",
  "batch": "PCM001",
  "expiry": "2027-12-31",
  "ampuleVerified": true,
  "expired": false,
  "used": false,
  "weightSelected": false,
  "weight": "",
  "weightIndex": -1,
  "weightCursor": 0,
  "doseCalculated": false,
  "dose": 0,
  "errorTitle": "",
  "errorMessage": "",
  "isError": false,
  "demoMode": true
}
```

---

## 15. Build & upload (PlatformIO — recommended)

### Step 1 — Install PlatformIO
Install [Visual Studio Code](https://code.visualstudio.com/) and the
**PlatformIO IDE** extension from the VS Code marketplace.

### Step 2 — Open the project
`File → Open Folder…` → select `AMPule_System/` (the folder containing
`platformio.ini`).

### Step 3 — Connect the ESP32
Connect via USB. PlatformIO auto-detects the serial port.

### Step 4 — Build the firmware
PlatformIO sidebar → **esp32dev → General → Build** (or `pio run`).

### Step 5 — Upload the firmware
PlatformIO sidebar → **esp32dev → General → Upload** (or `pio run --target upload`).

### Step 6 — Upload the web dashboard (LittleFS)
PlatformIO sidebar → **esp32dev → Platform → Upload Filesystem Image**
(or `pio run --target uploadfs`). This flashes everything in `data/`
(`index.html`, `style.css`, `app.js`) to the ESP32's LittleFS partition.

> Do both Step 5 *and* Step 6 — the firmware and the web assets are
> separate flash regions and are uploaded independently.

### Step 7 — Reset and connect
Press the ESP32's reset button (or unplug/replug USB). Open the Serial
Monitor at **115200 baud** to watch the boot log, then connect a device
to the `AMPule-System` Wi-Fi network and browse to `http://192.168.4.1`.

---

## 16. Testing procedure — from a blank ESP32 to a working scan

1. Flash firmware (§15 steps 1–5) and upload the filesystem (§15 step 6).
2. Open Serial Monitor at 115200 baud, reset the board. Expect to see, in
   order: `[BOOT]`, OLED found (`[I2C] OLED found at 0x3C`), `[RFID] PN532
   initialized`, `[STORAGE] LittleFS mounted` (+ seed messages on first
   boot), `[TIME] Fallback clock seeded...`, `[WIFI] AP started` /
   `IP: 192.168.4.1`, `[WEB] Server started on port 80`, `[READY] System
   ready`.
3. Confirm the OLED shows:
   ```
   AMPule
   SYSTEM READY
   INSERT AMPULE
   RFID READY
   ```
4. Press UP/DOWN/ENTER individually while watching Serial — no crash, no
   spurious repeated triggers (debouncing works).
5. Connect to Wi-Fi `AMPule-System`, browse to `http://192.168.4.1`.
   Confirm the **Dashboard** tab loads, all four status dots are green
   (ESP32 / RFID / OLED / DATABASE), and the ampule card shows "INSERT
   AMPULE".
6. Scan the demo tag with UID `A4327B19` (or register a real tag with
   this UID via an NFC-writable card / phone app, or substitute one of
   your own tags' UID into `/ampules.json` via the dashboard's Register
   form).
7. Confirm on the OLED: `RFID DETECTED / SCANNING...` then
   `PARACETAMOL (PCM) / VERIFIED / NOT EXPIRED / NOT USED / ENTER →`.
   Confirm the dashboard's ampule card updates within ~1 s to show the
   same medicine, UID, batch, expiry and a green "VERIFY" step.
8. Press ENTER. OLED shows the weight selection screen with `UNDER 40 kg`
   highlighted. Dashboard shows four weight option cards, "WEIGHT" step
   highlighted.
9. Press DOWN twice (cursor moves to `61-80 kg`), then ENTER. OLED shows
   `WEIGHT 61-80 kg / DOSE 300 mg / DEMO VALUE / ENTER →`. Dashboard shows
   the same weight card highlighted and "300 mg — DEMO VALUE" prominently.
10. Press ENTER again. OLED shows `COMPLETE / Dose: 300 mg / AMPULE USED`.
    Dashboard "COMPLETE" step is highlighted and the activity log shows
    the full sequence (verified → weight selected → dose displayed →
    marked used) with timestamps.
11. Remove the tag. Within ~1 s (four missed poll cycles) the OLED and
    dashboard both return to `INSERT AMPULE`.
12. Re-scan the **same** tag. Confirm it is now rejected:
    `AMPULE ALREADY USED` on both OLED and dashboard, and the History tab
    shows the new "Rejected: already used" row.
13. Remove the tag, scan a tag UID that is **not** in `/ampules.json`.
    Confirm `UNREGISTERED AMPULE` is shown and logged.
14. In the dashboard, open **Settings**, use "Reset Used" on the
    `A4327B19` row (admin PIN `1234`), remove/re-scan that tag, and
    confirm it is accepted again from `AMPULE_ACTIVE` onward.
15. Open **Medicines**, change Paracetamol's `61-80 kg` value to e.g.
    `999`, save with the admin PIN, reset the used ampule again, re-run
    steps 6–10, and confirm the dose shown is now `999 mg DEMO VALUE` —
    proving the dose table is live-editable without reflashing.

---

## 17. Troubleshooting

**OLED is blank**
Check 3.3V/GND, SDA→GPIO21, SCL→GPIO22. Confirm Serial shows
`[I2C] OLED found at 0x3C` (or `0x3D`); if it instead logs "OLED not
found", run an I2C scanner sketch to confirm the actual address and add
it to `OLED_I2C_ADDR_CANDIDATES` in `config.h`.

**PN532 not detected (`[RFID] PN532 not found on I2C bus`)**
- Confirm the DFR0231-H's mode switch is set to **I2C**, then power-cycle it.
- Recheck SDA→GPIO21, SCL→GPIO22, IRQ→GPIO27, VCC→3.3V, GND→GND.
- Try wiring GPIO 4 to the module's RSTPDN pad (see §2/§7).

**RFID tag doesn't scan**
- Confirm the tag is ISO14443A-compatible and held within a few cm of the
  antenna.
- Confirm `[RFID] PN532 initialized` appeared at boot.
- Confirm the UID is actually registered — an unrecognized tag correctly
  produces `UNREGISTERED AMPULE`, which is not a fault.

**Buttons don't respond**
Confirm wiring to GPIO 25/26/33 with the other leg to GND, and that
`pinMode(..., INPUT_PULLUP)` is in effect (it is set in
`ButtonManager::begin()` — no external pull-ups needed or wanted).

**Dashboard won't load**
- Confirm you're connected to the `AMPule-System` Wi-Fi network, not your
  home/office network.
- Browse to `http://192.168.4.1` exactly (not `https://`).
- If you flashed firmware but see a blank page or 404s for `style.css`/
  `app.js`, you forgot **Step 6 — Upload Filesystem Image** (`pio run
  --target uploadfs`); the firmware and the web assets are uploaded
  separately.

**Ampule always shows `TIME NOT AVAILABLE`**
This should not normally happen (build-time fallback always seeds a
clock) — if it does, check that `include/version.h`'s `__DATE__`/`__TIME__`
macros are being compiled fresh (a very old compiler toolchain issue) and
consider adding a DS3231 RTC per §9.

**"Invalid admin PIN" on every save**
Default PIN is `1234`. If you changed it via Settings and forgot it,
erase the filesystem (`pio run --target erase` + reflash) to fall back to
`ADMIN_PIN_DEFAULT` in `config.h`.

---

## 18. Consistency checklist (verified in this deliverable)

- [x] GPIOs match across `pins.h`, wiring table, and all managers (21/22
      I2C, 27 IRQ, 25/26/33 buttons).
- [x] I2C addresses don't conflict (OLED auto-scans 0x3C/0x3D; PN532
      negotiated internally by the driver).
- [x] DFR0231-H (PN532, `Adafruit_PN532`) used throughout — no MFRC522.
- [x] Medicine names are exactly "Paracetamol (PCM)", "Pantoprazole",
      "Rifampicin" everywhere (seed data, OLED, dashboard).
- [x] Medicine stays pinned across every OLED/dashboard screen until
      removal or reset — no separate medicine-selection step.
- [x] Weight selection works with UP/DOWN/ENTER (`system_manager.cpp`,
      `STATE_WEIGHT_SELECTION`).
- [x] Dose lookup via `DoseManager::getDose` → `MedicineManager`.
- [x] Used status persisted to `/ampules.json`, survives reboot.
- [x] Expiry checked when a clock is available; `ERROR_TIME_UNAVAILABLE`
      otherwise (see §9 for the fallback-clock trade-off).
- [x] Dashboard reflects OLED/application state via polling.
- [x] ESP32 hosts the dashboard itself (`WebServerManager`, LittleFS).
- [x] Dashboard reachable at `http://192.168.4.1`.
- [x] No laptop needed after firmware + filesystem are uploaded.
- [x] No external CDN/JS framework — `data/*.css`/`*.js` are fully local.
- [x] Demo dose values labeled `DEMO VALUE`/`DEMO CONFIGURATION`
      everywhere they're shown.
- [x] "Ampule" terminology used throughout; no "M-Fuel"/"MFuel".

---

## 19. Future upgrades (structured for, not implemented)

- **DS3231 RTC** for reliable offline time (§9) — I2C, shares GPIO 21/22.
- **STA + cloud backend** for remote monitoring (STA mode is already
  wired up for NTP and can be extended).
- **Admin authentication** beyond the local PIN.
- **WebSocket/SSE** push updates instead of polling (`web_server.cpp` and
  `app.js` are modular enough to add this without touching the state
  machine).
- Barcode/QR identification, load-cell presence detection, buzzer/LED
  status indicators.
