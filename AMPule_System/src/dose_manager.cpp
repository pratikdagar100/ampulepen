#include "dose_manager.h"
#include "medicine_manager.h"

namespace DoseManager {

int getDose(const String &medicineId, WeightCategory weight) {
    MedicineRecord med;
    if (!MedicineManager::getById(medicineId, med)) {
        return -1;
    }
    return doseForWeight(med.doses, weight);
}

} // namespace DoseManager
