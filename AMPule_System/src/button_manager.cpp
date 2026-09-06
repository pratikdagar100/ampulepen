#include "button_manager.h"
#include "pins.h"
#include "config.h"

namespace ButtonManager {

struct DebouncedButton {
    uint8_t pin;
    bool stableState;      // debounced logical state, true = pressed
    bool rawLastState;
    unsigned long lastChangeMs;
    bool edgeFired;         // set true the loop a press edge is detected
};

static DebouncedButton btnUp    = { PIN_BUTTON_UP,    false, false, 0, false };
static DebouncedButton btnDown  = { PIN_BUTTON_DOWN,  false, false, 0, false };
static DebouncedButton btnEnter = { PIN_BUTTON_ENTER, false, false, 0, false };

static void updateOne(DebouncedButton &b) {
    bool rawPressed = (digitalRead(b.pin) == LOW); // INPUT_PULLUP -> LOW = pressed
    unsigned long now = millis();

    if (rawPressed != b.rawLastState) {
        b.lastChangeMs = now;
        b.rawLastState = rawPressed;
    }

    if ((now - b.lastChangeMs) >= BUTTON_DEBOUNCE_MS) {
        if (rawPressed != b.stableState) {
            b.stableState = rawPressed;
            if (b.stableState) {
                b.edgeFired = true; // rising edge into "pressed"
            }
        }
    }
}

void begin() {
    pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
    pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
    pinMode(PIN_BUTTON_ENTER, INPUT_PULLUP);
}

void update() {
    updateOne(btnUp);
    updateOne(btnDown);
    updateOne(btnEnter);
}

static bool consumeEdge(DebouncedButton &b) {
    if (b.edgeFired) {
        b.edgeFired = false;
        return true;
    }
    return false;
}

bool upPressed()    { return consumeEdge(btnUp); }
bool downPressed()  { return consumeEdge(btnDown); }
bool enterPressed() { return consumeEdge(btnEnter); }

} // namespace ButtonManager
