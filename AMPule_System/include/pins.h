#ifndef AMPULE_PINS_H
#define AMPULE_PINS_H

// ============================================================================
// Central GPIO assignment. Change wiring ONLY here — application code never
// hardcodes a GPIO number outside this file.
// ============================================================================

// ---- Shared I2C bus (OLED + DFR0231-H PN532 both sit on this bus) --------
#define PIN_I2C_SDA        21
#define PIN_I2C_SCL        22

// ---- DFRobot DFR0231-H (PN532, I2C mode) ---------------------------------
#define PIN_PN532_IRQ      27
// NOTE: The DFR0231-H's RSTPDN line is pulled high on-board and is not
// brought out in this project's wiring table (only VCC/GND/SDA/SCL/IRQ are
// connected). The Adafruit_PN532 driver API requires a reset GPIO argument
// even in I2C mode, so we give it an otherwise-unused pin that is NOT wired
// to the module. Toggling an unconnected pin is harmless. If your specific
// DFR0231-H unit needs a hard reset for reliable init, wire this GPIO to the
// module's RSTPDN pad.
#define PIN_PN532_RESET    4

// ---- Buttons (all use internal INPUT_PULLUP, other leg to GND) ----------
#define PIN_BUTTON_UP      25
#define PIN_BUTTON_DOWN    26
#define PIN_BUTTON_ENTER   33

#endif // AMPULE_PINS_H
