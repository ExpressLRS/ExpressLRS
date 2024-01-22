#include "targets.h"
#include "common.h"
#include "devMSPVTX.h"
#include "devVTXSPI.h"
#include "freqTable.h"
#include "CRSF.h"
#include "hwTimer.h"

/**
 * Created by phobos-
 * Based on OpenVTX implementation - https://github.com/OpenVTx/OpenVTx/blob/master/src/src/mspVtx.c
 * Original author: Jye Smith.
**/

#ifdef HAS_MSP_VTX

#define FC_QUERY_PERIOD_MS      200 // poll every 200ms
#define MSP_VTX_FUNCTION_OFFSET 7
#define MSP_VTX_PAYLOAD_OFFSET  11
#define MSP_VTX_TIMEOUT_NO_CONNECTION 5000

typedef enum
{
  GET_VTX_TABLE_SIZE = 0,
  CHECK_POWER_LEVELS,
  CHECK_BANDS,
  SET_RCE_PIT_MODE,
  SEND_EEPROM_WRITE,
  MONITORING,
  STOP_MSPVTX,
  MSP_STATE_MAX
} mspVtxState_e;

void SendMSPFrameToFC(uint8_t *mspData);

static bool eepromWriteRequired = false;
static bool setRcePitMode = false;
static uint8_t checkingIndex = 0;
static uint8_t pitMode = 0;
static uint8_t power = 0;
static uint8_t channel = 0;
static uint8_t mspState = STOP_MSPVTX;

static void sendCrsfMspToFC(uint8_t *mspFrame, uint8_t mspFrameSize)
{
    CRSF::SetExtendedHeaderAndCrc(mspFrame, CRSF_FRAMETYPE_MSP_REQ, mspFrameSize, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_FLIGHT_CONTROLLER);
    SendMSPFrameToFC(mspFrame);
}

static void sendVtxConfigCommand(void)
{
    uint8_t vtxConfig[MSP_REQUEST_LENGTH(0)];
    CRSF::SetMspV2Request(vtxConfig, MSP_VTX_CONFIG, nullptr, 0);
    sendCrsfMspToFC(vtxConfig, MSP_REQUEST_FRAME_SIZE(0));
}

static void sendEepromWriteCommand(void)
{
    if (!eepromWriteRequired)
    {
        return;
    }

    uint8_t eepromWrite[MSP_REQUEST_LENGTH(0)];
    CRSF::SetMspV2Request(eepromWrite, MSP_EEPROM_WRITE, nullptr, 0);
    sendCrsfMspToFC(eepromWrite, MSP_REQUEST_FRAME_SIZE(0));

    eepromWriteRequired = false;
}

static void clearVtxTable(void)
{
    uint8_t payload[MSP_SET_VTX_CONFIG_PAYLOAD_LENGTH] = {
        0, // idx LSB
        0, // idx MSB
        3, // 25mW Power idx
        0, // pitmode
        0, // lowPowerDisarm
        0, // pitModeFreq LSB
        0, // pitModeFreq MSB
        4, // newBand - Band Fatshark
        4, // newChannel - Channel 4
        0, // newFreq  LSB
        0, // newFreq  MSB
        6, // newBandCount
        8, // newChannelCount
        5, // newPowerCount
        1  // vtxtable should be cleared
    };

    uint8_t request[MSP_REQUEST_LENGTH(MSP_SET_VTX_CONFIG_PAYLOAD_LENGTH)];
    CRSF::SetMspV2Request(request, MSP_SET_VTX_CONFIG, payload, MSP_SET_VTX_CONFIG_PAYLOAD_LENGTH);
    sendCrsfMspToFC(request, MSP_REQUEST_FRAME_SIZE(MSP_SET_VTX_CONFIG_PAYLOAD_LENGTH));

    eepromWriteRequired = true;
}

