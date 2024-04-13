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
     */
    virtual void startTransmission() = 0;

    /**
     * @brief Check to see if the system is initialised
     * @return true if it is.
     */
    virtual bool isInitialised() = 0;
};

