//
// Authors: 
// * Dominic Clifton (hydra)
//

#pragma once

class TransponderSystem {
public:
    virtual ~TransponderSystem() {};
    virtual bool init() = 0;
    virtual void deinit() = 0;
    virtual void startTransmission() = 0;
    virtual bool isInitialised() = 0;
};