static void sendRcePitModeCommand(void)
{
    uint8_t payload[4] = {
        channel, // idx LSB
        0, // idx MSB
        RACE_MODE, // 25mW Power idx
        1 // pitmode
    };

    uint8_t request[MSP_REQUEST_LENGTH(4)];
    CRSF::SetMspV2Request(request, MSP_SET_VTX_CONFIG, payload, 4);
    sendCrsfMspToFC(request, MSP_REQUEST_FRAME_SIZE(4));

    eepromWriteRequired = true;
}

static void setVtxTableBand(uint8_t band)
{
    uint8_t payload[MSP_SET_VTXTABLE_BAND_PAYLOAD_LENGTH];

    payload[0] = band;
    payload[1] = BAND_NAME_LENGTH;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        payload[2 + i] = channelFreqLabelByIdx((band - 1) * CHANNEL_COUNT + i);
    }

    payload[2+CHANNEL_COUNT] = getBandLetterByIdx(band - 1);
    payload[3+CHANNEL_COUNT] = IS_FACTORY_BAND;
    payload[4+CHANNEL_COUNT] = CHANNEL_COUNT;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        payload[(5+CHANNEL_COUNT) + (i * 2)] =  getFreqByIdx(((band-1) * CHANNEL_COUNT) + i) & 0xFF;
        payload[(6+CHANNEL_COUNT) + (i * 2)] = (getFreqByIdx(((band-1) * CHANNEL_COUNT) + i) >> 8) & 0xFF;
    }

    uint8_t request[MSP_REQUEST_LENGTH(MSP_SET_VTXTABLE_BAND_PAYLOAD_LENGTH)];
    CRSF::SetMspV2Request(request, MSP_SET_VTXTABLE_BAND, payload, MSP_SET_VTXTABLE_BAND_PAYLOAD_LENGTH);
    sendCrsfMspToFC(request, MSP_REQUEST_FRAME_SIZE(MSP_SET_VTXTABLE_BAND_PAYLOAD_LENGTH));

    eepromWriteRequired = true;
}

static void getVtxTableBand(uint8_t idx)
{
    uint8_t payloadLength = 1;
    uint8_t payload[1] = {idx};

    uint8_t request[MSP_REQUEST_LENGTH(payloadLength)];
    CRSF::SetMspV2Request(request, MSP_VTXTABLE_BAND, payload, payloadLength);
    sendCrsfMspToFC(request, MSP_REQUEST_FRAME_SIZE(payloadLength));
}

static void setVtxTablePowerLevel(uint8_t idx)
{
    uint8_t payload[MSP_SET_VTXTABLE_POWERLEVEL_PAYLOAD_LENGTH];

    payload[0] = idx;
    payload[1] = powerLevelsLut[idx - 1] & 0xFF;         // powerValue LSB
    payload[2] = (powerLevelsLut[idx - 1] >> 8) & 0xFF; // powerValue MSB
    payload[3] = POWER_LEVEL_LABEL_LENGTH;
    payload[4] = powerLevelsLabel[((idx - 1) * POWER_LEVEL_LABEL_LENGTH) + 0];
    payload[5] = powerLevelsLabel[((idx - 1) * POWER_LEVEL_LABEL_LENGTH) + 1];
    payload[6] = powerLevelsLabel[((idx - 1) * POWER_LEVEL_LABEL_LENGTH) + 2];

    uint8_t request[MSP_REQUEST_LENGTH(MSP_SET_VTXTABLE_POWERLEVEL_PAYLOAD_LENGTH)];
    CRSF::SetMspV2Request(request, MSP_SET_VTXTABLE_POWERLEVEL, payload, MSP_SET_VTXTABLE_POWERLEVEL_PAYLOAD_LENGTH);
    sendCrsfMspToFC(request, MSP_REQUEST_FRAME_SIZE(MSP_SET_VTXTABLE_POWERLEVEL_PAYLOAD_LENGTH));

    eepromWriteRequired = true;
}

