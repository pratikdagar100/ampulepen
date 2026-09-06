#ifndef AMPULE_DOSE_MANAGER_H
#define AMPULE_DOSE_MANAGER_H

#include <Arduino.h>
#include "models.h"

// Thin lookup layer on top of MedicineManager. Kept as its own module so the
// dosing rule (currently a straight table lookup) can evolve independently
// of how medicines are stored.
namespace DoseManager {

    // Returns the DEMO dose (mg) for a medicine id + weight category.
    // Returns -1 if the medicine id is unknown.
    int getDose(const String &medicineId, WeightCategory weight);
}

#endif // AMPULE_DOSE_MANAGER_H
