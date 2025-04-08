#pragma once
#include "handset.h"

// NullHandset is a class that represents a null object for the Handset interface, used when the Handset UART is consumed by Airport
class NullHandset final : public Handset
{
public:
    void Begin() override;
    void End() override;
    bool IsArmed() override;
    void handleInput() override;
    void write(uint8_t *data, uint8_t len) override;
    uint8_t readBytes(uint8_t *data, uint8_t len) override;
};