//
// Authors: 
// * Dominic Clifton (hydra)
//

#pragma once

/**
 * @brief base class for all transponder systems
 */
class TransponderSystem {
public:
    virtual ~TransponderSystem() {};

    /**
     * @brief Initialise the system
     * @return false on error
     */
    virtual bool init() = 0;

    /**
     * @brief De-initialise the system
     */
    virtual void deinit() = 0;

    /**
     * @brief Start transmission of transponder signal
     *
     * To prevent multiple transmitting transponders from always transmitting at the same point in time, which
     * would prevent the receiver from receiving a valid signal due to collisions, jitter needs to be introduced
     * into the signal so that each transmitting transponder doesn't transmit at the same time.
     *
     * @param intervalMs the interval between the start of this transmission and the start of the next, in milliseconds.
     */
    virtual void startTransmission(uint32_t &intervalMs) = 0;

    /**
     * @brief Check to see if the system is initialised
     * @return true if it is.
     */
    virtual bool isInitialised() = 0;
};

