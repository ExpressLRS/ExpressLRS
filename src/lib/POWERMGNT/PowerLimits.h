#pragma once

#include "PowerLevels.h"

// Selection covers every band the device supports, so changing rates does not
// discard the user's configured maximum. Output is limited to the active bands.
inline PowerLevels_e selectablePowerLimit(PowerLevels_e hardwareMax, PowerLevels_e primaryMax,
                                         PowerLevels_e secondaryMax)
{
    const PowerLevels_e domainMax = primaryMax > secondaryMax ? primaryMax : secondaryMax;
    return hardwareMax < domainMax ? hardwareMax : domainMax;
}

inline PowerLevels_e activePowerLimit(PowerLevels_e primaryMax, PowerLevels_e secondaryMax,
                                     bool usePrimary, bool useDualBand)
{
    if (useDualBand)
    {
        return primaryMax < secondaryMax ? primaryMax : secondaryMax;
    }
    return usePrimary ? primaryMax : secondaryMax;
}

inline PowerLevels_e clampOutputPower(PowerLevels_e requested, PowerLevels_e hardwareMin,
                                     PowerLevels_e hardwareMax, PowerLevels_e domainMax)
{
    const PowerLevels_e maximum = hardwareMax < domainMax ? hardwareMax : domainMax;
    // Preserve the existing CE fallback for hardware that cannot reach the cap.
    // Never index an output calibration table below its hardware minimum.
    if (requested < hardwareMin || maximum < hardwareMin)
    {
        return hardwareMin;
    }
    return requested > maximum ? maximum : requested;
}

class PowerRequest
{
    PowerLevels_e requested = PWR_COUNT;

public:
    bool initialized() const { return requested != PWR_COUNT; }
    void reset() { requested = PWR_COUNT; }
    PowerLevels_e value() const { return requested; }

    void set(PowerLevels_e power, PowerLevels_e hardwareMin, PowerLevels_e selectableMax)
    {
        requested = clampOutputPower(power, hardwareMin, selectableMax, selectableMax);
    }

    PowerLevels_e output(PowerLevels_e hardwareMin, PowerLevels_e hardwareMax,
                         PowerLevels_e domainMax) const
    {
        return clampOutputPower(requested, hardwareMin, hardwareMax, domainMax);
    }
};
