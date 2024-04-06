//
// Authors: 
// * Dominic Clifton (hydra)
//

#pragma once

class TransponderSystem {
public:
    virtual ~TransponderSystem() {};
    virtual void init() = 0;
    virtual void deinit() = 0;
    virtual void startTransmission() = 0;
};

