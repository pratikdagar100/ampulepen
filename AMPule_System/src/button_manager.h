#ifndef AMPULE_BUTTON_MANAGER_H
#define AMPULE_BUTTON_MANAGER_H

#include <Arduino.h>

// Debounced, edge-triggered reads for the three physical buttons
// (INPUT_PULLUP, active LOW). Call update() once per loop() iteration,
// then check the *Pressed() edge functions.
namespace ButtonManager {

    void begin();
    void update();

    bool upPressed();     // true exactly once per physical press
    bool downPressed();
    bool enterPressed();
}

#endif // AMPULE_BUTTON_MANAGER_H