static void getVtxTablePowerLvl(uint8_t idx)
{
    uint8_t payloadLength = 1;
    uint8_t payload[1] = {idx};

    uint8_t request[MSP_REQUEST_LENGTH(payloadLength)];
    CRSF::SetMspV2Request(request, MSP_VTXTABLE_POWERLEVEL, payload, payloadLength);
    sendCrsfMspToFC(request, MSP_REQUEST_FRAME_SIZE(payloadLength));
}

/**
 * CRSF frame structure:
 * <sync/address><length><type><destination><origin><status><MSP_v2_frame><crsf_crc>
 * MSP V2 over CRSF frame:
 * <flags><function1><function2><length1><length2><payload1>...<payloadN><msp_crc>
 * The biggest payload we have is 29 (MSP_VTXTABLE_BAND).
 * The maximum packet size is 42 bytes which fits well enough under maximum of 64.
 * Reply always comes with the same function as the request.
**/

void mspVtxProcessPacket(uint8_t *packet)
{
    mspVtxConfigPacket_t *vtxConfigPacket;
    mspVtxPowerLevelPacket_t *powerLevelPacket;
    mspVtxBandPacket_t *bandPacket;

    switch (packet[MSP_VTX_FUNCTION_OFFSET])
    {
    case MSP_VTX_CONFIG:
        vtxConfigPacket = (mspVtxConfigPacket_t *)(packet + MSP_VTX_PAYLOAD_OFFSET);

        switch (mspState)
        {
        case GET_VTX_TABLE_SIZE:

            //  Store initially received values.  If the VTx Table is correct, only then set these values.
            pitMode = vtxConfigPacket->pitmode;
            power = vtxConfigPacket->power;

            if (power == RACE_MODE && pitMode != 1) // If race mode and not already in PIT, force pit mode on boot and set it in BF.
            {
                pitMode = 1;
                setRcePitMode = true;
            }

            if (vtxConfigPacket->lowPowerDisarm) // Force 0mw on boot because BF doesnt send a low power index.
            {
                power = 1;
            }

            if (power >= NUM_POWER_LEVELS)
            {
                power = 3; // 25 mW
            }

            channel = ((vtxConfigPacket->band - 1) * 8) + (vtxConfigPacket->channel - 1);
            if (channel >= FREQ_TABLE_SIZE)
            {
                channel = 27; // F4 5800MHz
            }

            if (vtxConfigPacket->bands == getFreqTableBands() && vtxConfigPacket->channels == getFreqTableChannels() && vtxConfigPacket->powerLevels == NUM_POWER_LEVELS)
            {
               mspState = CHECK_POWER_LEVELS;
               break;
            }
            clearVtxTable();
            break;
        case SET_RCE_PIT_MODE:
            setRcePitMode = false;
            mspState = SEND_EEPROM_WRITE;
        case MONITORING:
            pitMode = vtxConfigPacket->pitmode;

            // Set power before freq changes to prevent PLL settling issues and spamming other frequencies.
            power = vtxConfigPacket->power;
            vtxSPIPitmode = pitMode;
            vtxSPIPowerIdx = power;

            channel = ((vtxConfigPacket->band - 1) * 8) + (vtxConfigPacket->channel - 1);
            if (channel < FREQ_TABLE_SIZE)
            {
                vtxSPIFrequency = getFreqByIdx(channel);
            }
            break;
        }
        break;

    case MSP_VTXTABLE_POWERLEVEL:
        powerLevelPacket = (mspVtxPowerLevelPacket_t *)(packet + MSP_VTX_PAYLOAD_OFFSET);

        switch (mspState)
        {
        case CHECK_POWER_LEVELS:
            if (powerLevelPacket->powerValue ==  powerLevelsLut[checkingIndex] && powerLevelPacket->powerLabelLength == POWER_LEVEL_LABEL_LENGTH) // Check lengths before trying to check content
            {
                if (memcmp(powerLevelPacket->label, powerLevelsLabel + checkingIndex * POWER_LEVEL_LABEL_LENGTH, sizeof(powerLevelPacket->label)) == 0)
                {
                    checkingIndex++;
                    if (checkingIndex > NUM_POWER_LEVELS - 1)
                    {
                        checkingIndex = 0;
                        mspState = CHECK_BANDS;
                    }
                    break;
                }
            }
            setVtxTablePowerLevel(checkingIndex + 1);
            break;
        }
        break;

    case MSP_VTXTABLE_BAND:
        bandPacket = (mspVtxBandPacket_t *)(packet + MSP_VTX_PAYLOAD_OFFSET);

        switch (mspState)
        {
        case CHECK_BANDS:
            if (bandPacket->bandNameLength ==  BAND_NAME_LENGTH && bandPacket->bandLetter == getBandLetterByIdx(checkingIndex) && bandPacket->isFactoryBand == IS_FACTORY_BAND && bandPacket->channels == CHANNEL_COUNT) // Check lengths before trying to check content
            {
                if (memcmp(bandPacket->bandName, channelFreqLabel + checkingIndex * CHANNEL_COUNT, sizeof(bandPacket->bandName)) == 0)
                {
                    if (memcmp(bandPacket->channel, channelFreqTable + checkingIndex * CHANNEL_COUNT, sizeof(bandPacket->channel)) == 0)
                    {
                        checkingIndex++;
                        if (checkingIndex > getFreqTableBands() - 1)
                        {
                            mspState = (setRcePitMode ? SET_RCE_PIT_MODE : (eepromWriteRequired ? SEND_EEPROM_WRITE : MONITORING));
                            vtxSPIPitmode = pitMode;
                            vtxSPIPowerIdx = power;
                            vtxSPIFrequency = getFreqByIdx(channel);
                        }
                        break;
                    }
                }
            }
            setVtxTableBand(checkingIndex + 1);
            break;
        }
        break;

    case MSP_SET_VTX_CONFIG:
    case MSP_SET_VTXTABLE_BAND:
    case MSP_SET_VTXTABLE_POWERLEVEL:
        break;

    case MSP_EEPROM_WRITE:
        mspState = MONITORING;
        break;
    }
}

