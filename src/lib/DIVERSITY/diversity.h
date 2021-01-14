#include <Arduino.h>
#include "../../src/targets.h"
#include "../../src/LowPassFilter.h"
#include "../../src/utils.h"

LPF antA(1);
LPF antB(1);

LPF antA_dt(2);
LPF antB_dt(2);

#ifdef Regulatory_Domain_ISM_2400
#define RSSIthreshold 85
#else
#define RSSIthreshold 67
#endif
#define LQthreshold 98

class DIVERSITY
{
private:
    bool activeAntenna = false;

    uint8_t linkquality;

    uint32_t antA_skip; // how many times the selected antenna was 'skipped' ie how many times it was not the active antenna for each call.
    uint32_t antB_skip;

    uint8_t antA_rssi; //uses raw RSSI values from radio, reverse scaled, ie higher is better.
    uint8_t antB_rssi;

    int8_t antA_rssidt; //rate of change of RSSI
    int8_t antB_rssidt;

public:
    DIVERSITY(/* args */);
    ~DIVERSITY();

    void ICACHE_RAM_ATTR updateRSSI(uint8_t rssi, uint8_t lq);
    bool ICACHE_RAM_ATTR calcActiveAntenna();
    bool ICACHE_RAM_ATTR getActiveAntenna();
    void ICACHE_RAM_ATTR toggleAntenna();
    int8_t ICACHE_RAM_ATTR RSSI();
    int8_t ICACHE_RAM_ATTR RSSIa();
    int8_t ICACHE_RAM_ATTR RSSIb();
};

DIVERSITY::DIVERSITY(/* args */)
{
}

DIVERSITY::~DIVERSITY()
{
}

void ICACHE_RAM_ATTR DIVERSITY::updateRSSI(uint8_t rssi, uint8_t lq)
{
    linkquality = lq;

    if (activeAntenna == true)
    {
        uint8_t antA_rssi_prev = antA_rssi;
#ifdef Regulatory_Domain_ISM_2400
        antA_rssi = (uint8_t)antA.update(255 - rssi); // sx1280 raw value decreases as signal gets better
#else
        antA_rssi = (uint8_t)antA.update(rssi); // sx1276 raw value increases as signal gets better
#endif
        antA_rssidt = (uint8_t)antA_dt.update((antA_rssi - antA_rssi_prev) / (antA_skip + 1));

        antA_skip = 0;
        antB_skip++; // antenna b was skipped
    }
    else
    {
        uint8_t antB_rssi_prev = antB_rssi;
#ifdef Regulatory_Domain_ISM_2400
        antB_rssi = (uint8_t)antB.update(255 - rssi);
#else
        antB_rssi = (uint8_t)antB.update(rssi);
#endif
        antB_rssidt = (uint8_t)antB_dt.update((antB_rssi - antB_rssi_prev) / (antB_skip + 1));

        antB_skip = 0;
        antA_skip++; // antenna a was skipped
    }
}

bool ICACHE_RAM_ATTR DIVERSITY::calcActiveAntenna()
{
    if (linkquality >= LQthreshold && ((antA_rssi > RSSIthreshold) || (antB_rssi > RSSIthreshold))) // link is ok, no need to switch
    {
        return activeAntenna;
    }

    uint32_t antA_weight = antA_rssi + (antA_rssidt << 3) + (rng8Bit() >> 5) + (antA_skip >> 3);
    uint32_t antB_weight = antB_rssi + (antB_rssidt << 3) + (rng8Bit() >> 5) + (antB_skip >> 3);

    if (antA_weight > antB_weight)
    {
        activeAntenna = true;
    }
    else
    {
        activeAntenna = false;
    }

    return activeAntenna;
}

bool ICACHE_RAM_ATTR DIVERSITY::getActiveAntenna()
{
    return activeAntenna;
}

void ICACHE_RAM_ATTR DIVERSITY::toggleAntenna()
{
    activeAntenna = !activeAntenna;
}

int8_t ICACHE_RAM_ATTR DIVERSITY::RSSI()
{
    uint8_t rssiRaw;
    activeAntenna ? rssiRaw = antA_rssi : rssiRaw = antB_rssi;

#ifdef Regulatory_Domain_ISM_2400
    return -(int8_t)(rssiRaw / 2);
#else
    return (-157 + (int8_t)rssiRaw);
#endif
}

int8_t ICACHE_RAM_ATTR DIVERSITY::RSSIa()
{
#ifdef Regulatory_Domain_ISM_2400
    return -(int8_t)(antA_rssi / 2);
#else
    return (-157 + (int8_t)antA_rssi);
#endif
}

int8_t ICACHE_RAM_ATTR DIVERSITY::RSSIb()
{
#ifdef Regulatory_Domain_ISM_2400
    return -(int8_t)(antB_rssi / 2);
#else
    return (-157 + (int8_t)antB_rssi);
#endif
}