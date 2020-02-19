#pragma once

#include "common.h"

extern SX127xDriver Radio;

// FSM state engine written to handle both RFrate and TLMrate changes. Should ensure that master and slave stay in sync can switch states whilist handling packet loss
// Uses the 3way handshake method

typedef enum
{
    RATES_UPD_NONE = 0,
    RATES_UPD_SYN = 1,
    RATES_UPD_SYNACK = 2,
    RATES_UPD_ACK = 3,
} rates_updater_fsm_;

uint8_t RatesConfig = 0;
uint8_t RatesConfigPrev = 0;
rates_updater_fsm_ rates_updater_fsm_state = RATES_UPD_NONE;
rates_updater_fsm_ rates_updater_fsm_state_prev = RATES_UPD_NONE;
bool rates_updater_fsm_busy = false;
//rates_updater_fsm_ rates_updater_fsm_nextState = RT_UPD_NOREQ;

extern expresslrs_mod_settings_s ExpressLRS_currAirRate;

extern uint32_t SyncPacketSendIntervalRXconn;

uint8_t updateNonce;
uint8_t updateNonceCounter;

uint32_t startTime;

//void FSMratesNewEvent(rates_updater_fsm_ newEvent, uint8_t newRate)
bool FSMratesNewEventTX(rates_updater_fsm_ newEvent)
{

    //bool FSMupdate = false;

    switch (rates_updater_fsm_state)
    {
    case RATES_UPD_NONE:
        if (newEvent == RATES_UPD_SYN && rates_updater_fsm_busy == false)
        {
            rates_updater_fsm_busy = true;
            rates_updater_fsm_state = RATES_UPD_SYN;
            SyncPacketSendIntervalRXconn = 250;
            startTime = millis();
        }
        break;
    case RATES_UPD_SYN:
        if (newEvent == RATES_UPD_SYNACK)
        {
            rates_updater_fsm_state = RATES_UPD_SYNACK;
        }
        if (newEvent < rates_updater_fsm_state)
        {
            rates_updater_fsm_state = RATES_UPD_SYN;
        }
        break;
    case RATES_UPD_SYNACK:
        if (newEvent == RATES_UPD_ACK)
        {
            rates_updater_fsm_state = RATES_UPD_ACK;
            ExpressLRS_RateUpdateTime = true;
        }
        if (newEvent < rates_updater_fsm_state)
        {
            rates_updater_fsm_state = RATES_UPD_SYN;
        }
        break;
    case RATES_UPD_ACK:
        Serial.println("got to ack stage!");
        Serial.println(millis() - startTime);
        rates_updater_fsm_state = RATES_UPD_NONE;
        rates_updater_fsm_busy = false;
        SyncPacketSendIntervalRXconn = 1500;

        break;
    default:
        break;
    }
    Serial.println((uint8_t)rates_updater_fsm_state);

    if (rates_updater_fsm_state_prev != rates_updater_fsm_state)
    {
        return true;
    }
    else
    {
        return false;
    }

    //rates_updater_fsm_state_prev = rates_updater_fsm_state;
}

bool FSMratesNewEventRX(rates_updater_fsm_ newEvent)
{
    switch (rates_updater_fsm_state)
    {
    case RATES_UPD_NONE:
        if (newEvent == RATES_UPD_SYN && rates_updater_fsm_busy == false)
        {
            rates_updater_fsm_busy = true;
            rates_updater_fsm_state = RATES_UPD_SYN;
        }
        break;
    case RATES_UPD_SYN:
        if (newEvent == rates_updater_fsm_state)
        {
            rates_updater_fsm_state = RATES_UPD_SYNACK;
            
        }
        break;
    case RATES_UPD_SYNACK:
        if (newEvent == rates_updater_fsm_state)
        {
            rates_updater_fsm_state = RATES_UPD_ACK;
            ExpressLRS_RateUpdateTime = true;
        }
        break;
    case RATES_UPD_ACK:
        Serial.println("got to ack stage!");
        rates_updater_fsm_state = RATES_UPD_NONE;
        rates_updater_fsm_busy = false;
        break;
    default:
        break;
    }

    Serial.println((uint8_t)rates_updater_fsm_state);
}

rates_updater_fsm_ FSMratesGetState()
{
    return rates_updater_fsm_state;
}

void FSMUpdateState(rates_updater_fsm_ newEvent)
{
    rates_updater_fsm_state = newEvent;
}