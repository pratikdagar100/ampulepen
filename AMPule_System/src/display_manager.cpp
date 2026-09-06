#include "display_manager.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "config.h"

namespace DisplayManager {

static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
static bool hardwareOk = false;

static uint8_t scanForAddress() {
    for (uint8_t i = 0; i < OLED_I2C_ADDR_CANDIDATE_COUNT; i++) {
        uint8_t addr = OLED_I2C_ADDR_CANDIDATES[i];
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            return addr;
        }
    }
    return 0;
}

bool begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    uint8_t addr = scanForAddress();
    if (addr == 0) {
        Serial.println("[I2C] OLED not found at 0x3C or 0x3D");
        hardwareOk = false;
        return false;
    }
    Serial.printf("[I2C] OLED found at 0x%02X\n", addr);

    if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) {
        Serial.println("[I2C] OLED init FAILED");
        hardwareOk = false;
        return false;
    }

    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    display.display();
    hardwareOk = true;
    return true;
}

bool isHardwareOk() { return hardwareOk; }

static void drawHeader(const String &text) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(text);
    display.drawLine(0, 10, OLED_WIDTH - 1, 10, SSD1306_WHITE);
}

void showBootMessage(const String &line) {
    if (!hardwareOk) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Ampule SYSTEM");
    display.setCursor(0, 20);
    display.println(line);
    display.display();
}

void showReady() {
    if (!hardwareOk) return;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Ampule");
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("SYSTEM READY");
    display.setCursor(0, 40);
    display.println("INSERT AMPULE");
    display.setCursor(0, 52);
    display.println("RFID READY");
    display.display();
}

void showScanning() {
    if (!hardwareOk) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("RFID DETECTED");
    display.setCursor(0, 24);
    display.setTextSize(2);
    display.println("SCANNING");
    display.setTextSize(1);
    display.setCursor(0, 44);
    display.println("...");
    display.display();
}

void showVerified(const String &medicineName) {
    if (!hardwareOk) return;
    display.clearDisplay();
    drawHeader(medicineName);
    display.setCursor(0, 16);
    display.println("VERIFIED");
    display.setCursor(0, 28);
    display.println("NOT EXPIRED");
    display.setCursor(0, 40);
    display.println("NOT USED");
    display.setCursor(0, 54);
    display.println("ENTER ->");
    display.display();
}

void showWeightSelection(const String &medicineName, WeightCategory selected) {
    if (!hardwareOk) return;
    display.clearDisplay();
    drawHeader(medicineName);
    display.setCursor(0, 14);
    display.println("SELECT WEIGHT");

    const char *labels[WEIGHT_COUNT] = {
        weightCategoryLabel(WEIGHT_UNDER_40),
        weightCategoryLabel(WEIGHT_41_60),
        weightCategoryLabel(WEIGHT_61_80),
        weightCategoryLabel(WEIGHT_81_PLUS)
    };

    int y = 26;
    for (int i = 0; i < WEIGHT_COUNT; i++) {
        display.setCursor(0, y);
        if (i == (int)selected) {
            display.print("> ");
        } else {
            display.print("  ");
        }
        display.println(labels[i]);
        y += 10;
    }
    display.display();
}

void showDose(const String &medicineName, WeightCategory weight, int doseMg) {
    if (!hardwareOk) return;
    display.clearDisplay();
    drawHeader(medicineName);
    display.setCursor(0, 14);
    display.print("WEIGHT ");
    display.println(weightCategoryLabel(weight));

    display.setTextSize(2);
    display.setCursor(0, 28);
    display.print("DOSE ");
    display.print(doseMg);
    display.println("mg");

    display.setTextSize(1);
    display.setCursor(0, 48);
    display.println("DEMO VALUE");
    display.setCursor(0, 56);
    display.println("ENTER ->");
    display.display();
}

void showComplete(const String &medicineName, int doseMg) {
    if (!hardwareOk) return;
    display.clearDisplay();
    drawHeader(medicineName);
    display.setCursor(0, 16);
    display.println("COMPLETE");
    display.setCursor(0, 30);
    display.print("Dose: ");
    display.print(doseMg);
    display.println(" mg");
    display.setCursor(0, 44);
    display.println("AMPULE USED");
    display.setCursor(0, 56);
    display.println("Remove ampule");
    display.display();
}

void showError(const String &title, const String &message) {
    if (!hardwareOk) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(title);
    display.drawLine(0, 10, OLED_WIDTH - 1, 10, SSD1306_WHITE);
    display.setCursor(0, 18);
    display.println(message);
    display.setCursor(0, 54);
    display.println("Remove ampule");
    display.display();
}

void showTimeUnavailable() {
    showError("TIME NOT AVAILABLE", "Cannot verify expiry.\nCheck system clock.");
}

} // namespace DisplayManager