static void mspVtxStateUpdate(void)
{
    switch (mspState)
    {
        case GET_VTX_TABLE_SIZE:
            sendVtxConfigCommand();
            break;
        case CHECK_POWER_LEVELS:
            getVtxTablePowerLvl(checkingIndex + 1);
            break;
        case CHECK_BANDS:
            getVtxTableBand(checkingIndex + 1);
            break;
        case SET_RCE_PIT_MODE:
            sendRcePitModeCommand();
            break;
        case SEND_EEPROM_WRITE:
            sendEepromWriteCommand();
            break;
        case MONITORING:
            break;
        default:
            break;
    }
}

void disableMspVtx(void)
{
    mspState = STOP_MSPVTX;
}

static void initialize()
{
    if (OPT_HAS_VTX_SPI)
    {
        mspState = GET_VTX_TABLE_SIZE;
    }
}

static int event(void)
{
    if (GPIO_PIN_SPI_VTX_NSS == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
    return DURATION_IMMEDIATELY;
}

static int timeout(void)
{
    if (mspState == STOP_MSPVTX || (mspState != MONITORING && millis() > MSP_VTX_TIMEOUT_NO_CONNECTION))
    {
        return DURATION_NEVER;
    }

    if (hwTimer::running && !hwTimer::isTick)
    {
        // Only run code during rx free time or when disconnected.
        return DURATION_IMMEDIATELY;
    } else {
        mspVtxStateUpdate();
        return FC_QUERY_PERIOD_MS;
    }
}

device_t MSPVTx_device = {
    .initialize = initialize,
    .start = nullptr,
    .event = event,
    .timeout = timeout
};

#endif
