#pragma once

#include "SX12xxDriverCommon.h"
#include <targets.h>

class RFAMP_hal
{
public:
    static RFAMP_hal *instance;

    RFAMP_hal();

    void init();
    void ICACHE_RAM_ATTR TXenable(SX12XX_Radio_Number_t radioNumber);
    void ICACHE_RAM_ATTR RXenable();
    void ICACHE_RAM_ATTR TXRXdisable();

private:
#if defined(PLATFORM_ESP32)
    uint64_t txrx_disable_clr_bits;
    uint64_t tx1_enable_set_bits;
    uint64_t tx1_enable_clr_bits;
    uint64_t tx2_enable_set_bits;
    uint64_t tx2_enable_clr_bits;
    uint64_t tx_all_enable_set_bits;
    uint64_t tx_all_enable_clr_bits;
    uint64_t rx_enable_set_bits;
    uint64_t rx_enable_clr_bits;
#else
    bool rx_enabled;
    bool tx1_enabled;
    bool tx2_enabled;
#endif
};