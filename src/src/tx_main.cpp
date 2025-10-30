#include "rxtx_common.h"

#include "CRSFHandset.h"
#include "CRSFParameters.h"
#include "dynpower.h"
#include "msp.h"
#include "msptypes.h"
#include "stubborn_receiver.h"
#include "stubborn_sender.h"

#include "devHandset.h"
#include "devADC.h"
#include "devLED.h"
#include "devTXLUA.h"
#include "devWIFI.h"
#include "devButton.h"
#include "devVTX.h"
#if defined(PLATFORM_ESP32)
#include "devScreen.h"
#include "devBLE.h"
#include "devGsensor.h"
#include "devThermal.h"
#include "devPDET.h"
#include "devBackpack.h"
#else
// Fake functions for 8285
void checkBackpackUpdate() {}
void sendCRSFTelemetryToBackpack(uint8_t *) {}
void sendMAVLinkTelemetryToBackpack(uint8_t *) {}
#endif

#include "CRSFRouter.h"
#include "TXModuleEndpoint.h"
#include "TXOTAConnector.h"
#include "MAVLink.h"

#if defined(PLATFORM_ESP32_S3) || defined(PLATFORM_ESP32_C3)
#include "USB.h"
#define USBSerial Serial
#elif defined(PLATFORM_ESP8266)
#include <user_interface.h>
#endif

#if defined(PLATFORM_ESP8266)
#include <user_interface.h>
#endif

//// CONSTANTS ////
#define MSP_PACKET_SEND_INTERVAL 10LU

/// define some libs to use ///
MSP msp;
ELRS_EEPROM eeprom;
TxConfig config;
Stream *TxUSB;

// Variables / constants for Airport //
FIFO<AP_MAX_BUF_LEN> apInputBuffer;
FIFO<AP_MAX_BUF_LEN> apOutputBuffer;

#define UART_INPUT_BUF_LEN 1024
FIFO<UART_INPUT_BUF_LEN> uartInputBuffer;

uint8_t mavlinkSSBuffer[CRSF_MAX_PACKET_LEN]; // Buffer for current stubbon sender packet (mavlink only)

unsigned long rebootTime = 0;
extern bool webserverPreventAutoStart;
//// MSP Data Handling ///////
bool NextPacketIsMspData = false;  // if true the next packet will contain the msp data
char backpackVersion[32] = "";
uint8_t packageIndexRadio1 = 0xFF;
uint8_t packageIndexRadio2 = 0xFF;
uint8_t tlmSenderDoubleBuffer[20] = {0};

////////////SYNC PACKET/////////
/// sync packet spamming on mode change vars ///
#define syncSpamAmount 3
#define syncSpamAmountAfterRateChange 10
volatile uint8_t syncSpamCounter = 0;
volatile uint8_t syncSpamCounterAfterRateChange = 0;
uint32_t rfModeLastChangedMS = 0;
uint32_t SyncPacketLastSent = 0;
static enum { stbIdle, stbRequested, stbBoosting } syncTelemBoostState = stbIdle;
////////////////////////////////////////////////

static uint32_t LastTLMpacketRecv_Ms = 0;
static uint32_t LinkStatsLastReported_Ms = 0;
static bool commitInProgress = false;

LQCALC<100> LqTQly;

volatile bool busyTransmitting;
static volatile bool ModelUpdatePending;

uint8_t MSPDataPackage[5];
#define BindingSpamAmount 25
static uint8_t BindingSendCount;
bool RxWiFiReadyToSend = false;

static TxTlmRcvPhase_e TelemetryRcvPhase = ttrpTransmitting;
StubbornReceiver TelemetryReceiver;
StubbornSender MspSender;
uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN+1];

CRSFRouter crsfRouter;
TXModuleEndpoint crsfTransmitter;
TXOTAConnector otaConnector;

device_affinity_t ui_devices[] = {
  {&Handset_device, 1},
  {&LED_device, 0},
  {&RGB_device, 0},
  {&TXLUA_device, 1},
  {&ADC_device, 1},
  {&WIFI_device, 0},
  {&Button_device, 0},
#if defined(PLATFORM_ESP32)
  {&Backpack_device, 0},
  {&BLE_device, 0},
#if !defined(PLATFORM_ESP32_C3)
  {&Screen_device, 0},
  {&Gsensor_device, 0},
  {&Thermal_device, 0},
  {&PDET_device, 0},
#endif
#endif
  {&VTX_device, 0}
};

static bool diversityAntennaState = LOW;

static bool inGeminiMode()
{
    return isDualRadio() && config.GetAntennaMode() == TX_RADIO_MODE_GEMINI;
}

void switchDiversityAntennas()
{
  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    diversityAntennaState = !diversityAntennaState;
    digitalWrite(GPIO_PIN_ANT_CTRL, diversityAntennaState);
  }
  if (GPIO_PIN_ANT_CTRL_COMPL != UNDEF_PIN)
  {
    digitalWrite(GPIO_PIN_ANT_CTRL_COMPL, !diversityAntennaState);
  }
}

void ICACHE_RAM_ATTR LinkStatsFromOta(OTA_LinkStats_s * const ls)
{
  int8_t snrScaled = ls->SNR;
  DynamicPower_TelemetryUpdate(snrScaled);

  // Antenna is the high bit in the RSSI_1 value
  // RSSI received is signed, inverted polarity (positive value = -dBm)
  // OpenTX's value is signed and will display +dBm and -dBm properly
  linkStats.uplink_RSSI_1 = -(ls->uplink_RSSI_1);
  linkStats.uplink_RSSI_2 = -(ls->uplink_RSSI_2);
  linkStats.uplink_Link_quality = ls->lq;
#if defined(DEBUG_FREQ_CORRECTION)
  // Don't descale the FreqCorrection value being send in SNR
  linkStats.uplink_SNR = snrScaled;
#else
  linkStats.uplink_SNR = SNR_DESCALE(snrScaled);
#endif
  linkStats.active_antenna = ls->antenna;
  connectionHasModelMatch = ls->modelMatch;
  // -- downlink_SNR / downlink_RSSI is updated for any packet received, not just Linkstats
  // -- uplink_TX_Power is updated when sending to the handset, so it updates when missing telemetry
  // -- rf_mode is updated when we change rates
  // -- downlink_Link_quality is updated before the LQ period is incremented

  // The Rx only has a single radio.  Force the Tx out of Gemini mode.
  if (inGeminiMode() && !ls->trueDiversityAvailable)
  {
      config.SetAntennaMode(TX_RADIO_MODE_SWITCH);
  }
}

bool ICACHE_RAM_ATTR ProcessTLMpacket(SX12xxDriverCommon::rx_status const status)
{
  if (status != SX12xxDriverCommon::SX12XX_RX_OK)
  {
    DBGLN("TLM HW CRC error");
    return false;
  }

  OTA_Packet_s * const otaPktPtr = (OTA_Packet_s * const)Radio.RXdataBuffer;
  OTA_Packet_s * const otaPktPtrSecond = (OTA_Packet_s * const)Radio.RXdataBufferSecond;

  if (!OtaValidatePacketCrc(otaPktPtr))
  {
    DBGLN("TLM crc error");
    return false;
  }

  LastTLMpacketRecv_Ms = millis();
  LqTQly.add();

  Radio.CheckForSecondPacket();
  if (Radio.hasSecondRadioGotData)
  {
    if (!OtaValidatePacketCrc(otaPktPtrSecond))
    {
      Radio.hasSecondRadioGotData = false;
    }
  }

  Radio.GetLastPacketStats();
  linkStats.downlink_SNR = SNR_DESCALE(Radio.LastPacketSNRRaw);
  linkStats.downlink_RSSI_1 = Radio.LastPacketRSSI;
  linkStats.downlink_RSSI_2 = Radio.LastPacketRSSI2;

  // Full res mode
  if (OtaIsFullRes)
  {
    OTA_Packet8_s * const ota8 = (OTA_Packet8_s * const)otaPktPtr;
    OTA_Packet8_s * const ota8Second = (OTA_Packet8_s * const)otaPktPtrSecond;

    switch (otaPktPtr->std.type)
    {
      case PACKET_TYPE_LINKSTATS:
        LinkStatsFromOta(&ota8->data_dl.ul_link_stats.stats);

        if (inGeminiMode())
        {
            if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1)
            {
                packageIndexRadio1 = ota8->data_dl.packageIndex;
                memcpy(tlmSenderDoubleBuffer, ota8->data_dl.ul_link_stats.payload, sizeof(ota8->data_dl.ul_link_stats.payload));
            }
            else
            {
                packageIndexRadio2 = ota8->data_dl.packageIndex;
                memcpy(&tlmSenderDoubleBuffer[sizeof(ota8->data_dl.ul_link_stats.payload)], ota8->data_dl.ul_link_stats.payload, sizeof(ota8->data_dl.ul_link_stats.payload));
            }

            if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1 && Radio.hasSecondRadioGotData)
            {
                packageIndexRadio2 = ota8Second->data_dl.packageIndex;
                memcpy(&tlmSenderDoubleBuffer[sizeof(ota8Second->data_dl.ul_link_stats.payload)], ota8Second->data_dl.ul_link_stats.payload, sizeof(ota8Second->data_dl.ul_link_stats.payload));
            }
            else if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_2 && Radio.hasSecondRadioGotData)
            {
                packageIndexRadio1 = ota8Second->data_dl.packageIndex;
                memcpy(tlmSenderDoubleBuffer, ota8Second->data_dl.ul_link_stats.payload, sizeof(ota8Second->data_dl.ul_link_stats.payload));
            }

            if (packageIndexRadio1 == packageIndexRadio2 && packageIndexRadio1 != 0xFF)
            {
                TelemetryReceiver.ReceiveData(packageIndexRadio1 & ELRS8_TELEMETRY_MAX_PACKAGES,
                    tlmSenderDoubleBuffer, 2 * sizeof(ota8->data_dl.ul_link_stats.payload));
                packageIndexRadio1 = 0xFF;
                packageIndexRadio2 = 0xFF;
            }
        }
        else
        {
            TelemetryReceiver.ReceiveData(ota8->data_dl.packageIndex & ELRS8_TELEMETRY_MAX_PACKAGES,
                ota8->data_dl.ul_link_stats.payload, sizeof(ota8->data_dl.ul_link_stats.payload));
        }
        break;

      case PACKET_TYPE_DATA:
        if (firmwareOptions.is_airport)
        {
          OtaUnpackAirportData(otaPktPtr, &apOutputBuffer);
        }
        else
        {
            if (inGeminiMode())
            {
                if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1)
                {
                    packageIndexRadio1 = ota8->data_dl.packageIndex;
                    memcpy(tlmSenderDoubleBuffer, ota8->data_dl.payload, sizeof(ota8->data_dl.payload));
                }
                else
                {
                    packageIndexRadio2 = ota8->data_dl.packageIndex;
                    memcpy(&tlmSenderDoubleBuffer[sizeof(ota8->data_dl.payload)], ota8->data_dl.payload, sizeof(ota8->data_dl.payload));
                }

                if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1 && Radio.hasSecondRadioGotData)
                {
                    packageIndexRadio2 = ota8Second->data_dl.packageIndex;
                    memcpy(&tlmSenderDoubleBuffer[sizeof(ota8Second->data_dl.payload)], ota8Second->data_dl.payload, sizeof(ota8Second->data_dl.payload));
                }
                else if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_2 && Radio.hasSecondRadioGotData)
                {
                    packageIndexRadio1 = ota8Second->data_dl.packageIndex;
                    memcpy(tlmSenderDoubleBuffer, ota8Second->data_dl.payload, sizeof(ota8Second->data_dl.payload));
                }

                if (packageIndexRadio1 == packageIndexRadio2 && packageIndexRadio1 != 0xFF)
                {
                    MspSender.ConfirmCurrentPayload(ota8->data_dl.tlmConfirm);
                    TelemetryReceiver.ReceiveData(packageIndexRadio1 & ELRS8_TELEMETRY_MAX_PACKAGES,
                        tlmSenderDoubleBuffer, 2 * sizeof(ota8->data_dl.payload));
                    packageIndexRadio1 = 0xFF;
                    packageIndexRadio2 = 0xFF;
                }
            }
            else
            {
                MspSender.ConfirmCurrentPayload(ota8->data_dl.tlmConfirm);
                TelemetryReceiver.ReceiveData(ota8->data_dl.packageIndex & ELRS8_TELEMETRY_MAX_PACKAGES,
                    ota8->data_dl.payload, sizeof(ota8->data_dl.payload));
            }
        }
        break;
    }
  }
  // Std res mode
  else
  {
    switch (otaPktPtr->std.type)
    {
      case PACKET_TYPE_LINKSTATS:
        LinkStatsFromOta(&otaPktPtr->std.data_dl.ul_link_stats.stats);
        break;

      case PACKET_TYPE_DATA:
        if (firmwareOptions.is_airport)
        {
          OtaUnpackAirportData(otaPktPtr, &apOutputBuffer);
        }
        else
        {
            if (inGeminiMode())
            {
                if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1)
                {
                    packageIndexRadio1 = otaPktPtr->std.data_dl.packageIndex;
                    memcpy(tlmSenderDoubleBuffer, otaPktPtr->std.data_dl.payload, sizeof(otaPktPtr->std.data_dl.payload));
                }
                else
                {
                    packageIndexRadio2 = otaPktPtr->std.data_dl.packageIndex;
                    memcpy(&tlmSenderDoubleBuffer[sizeof(otaPktPtr->std.data_dl.payload)], otaPktPtr->std.data_dl.payload, sizeof(otaPktPtr->std.data_dl.payload));
                }

                if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1 && Radio.hasSecondRadioGotData)
                {
                    packageIndexRadio2 = otaPktPtrSecond->std.data_dl.packageIndex;
                    memcpy(&tlmSenderDoubleBuffer[sizeof(otaPktPtrSecond->std.data_dl.payload)], otaPktPtrSecond->std.data_dl.payload, sizeof(otaPktPtrSecond->std.data_dl.payload));
                }
                else if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_2 && Radio.hasSecondRadioGotData)
                {
                    packageIndexRadio1 = otaPktPtrSecond->std.data_dl.packageIndex;
                    memcpy(tlmSenderDoubleBuffer, otaPktPtrSecond->std.data_dl.payload, sizeof(otaPktPtrSecond->std.data_dl.payload));
                }

                if (packageIndexRadio1 == packageIndexRadio2 && packageIndexRadio1 != 0xFF)
                {
                    MspSender.ConfirmCurrentPayload(otaPktPtr->std.data_dl.tlmConfirm);
                    TelemetryReceiver.ReceiveData(packageIndexRadio1 & ELRS4_TELEMETRY_MAX_PACKAGES,
                        tlmSenderDoubleBuffer, 2 * sizeof(otaPktPtr->std.data_dl.payload));
                    packageIndexRadio1 = 0xFF;
                    packageIndexRadio2 = 0xFF;
                }
            }
            else
            {
                MspSender.ConfirmCurrentPayload(otaPktPtr->std.data_dl.tlmConfirm);
                TelemetryReceiver.ReceiveData(otaPktPtr->std.data_dl.packageIndex & ELRS4_TELEMETRY_MAX_PACKAGES,
                    otaPktPtr->std.data_dl.payload,
                    sizeof(otaPktPtr->std.data_dl.payload));
            }
        }
        break;
    }
  }

  return true;
}

expresslrs_tlm_ratio_e ICACHE_RAM_ATTR UpdateTlmRatioEffective()
{
  expresslrs_tlm_ratio_e ratioConfigured = (expresslrs_tlm_ratio_e)config.GetTlm();
  // default is suggested rate for TLM_RATIO_STD/TLM_RATIO_DISARMED
  expresslrs_tlm_ratio_e retVal = ExpressLRS_currAirRate_Modparams->TLMinterval;
  bool updateTelemDenom = true;

  // TLM ratio is boosted until there is one complete sync cycle with no BoostRequest
  if (syncTelemBoostState == stbBoosting)
  {
    syncTelemBoostState = stbIdle;
  }

  if (syncTelemBoostState == stbRequested)
  {
    syncTelemBoostState = stbBoosting;
    retVal = TLM_RATIO_1_2;
  }
  // If Armed, telemetry is disabled, otherwise use STD
  else if (ratioConfigured == TLM_RATIO_DISARMED)
  {
    if (handset->IsArmed())
    {
      retVal = TLM_RATIO_NO_TLM;
      // Avoid updating ExpressLRS_currTlmDenom until connectionState == disconnected
      if (connectionState == connected)
        updateTelemDenom = false;
    }
  }
  else if (ratioConfigured != TLM_RATIO_STD)
  {
    retVal = ratioConfigured;
  }

  if (updateTelemDenom)
  {
    uint8_t newTlmDenom = TLMratioEnumToValue(retVal);
    // Delay going into disconnected state when the TLM ratio increases
    if (connectionState == connected && ExpressLRS_currTlmDenom > newTlmDenom)
      LastTLMpacketRecv_Ms = SyncPacketLastSent;
    ExpressLRS_currTlmDenom = newTlmDenom;
  }

  return retVal;
}

void ICACHE_RAM_ATTR GenerateSyncPacketData(OTA_Sync_s * const syncPtr)
{
  const uint8_t SwitchEncMode = config.GetSwitchMode();
  const uint8_t Index = (syncSpamCounter) ? config.GetRate() : ExpressLRS_currAirRate_Modparams->index;

  if (syncSpamCounter)
    --syncSpamCounter;

  if (syncSpamCounterAfterRateChange && Index == ExpressLRS_currAirRate_Modparams->index)
  {
    --syncSpamCounterAfterRateChange;
    if (connectionState == connected) // We are connected again after a rate change.  No need to keep spaming sync.
      syncSpamCounterAfterRateChange = 0;
  }

  SyncPacketLastSent = millis();

  expresslrs_tlm_ratio_e newTlmRatio = UpdateTlmRatioEffective();

  syncPtr->fhssIndex = FHSSgetCurrIndex();
  syncPtr->nonce = OtaNonce;
  syncPtr->rfRateEnum = get_elrs_airRateConfig(Index)->enum_rate;
  syncPtr->switchEncMode = SwitchEncMode;
  syncPtr->newTlmRatio = newTlmRatio - TLM_RATIO_NO_TLM;
  syncPtr->geminiMode = inGeminiMode();
  syncPtr->otaProtocol = config.GetLinkMode();
  syncPtr->UID4 = UID[4];
  syncPtr->UID5 = UID[5];

  // For model match, the last byte of the binding ID is XORed with the inverse of the modelId
  if (!InBindingMode && config.GetModelMatch())
  {
    syncPtr->UID5 ^= (~crsfTransmitter.modelId) & MODELMATCH_MASK;
  }
}

uint8_t adjustPacketRateForBaud(const uint8_t rateIndex)
{
  return get_elrs_HandsetRate_max(rateIndex, handset->getMinPacketInterval());
}

uint8_t adjustSwitchModeForAirRate(OtaSwitchMode_e eSwitchMode, uint8_t packetSize)
{
    // Only the fullres modes have 3 switch modes, so reset the switch mode if outside the
    // range for 4ch mode
    if (packetSize == OTA4_PACKET_SIZE&& eSwitchMode > smHybridOr16ch)
        return smWideOr8ch;
    return eSwitchMode;
}

void SetRFLinkRate(uint8_t index) // Set speed of RF link
{
  expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
  expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);
  // Binding always uses invertIQ
  bool invertIQ = InBindingMode || (UID[5] & 0x01);
  OtaSwitchMode_e newSwitchMode = (OtaSwitchMode_e)config.GetSwitchMode();

  bool subGHz = FHSSconfig->freq_center < 1000000000;
#if defined(RADIO_LR1121)
  if (FHSSuseDualBand && subGHz)
  {
      subGHz = FHSSconfigDualBand->freq_center < 1000000000;
  }
#endif

  if ((ModParams == ExpressLRS_currAirRate_Modparams)
    && (RFperf == ExpressLRS_currAirRate_RFperfParams)
    && (subGHz || invertIQ == Radio.IQinverted)
    && (OtaSwitchModeCurrent == newSwitchMode)
    && (!InBindingMode))  // binding mode must always execute code below to set frequency
    return;

  DBGLN("set rate %u", index);
  uint32_t interval = ModParams->interval;
#if defined(DEBUG_FREQ_CORRECTION) && defined(RADIO_SX128X)
  interval = interval * 12 / 10; // increase the packet interval by 20% to allow adding packet header
#endif
  hwTimer::updateInterval(interval);

  FHSSusePrimaryFreqBand = !(ModParams->radio_type == RADIO_TYPE_LR1121_LORA_2G4) && !(ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_2G4);
  FHSSuseDualBand = ModParams->radio_type == RADIO_TYPE_LR1121_LORA_DUAL;

  Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, FHSSgetInitialFreq(),
               ModParams->PreambleLen, invertIQ, ModParams->PayloadLength
#if defined(RADIO_SX128X)
               , uidMacSeedGet(), OtaCrcInitializer, (ModParams->radio_type == RADIO_TYPE_SX128x_FLRC)
#endif
#if defined(RADIO_LR1121)
               , (ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_900 || ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_2G4), (uint8_t)UID[5], (uint8_t)UID[4]
#endif
               );

#if defined(RADIO_LR1121)
  if (FHSSuseDualBand)
  {
    Radio.Config(ModParams->bw2, ModParams->sf2, ModParams->cr2, FHSSgetInitialGeminiFreq(),
                ModParams->PreambleLen2, invertIQ, ModParams->PayloadLength,
                (ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_900 || ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_2G4),
                (uint8_t)UID[5], (uint8_t)UID[4], SX12XX_Radio_2);
  }
#endif

  Radio.FuzzySNRThreshold = (RFperf->DynpowerSnrThreshUp == DYNPOWER_SNR_THRESH_NONE) ? 0 : (RFperf->DynpowerSnrThreshUp - RFperf->DynpowerSnrThreshDn);

  if (inGeminiMode() || FHSSuseDualBand)
  {
    Radio.SetFrequencyReg(FHSSgetInitialGeminiFreq(), SX12XX_Radio_2, false);
  }

  // InitialFreq has been set, so lets also reset the FHSS Idx and Nonce.
  FHSSsetCurrIndex(0);
  OtaNonce = 0;

  OtaUpdateSerializers(newSwitchMode, ModParams->PayloadLength);
  MspSender.setMaxPackageIndex(ELRS_MSP_MAX_PACKAGES);
  TelemetryReceiver.setMaxPackageIndex(OtaIsFullRes ? ELRS8_TELEMETRY_MAX_PACKAGES : ELRS4_TELEMETRY_MAX_PACKAGES);

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;
  linkStats.rf_Mode = ModParams->enum_rate;

  handset->setPacketInterval(interval * ExpressLRS_currAirRate_Modparams->numOfSends);
  setConnectionState(disconnected);
  rfModeLastChangedMS = millis();
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{
  // Do not send a stale channels packet to the RX if one has not been received from the handset
  // *Do* send data if a packet has never been received from handset and the timer is running
  // this is the case when bench testing and TXing without a handset
  bool dontSendChannelData = false;
  uint32_t lastRcData = handset->GetRCdataLastRecv();
  if (lastRcData && (micros() - lastRcData > 1000000))
  {
    // The tx is in Mavlink mode and without a valid crsf or RC input.  Do not send stale or fake zero packet RC!
    // Only send sync and MSP packets.
    if (config.GetLinkMode() == TX_MAVLINK_MODE)
    {
      dontSendChannelData = true;
    }
    else
    {
      return;
    }
  }

  busyTransmitting = true;

  uint32_t const now = millis();
  // ESP requires word aligned buffer
  WORD_ALIGNED_ATTR OTA_Packet_s otaPkt = {0};
  static uint8_t syncSlot;

  const bool isTlmDisarmed = config.GetTlm() == TLM_RATIO_DISARMED;
  uint32_t SyncInterval = (connectionState == connected && !isTlmDisarmed) ? ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalConnected : ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalDisconnected;
  bool skipSync = InBindingMode ||
    // TLM_RATIO_DISARMED keeps sending sync packets even when armed until the RX stops sending telemetry and the TLM=Off has taken effect
    (isTlmDisarmed && handset->IsArmed() && (ExpressLRS_currTlmDenom == 1));

  uint8_t NonceFHSSresult = OtaNonce % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

  // Sync spam only happens on slot 1 and 2 and can't be disabled
  if ((syncSpamCounter || (syncSpamCounterAfterRateChange && FHSSonSyncChannel())) && (NonceFHSSresult == 1 || NonceFHSSresult == 2))
  {
    otaPkt.std.type = PACKET_TYPE_SYNC;
    GenerateSyncPacketData(OtaIsFullRes ? &otaPkt.full.sync.sync : &otaPkt.std.sync);
    syncSlot = 0; // reset the sync slot in case the new rate (after the syncspam) has a lower FHSShopInterval
  }
  // Regular sync rotates through 4x slots, twice on each slot, and telemetry pushes it to the next slot up
  // But only on the sync FHSS channel and with a timed delay between them
  else if ((!skipSync) && ((syncSlot / 2) <= NonceFHSSresult) && (now - SyncPacketLastSent > SyncInterval) && FHSSonSyncChannel())
  {
    otaPkt.std.type = PACKET_TYPE_SYNC;
    GenerateSyncPacketData(OtaIsFullRes ? &otaPkt.full.sync.sync : &otaPkt.std.sync);
    syncSlot = (syncSlot + 1) % (ExpressLRS_currAirRate_Modparams->FHSShopInterval * 2);
  }
  else
  {
    if (firmwareOptions.is_airport)
    {
      OtaPackAirportData(&otaPkt, &apInputBuffer);
    }
    else if ((NextPacketIsMspData && MspSender.IsActive()) || dontSendChannelData)
    {
      otaPkt.std.type = PACKET_TYPE_DATA;
      if (OtaIsFullRes)
      {
        otaPkt.full.data_ul.packageIndex = MspSender.GetCurrentPayload(
          otaPkt.full.data_ul.payload,
          sizeof(otaPkt.full.data_ul.payload));
        if (config.GetLinkMode() == TX_MAVLINK_MODE)
          otaPkt.full.data_ul.tlmConfirm = TelemetryReceiver.GetCurrentConfirm();
      }
      else
      {
        otaPkt.std.data_ul.packageIndex = MspSender.GetCurrentPayload(
          otaPkt.std.data_ul.payload,
          sizeof(otaPkt.std.data_ul.payload));
        if (config.GetLinkMode() == TX_MAVLINK_MODE)
          otaPkt.std.data_ul.tlmConfirm = TelemetryReceiver.GetCurrentConfirm();
      }

      // send channel data next so the channel messages also get sent during msp transmissions
      NextPacketIsMspData = false;
      // counter can be increased even for normal msp messages since it's reset if a real bind message should be sent
      BindingSendCount++;
      // If not in TlmBurst, request a sync packet soon to trigger higher download bandwidth for reply
      if (syncTelemBoostState == stbIdle)
        syncSpamCounter = 1;
      syncTelemBoostState = stbRequested;
    }
    else
    {
      // always enable msp after a channel package since the slot is only used if MspSender has data to send
      NextPacketIsMspData = true;

      OtaPackChannelData(&otaPkt, ChannelData, TelemetryReceiver.GetCurrentConfirm());
    }
  }

  ///// Next, Calculate the CRC and put it into the buffer /////
  OtaGeneratePacketCrc(&otaPkt);

  SX12XX_Radio_Number_t transmittingRadio = SX12XX_Radio_All;

  if (isDualRadio())
  {
    // Single antenna modes: tx on one antenna, and true diversity rx for tlm reception.
    switch (config.GetAntennaMode())
    {
    case TX_RADIO_MODE_ANT_1:
      transmittingRadio = SX12XX_Radio_1;
      break;
    case TX_RADIO_MODE_ANT_2:
      transmittingRadio = SX12XX_Radio_2;
      break;
    case TX_RADIO_MODE_SWITCH:
      transmittingRadio = OtaNonce%2 == 0 ? SX12XX_Radio_1 : SX12XX_Radio_2;
      break;
    default:
      break;
    }
  }

#if defined(Regulatory_Domain_EU_CE_2400)
  transmittingRadio = LbtChannelIsClear(transmittingRadio);   // weed out the radio(s) if channel in use

  if (transmittingRadio == SX12XX_Radio_NONE)
  {
    // No packet will be sent due to LBT.
    // Defer TXdoneCallback() to prepare for TLM when the IRQ is normally triggered.
    deferExecutionMicros(ExpressLRS_currAirRate_RFperfParams->TOA, Radio.TXdoneCallback);
  }
  else
#endif
  {
    Radio.TXnb((uint8_t*)&otaPkt, false, (uint8_t*)&otaPkt, transmittingRadio);
  }
}

void ICACHE_RAM_ATTR nonceAdvance()
{
  OtaNonce++;
  if ((OtaNonce + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0)
  {
    ++FHSSptr;
  }
}

/*
 * Called as the TOCK timer ISR when there is a CRSF connection from the handset
 */
void ICACHE_RAM_ATTR timerCallback()
{
  /* If we are busy writing to EEPROM (committing config changes) then we just advance the nonces, i.e. no SPI traffic */
  if (commitInProgress)
  {
    nonceAdvance();
    return;
  }

  // Sync OpenTX to this point
  if (!(OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends))
  {
    handset->JustSentRFpacket();
  }

  // Do not transmit or advance FHSS/Nonce until in disconnected/connected state
  if (connectionState == awaitingModelId)
    return;

  // Tx Antenna Diversity
  if ((OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends == 0 || // Swicth with new packet data
      OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends == ExpressLRS_currAirRate_Modparams->numOfSends / 2) && // Swicth in the middle of DVDA sends
      TelemetryRcvPhase == ttrpTransmitting) // Only switch when transmitting.  A diversity rx will send tlm back on the best antenna.  So don't switch away from it.
  {
    switchDiversityAntennas();
  }

  // Nonce advances on every timer tick
  if (!InBindingMode)
    OtaNonce++;

  // If HandleTLM has started Receive mode, TLM packet reception should begin shortly
  // Skip transmitting on this slot
  if (TelemetryRcvPhase == ttrpPreReceiveGap)
  {
    TelemetryRcvPhase = ttrpExpectingTelem;
#if defined(Regulatory_Domain_EU_CE_2400)
    // Use downlink LQ for LBT success ratio instead for EU/CE reg domain
    linkStats.downlink_Link_quality = LBTSuccessCalc.getLQ();
#else
    linkStats.downlink_Link_quality = LqTQly.getLQ();
#endif
    LqTQly.inc();
    return;
  }
  else if (TelemetryRcvPhase == ttrpExpectingTelem && !LqTQly.currentIsSet())
  {
    // Indicate no telemetry packet received to the DP system
    DynamicPower_TelemetryUpdate(DYNPOWER_UPDATE_MISSED);
  }

  TelemetryRcvPhase = ttrpTransmitting;

  SendRCdataToRF();
}

static void UARTdisconnected()
{
  hwTimer::stop();
  setConnectionState(noCrossfire);
}

static void UARTconnected()
{
  webserverPreventAutoStart = true;
  rfModeLastChangedMS = millis(); // force syncspam on first packets

  auto index = adjustPacketRateForBaud(config.GetRate());
  config.SetRate(index);
  if (connectionState == noCrossfire || connectionState < MODE_STATES)
  {
    // When CRSF first connects, always go into a brief delay before
    // starting to transmit, to make sure a ModelID update isn't coming
    // right behind it
    setConnectionState(awaitingModelId);
  }
  // But start the timer to get OpenTX sync going and a ModelID update sent
  hwTimer::resume();
}

void ResetPower()
{
  // Dynamic Power starts at MinPower unless armed
  // (user may be turning up the power while flying and dropping the power may compromise the link)
  if (config.GetDynamicPower())
  {
    if (!handset->IsArmed())
    {
      // if dynamic power enabled and not armed then set to MinPower
      POWERMGNT::setPower(MinPower);
    }
    else if (POWERMGNT::currPower() < config.GetPower())
    {
      // if the new config is a higher power then set it, otherwise leave it alone
      POWERMGNT::setPower((PowerLevels_e)config.GetPower());
    }
  }
  else
  {
    POWERMGNT::setPower((PowerLevels_e)config.GetPower());
  }
  // TLM interval is set on the next SYNC packet
}

static void ChangeRadioParams()
{
  ModelUpdatePending = false;
  ResetPower(); // Call before SetRFLinkRate(). The LR1121 Radio lib can now set the correct output power in Config().
  SetRFLinkRate(config.GetRate());
  LbtEnableIfRequired();
}

void ModelUpdateReq()
{
  // Force synspam with the current rate parameters in case already have a connection established
  if (config.SetModelId(crsfTransmitter.modelId))
  {
    syncSpamCounter = syncSpamAmount;
    syncSpamCounterAfterRateChange = syncSpamAmountAfterRateChange;
    ModelUpdatePending = true;
  }

  devicesTriggerEvent(EVENT_MODEL_SELECTED);

  // Jump from awaitingModelId to transmitting to break the startup delay now
  // that the ModelID has been confirmed by the handset
  if (connectionState == awaitingModelId)
  {
    setConnectionState(disconnected);
  }
}

static void ConfigChangeCommit()
{
  // Adjust the air rate based on the current baud rate
  auto index = adjustPacketRateForBaud(config.GetRate());
  config.SetRate(index);

  // Write the uncommitted eeprom values (may block for a while)
  uint32_t changes = config.Commit();
  // Change params after the blocking finishes as a rate change will change the radio freq
  ChangeRadioParams();
  // Clear the commitInProgress flag so normal processing resumes
  commitInProgress = false;
  // UpdateFolderNames is expensive so it is called directly instead of in event() which gets called a lot
  crsfTransmitter.updateFolderNames();
  devicesTriggerEvent(changes);
}

static void CheckConfigChangePending()
{
  if (config.IsModified() || ModelUpdatePending)
  {
    // Keep transmitting sync packets until the spam counter runs out
    if (syncSpamCounter > 0)
      return;

    // wait until no longer transmitting
    while (busyTransmitting);
    // Set the commitInProgress flag to prevent any other RF SPI traffic during the commit from RX or scheduled TX
    commitInProgress = true;
    // If telemetry expected in the next interval, the radio was in RX mode
    // and will skip sending the next packet when the timer resumes.
    // Return to normal send mode because if the skipped packet happened
    // to be on the last slot of the FHSS the skip will prevent FHSS
    if (TelemetryRcvPhase != ttrpTransmitting)
    {
      Radio.SetTxIdleMode();
      TelemetryRcvPhase = ttrpTransmitting;
    }
    ConfigChangeCommit();
  }
}

bool ICACHE_RAM_ATTR RXdoneISR(SX12xxDriverCommon::rx_status const status)
{
  // busyTransmitting is required here to prevent accidental rxdone IRQs due to interference triggering RXdoneISR.
  if (LqTQly.currentIsSet() || busyTransmitting)
  {
    return false; // Already received tlm, do not run ProcessTLMpacket() again.
  }

  LbtCcaTimerStart();
  const bool packetSuccessful = ProcessTLMpacket(status);
  return packetSuccessful;
}

void ICACHE_RAM_ATTR TXdoneISR()
{
  if (!busyTransmitting)
  {
    return; // Already finished transmission and do not call HandleFHSS() a second time, which may hop the frequency!
  }

  if (connectionState != awaitingModelId)
  {
    const uint8_t modResult = (OtaNonce + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

    // If TLM enabled and next packet is going to be telemetry, or in LBT mode, start listening to have a large receive window (time-wise)
    const bool nextIsTLM = ExpressLRS_currTlmDenom != 1 && ((OtaNonce + 1) % ExpressLRS_currTlmDenom) == 0;
    const bool doRx = nextIsTLM || LbtIsEnabled;

    // If the next packet should be on the next FHSS frequency, do the hop
    if (!InBindingMode && modResult == 0)
    {
      // Gemini mode
      // If using DualBand always set the correct frequency band to the radios.  The HighFreq/LowFreq Tx amp is set during config.
      if (inGeminiMode() || FHSSuseDualBand)
      {
        // Optimises the SPI traffic order.
        if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1)
        {
          const uint32_t freqRadio = FHSSgetNextFreq();
          Radio.SetFrequencyReg(FHSSgetGeminiFreq(), SX12XX_Radio_2, doRx);
          Radio.SetFrequencyReg(freqRadio, SX12XX_Radio_1, doRx);
        }
        else
        {
          Radio.SetFrequencyReg(FHSSgetNextFreq(), SX12XX_Radio_1, doRx);
          Radio.SetFrequencyReg(FHSSgetGeminiFreq(), SX12XX_Radio_2, doRx);
        }
      }
      else
      {
        Radio.SetFrequencyReg(FHSSgetNextFreq(), SX12XX_Radio_All, doRx);
      }
    }
    else if (doRx)
    {
      Radio.RXnb();
    }

    // If TLM enabled and next packet is going to be telemetry
    if (nextIsTLM)
    {
      TelemetryRcvPhase = ttrpPreReceiveGap;
    }
    else if (doRx)
    {
      LbtCcaTimerStart();
    }
  }
  busyTransmitting = false;
}

static void UpdateConnectDisconnectStatus()
{
  // Number of telemetry packets which can be lost in a row before going to disconnected state
  constexpr unsigned RX_LOSS_CNT = 5;
  // Must be at least 512ms and +2 to account for any rounding down and partial millis()
  const uint32_t msConnectionLostTimeout = std::max((uint32_t)512U,
    (uint32_t)ExpressLRS_currTlmDenom * ExpressLRS_currAirRate_Modparams->interval / (1000U / RX_LOSS_CNT)
    ) + 2U;
  // Capture the last before now so it will always be <= now
  const uint32_t lastTlmMillis = LastTLMpacketRecv_Ms;
  const uint32_t now = millis();
  if (lastTlmMillis && ((now - lastTlmMillis) <= msConnectionLostTimeout))
  {
    if (connectionState != connected)
    {
      setConnectionState(connected);
      DBGLN("got downlink conn");

      apInputBuffer.flush();
      apOutputBuffer.flush();
      uartInputBuffer.flush();
    }
  }
  // If past RX_LOSS_CNT, or in awaitingModelId state for longer than DisconnectTimeoutMs, go to disconnected
  else if (connectionState == connected ||
    (connectionState == awaitingModelId && (now - rfModeLastChangedMS) > ExpressLRS_currAirRate_RFperfParams->DisconnectTimeoutMs))
  {
    setConnectionState(disconnected);
    linkStats.uplink_Link_quality = 0;
    LinkStatsLastReported_Ms = 0; // Notify immediately
    connectionHasModelMatch = true;
  }
}

void clearOTAQueue()
{
    otaConnector.resetOutputQueue();
}

void SetSyncSpam()
{
  // Send sync spam if a UI device has requested to and the config has changed
  if (config.IsModified())
  {
    syncSpamCounter = syncSpamAmount;
    syncSpamCounterAfterRateChange = syncSpamAmountAfterRateChange;
  }
}

static void SendRxWiFiOverMSP()
{
  MSPDataPackage[0] = MSP_ELRS_SET_RX_WIFI_MODE;
  MspSender.SetDataToTransmit(MSPDataPackage, 1);
}

static void CheckReadyToSend()
{
  if (RxWiFiReadyToSend)
  {
    RxWiFiReadyToSend = false;
    if (!handset->IsArmed())
    {
      SendRxWiFiOverMSP();
    }
  }
}

void OnPowerGetCalibration(mspPacket_t *packet)
{
  uint8_t index = packet->readByte();
  UNUSED(index);
  int8_t values[PWR_COUNT] = {0};
  POWERMGNT::GetPowerCaliValues(values, PWR_COUNT);
  DBGLN("power get calibration value %d",  values[index]);
}

void OnPowerSetCalibration(mspPacket_t *packet)
{
  uint8_t index = packet->readByte();
  int8_t value = packet->readByte();

  if((index < 0) || (index >= PWR_COUNT))
  {
    DBGLN("calibration error index %d out of range", index);
    return;
  }
  hwTimer::stop();
  delay(20);

  int8_t values[PWR_COUNT] = {0};
  POWERMGNT::GetPowerCaliValues(values, PWR_COUNT);
  values[index] = value;
  POWERMGNT::SetPowerCaliValues(values, PWR_COUNT);
  DBGLN("power calibration done %d, %d", index, value);
  hwTimer::resume();
}

void SendUIDOverMSP()
{
  MSPDataPackage[0] = MSP_ELRS_BIND;
  memcpy(&MSPDataPackage[1], &UID[2], 4);
  BindingSendCount = 0;
  MspSender.ResetState();
  MspSender.SetDataToTransmit(MSPDataPackage, 5);
}

static void EnterBindingMode()
{
  if (InBindingMode)
      return;

  // Disable the TX timer and wait for any TX to complete
  hwTimer::stop();
  while (busyTransmitting);

  // Queue up sending the Master UID as MSP packets
  SendUIDOverMSP();

  // Binding uses 50Hz, and InvertIQ
  OtaCrcInitializer = OTA_VERSION_ID;
  OtaNonce = 0; // Lock the OtaNonce to prevent syncspam packets
  InBindingMode = true; // Set binding mode before SetRFLinkRate() for correct IQ

  // Start attempting to bind
  // Lock the RF rate and freq while binding
  SetRFLinkRate(enumRatetoIndex(RATE_BINDING));

  // Start transmitting again
  hwTimer::resume();

  DBGLN("Entered binding mode at freq = %d", Radio.currFreq);
}

static void ExitBindingMode()
{
  if (!InBindingMode)
    return;

  MspSender.ResetState();

  // Reset CRCInit to UID-defined value
  OtaUpdateCrcInitFromUid();
  InBindingMode = false; // Clear binding mode before SetRFLinkRate() for correct IQ

  UARTconnected();

  SetRFLinkRate(config.GetRate()); //return to original rate

  DBGLN("Exiting binding mode");
}

void EnterBindingModeSafely()
{
  // TX can always enter binding mode safely as the function handles stopping the transmitter
  EnterBindingMode();
}

void ProcessMSPPacket(uint32_t now, mspPacket_t *packet)
{
#if defined(PLATFORM_ESP32)
  // Inspect packet for ELRS specific opcodes
  if (packet->function == MSP_ELRS_FUNC)
  {
    uint8_t opcode = packet->readByte();

    CHECK_PACKET_PARSING();

    switch (opcode)
    {
    case MSP_ELRS_POWER_CALI_GET:
      OnPowerGetCalibration(packet);
      break;
    case MSP_ELRS_POWER_CALI_SET:
      OnPowerSetCalibration(packet);
      break;
    default:
      break;
    }
  }
  else if (packet->function == MSP_SET_VTX_CONFIG)
  {
    if (packet->payload[0] < 48) // Standard 48 channel VTx table size e.g. A, B, E, F, R, L
    {
      config.SetVtxBand(packet->payload[0] / 8 + 1);
      config.SetVtxChannel(packet->payload[0] % 8);
    } else
    {
      return; // Packets containing frequency in MHz are not yet supported.
    }

    VtxTriggerSend();
  }
  else if (packet->function == MSP_ELRS_BACKPACK_SET_PTR && packet->payloadSize == 6)
  {
    processPanTiltRollPacket(now, packet);
  }
  if (packet->function == MSP_ELRS_GET_BACKPACK_VERSION)
  {
    memset(backpackVersion, 0, sizeof(backpackVersion));
    memcpy(backpackVersion, packet->payload, min((size_t)packet->payloadSize, sizeof(backpackVersion)-1));
  }
#endif
}

void ParseMSPData(uint8_t *buf, uint8_t size)
{
  for (uint8_t i = 0; i < size; ++i)
  {
    if (msp.processReceivedByte(buf[i]))
    {
      ProcessMSPPacket(millis(), msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }
}

static void HandleUARTout()
{
  if (firmwareOptions.is_airport)
  {
    auto size = apOutputBuffer.size();
    if (size)
    {
      uint8_t buf[size];
      apOutputBuffer.lock();
      apOutputBuffer.popBytes(buf, size);
      apOutputBuffer.unlock();
      TxUSB->write(buf, size);
    }
  }
}

static void HandleUARTin()
{
  // Read from the USB serial port
  if (TxUSB->available())
  {
    if (firmwareOptions.is_airport)
    {
      auto size = std::min(apInputBuffer.free(), (uint16_t)TxUSB->available());
      if (size > 0)
      {
        uint8_t buf[size];
        TxUSB->readBytes(buf, size);
        apInputBuffer.lock();
        apInputBuffer.pushBytes(buf, size);
        apInputBuffer.unlock();
      }
    }
    else
    {
      auto size = std::min(uartInputBuffer.free(), (uint16_t)TxUSB->available());
      if (size > 0)
      {
        uint8_t buf[size];
        TxUSB->readBytes(buf, size);
        uartInputBuffer.lock();
        uartInputBuffer.pushBytes(buf, size);
        uartInputBuffer.unlock();

        // Lets check if the data is Mav and auto change LinkMode
        // Start the hwTimer since the user might be operating the module as a standalone unit without a handset.
        if (connectionState == noCrossfire)
        {
          if (isThisAMavPacket(buf, size))
          {
            config.SetLinkMode(TX_MAVLINK_MODE);
            UARTconnected();
          }
        }
      }
    }
  }

  // Read from the Backpack serial port
  if (BackpackOrLogStrm->available())
  {
    auto size = std::min(uartInputBuffer.free(), (uint16_t)BackpackOrLogStrm->available());
    if (size > 0)
    {
      uint8_t buf[size];
      BackpackOrLogStrm->readBytes(buf, size);

      // If the TX is in Mavlink mode, push the bytes into the fifo buffer
      if (config.GetLinkMode() == TX_MAVLINK_MODE)
      {
        uartInputBuffer.lock();
        uartInputBuffer.pushBytes(buf, size);
        uartInputBuffer.unlock();

        // The tx is in Mavlink mode and receiving data from the Backpack.
        // Start the hwTimer since the user might be operating the module as a standalone unit without a handset.
        if (connectionState == noCrossfire)
        {
          if (isThisAMavPacket(buf, size))
          {
            UARTconnected();
          }
        }
      }

      // Try to parse any MSP packets from the Backpack
      ParseMSPData(buf, size);
    }
  }
}

static void setupSerial()
{  /*
   * Setup the logging/backpack serial port, and the USB serial port.
   * This is always done because we need a place to send data even if there is no backpack!
   */

// Setup BackpackOrLogStrm
#if defined(PLATFORM_ESP32)
  Stream *serialPort;

  if(firmwareOptions.is_airport)
  {
    serialPort = new HardwareSerial(1);
    ((HardwareSerial *)serialPort)->begin(firmwareOptions.uart_baud, SERIAL_8N1, U0RXD_GPIO_NUM, U0TXD_GPIO_NUM);
  }
  else if (GPIO_PIN_DEBUG_RX != UNDEF_PIN && GPIO_PIN_DEBUG_TX != UNDEF_PIN)
  {
    serialPort = new HardwareSerial(2);
    ((HardwareSerial *)serialPort)->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1, GPIO_PIN_DEBUG_RX, GPIO_PIN_DEBUG_TX);
  }
  else
  {
    serialPort = new NullStream();
  }
#elif defined(PLATFORM_ESP8266)
  Stream *serialPort;
  if (GPIO_PIN_DEBUG_TX != UNDEF_PIN)
  {
    serialPort = new HardwareSerial(1);
    ((HardwareSerial*)serialPort)->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1, SERIAL_TX_ONLY, GPIO_PIN_DEBUG_TX);
  }
  else
  {
    serialPort = new NullStream();
  }
#endif
  BackpackOrLogStrm = serialPort;

#if defined(PLATFORM_ESP32_S3) || defined(PLATFORM_ESP32_C3)
  Serial.begin(460800);
#endif

// Setup TxUSB
#if defined(PLATFORM_ESP32_S3) || defined(PLATFORM_ESP32_C3)
  USBSerial.begin(firmwareOptions.uart_baud);
  TxUSB = &USBSerial;
#elif defined(PLATFORM_ESP32)
  if (GPIO_PIN_DEBUG_RX == U0RXD_GPIO_NUM && GPIO_PIN_DEBUG_TX == U0TXD_GPIO_NUM)
  {
    // The backpack or Airpoirt is already assigned on UART0 (pins 3, 1)
    // This is also USB on modules that use DIPs
    // Set TxUSB to BackpackOrLogStrm so that data goes to the same place
    TxUSB = BackpackOrLogStrm;
  }
  else if (GPIO_PIN_RCSIGNAL_RX == U0RXD_GPIO_NUM && GPIO_PIN_RCSIGNAL_TX == U0TXD_GPIO_NUM)
  {
    // This is an internal module, or an external module configured with a relay.  Do not setup TxUSB.
    TxUSB = new NullStream();
  }
  else
  {
    // The backpack is on a separate UART to UART0
    // Set TxUSB to pins 3, 1 so that we can access TxUSB and BackpackOrLogStrm independantly
    TxUSB = new HardwareSerial(1);
    ((HardwareSerial *)TxUSB)->begin(firmwareOptions.uart_baud, SERIAL_8N1, U0RXD_GPIO_NUM, U0TXD_GPIO_NUM);
  }
#else
  TxUSB = new NullStream();
#endif
}

/**
 * Target-specific initialization code called early in setup()
 * Setup GPIOs or other hardware, config not yet loaded
 ***/
static void setupTarget()
{
#if defined(PLATFORM_ESP32)
  // arduino-espressif32 HardwareSerial's constructor for UART0 saves and attaches to GPIO 1 and 3, which
  // will reset any other use of them when begin() is actually called for UART0 by CRSFHandset/SerialIO.
  // Calling end() here, will call _uartDetachPins() on the underlying UART implementation so they won't
  // be saved later (fixed upstream, coming someday)
  Serial.end();
#endif

  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    pinMode(GPIO_PIN_ANT_CTRL, OUTPUT);
    digitalWrite(GPIO_PIN_ANT_CTRL, diversityAntennaState);
  }
  if (GPIO_PIN_ANT_CTRL_COMPL != UNDEF_PIN)
  {
    pinMode(GPIO_PIN_ANT_CTRL_COMPL, OUTPUT);
    digitalWrite(GPIO_PIN_ANT_CTRL_COMPL, !diversityAntennaState);
  }

  setupSerial();
  setupTargetCommon();
}

bool setupHardwareFromOptions()
{
  if (!options_init())
  {
    // Register the WiFi with the framework
    static device_affinity_t wifi_device[] = {
        {&WIFI_device, 1}
    };
    devicesRegister(wifi_device, ARRAY_SIZE(wifi_device));
    devicesInit();

    setConnectionState(hardwareUndefined);
    return false;
  }
  return true;
}

static void setupBindingFromConfig()
{
  if (firmwareOptions.hasUID)
  {
    memcpy(UID, firmwareOptions.uid, UID_LEN);
  }
  else
  {
#if defined(PLATFORM_ESP32)
    esp_read_mac(UID, ESP_MAC_WIFI_STA);
#else
    wifi_get_macaddr(STATION_IF, UID);
#endif
  }

  DBGLN("UID=(%u, %u, %u, %u, %u, %u)",
    UID[0], UID[1], UID[2], UID[3], UID[4], UID[5]);

  OtaUpdateCrcInitFromUid();
}


static void cyclePower()
{
  // Only change power if we are running normally
  if (connectionState < MODE_STATES)
  {
    PowerLevels_e curr = POWERMGNT::currPower();
    if (curr == POWERMGNT::getMaxPower())
    {
      POWERMGNT::setPower(POWERMGNT::getMinPower());
    }
    else
    {
      POWERMGNT::incPower();
    }
  }
}

static void checkSendLinkStatsToHandset(uint32_t now)
{
  if ((now - LinkStatsLastReported_Ms) > firmwareOptions.tlm_report_interval)
  {
    uint8_t linkStatisticsFrame[CRSF_FRAME_NOT_COUNTED_BYTES + CRSF_FRAME_SIZE(sizeof(crsfLinkStatistics_t))];

    crsfRouter.makeLinkStatisticsPacket(linkStatisticsFrame);
    // the linkStats originates from the OTA connector so we don't send it back there.
    crsfRouter.deliverMessage(&otaConnector, (crsf_header_t *)linkStatisticsFrame);
    sendCRSFTelemetryToBackpack(linkStatisticsFrame);
    LinkStatsLastReported_Ms = now;
  }
}

void setup()
{
  if (setupHardwareFromOptions())
  {
    setupTarget();
    // Register the devices with the framework
    devicesRegister(ui_devices, ARRAY_SIZE(ui_devices));
    // Initialise the devices
    devicesInit();
    DBGLN("Initialised devices");

    setupBindingFromConfig();
    FHSSrandomiseFHSSsequence(uidMacSeedGet());

    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;

    crsfTransmitter.begin();
    crsfRouter.addConnector(&otaConnector);
    crsfRouter.addEndpoint(&crsfTransmitter);
    // When a CRSF handset is detected, it will add itself to the router

    handset->registerCallbacks(UARTconnected, firmwareOptions.is_airport ? nullptr : UARTdisconnected);

    eeprom.Begin(); // Init the eeprom
    config.SetStorageProvider(&eeprom); // Pass pointer to the Config class for access to storage
    config.Load(); // Load the stored values from eeprom

    Radio.currFreq = FHSSgetInitialFreq(); //set frequency first or an error will occur!!!
    #if defined(RADIO_SX127X)
    //Radio.currSyncWord = UID[3];
    #endif
    bool init_success;
    #if defined(USE_BLE_JOYSTICK)
    init_success = true; // No radio is attached with a joystick only module.  So we are going to fake success so that crsf, hwTimer etc are initiated below.
    #else
    if (GPIO_PIN_SCK != UNDEF_PIN)
    {
      init_success = Radio.Begin(FHSSgetMinimumFreq(), FHSSgetMaximumFreq());
    }
    else
    {
      // Assume BLE Joystick mode if no radio SCK pin
      init_success = true;
    }
    #endif

    if (!init_success)
    {
      setConnectionState(radioFailed);
    }
    else
    {
      TelemetryReceiver.SetDataToReceive(CRSFinBuffer, sizeof(CRSFinBuffer));

      POWERMGNT::init();
      DynamicPower_Init();

      // Set the pkt rate, TLM ratio, and power from the stored eeprom values
      ChangeRadioParams();

      LbtCcaTimerStart();
      hwTimer::init(nullptr, timerCallback);
      setConnectionState(noCrossfire);
    }
  }
  else
  {
    // In the failure case we set the logging to the null logger so nothing crashes
    // if it decides to log something
    BackpackOrLogStrm = new NullStream();
    TxUSB = BackpackOrLogStrm;
  }

  registerButtonFunction(ACTION_BIND, EnterBindingMode);
  registerButtonFunction(ACTION_INCREASE_POWER, cyclePower);

  devicesStart();

  if (firmwareOptions.is_airport)
  {
    config.SetTlm(TLM_RATIO_1_2); // Force TLM ratio of 1:2 for balanced bi-dir link
    config.SetMotionMode(0); // Ensure motion detection is off
    UARTconnected();
  }
}

void loop()
{
  uint32_t now = millis();

  HandleUARTout(); // Only used for non-CRSF output

  #if defined(USE_BLE_JOYSTICK)
  if (connectionState != bleJoystick && connectionState != noCrossfire) // Wait until the correct crsf baud has been found
  {
      connectionState = bleJoystick;
  }
  #endif

  if (connectionState < MODE_STATES)
  {
    UpdateConnectDisconnectStatus();
  }

  // Update UI devices
  devicesUpdate(now);

  // Not a device because it must be run on the loop core
  checkBackpackUpdate();

  // If the reboot time is set and the current time is past the reboot time then reboot.
  if (rebootTime != 0 && now > rebootTime) {
    ESP.restart();
  }

  executeDeferredFunction(micros());

  HandleUARTin();

  if (connectionState > MODE_STATES)
  {
    return;
  }

  CheckReadyToSend();
  CheckConfigChangePending();
  DynamicPower_Update(now);
  VtxPitmodeSwitchUpdate();
  checkSendLinkStatsToHandset(now);

  if (TelemetryReceiver.HasFinishedData())
  {
      if (CRSFinBuffer[0] == CRSF_ADDRESS_USB)
      {
        if (config.GetLinkMode() == TX_MAVLINK_MODE)
        {
          const uint8_t count = CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX];
          // Convert to CRSF telemetry where we can and send to handset
          convert_mavlink_to_crsf_telem(&otaConnector, CRSFinBuffer, count);
          // forward raw mavlink data to USB
          TxUSB->write(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, count);
          // And to the backpack if we have one
          if (TxUSB != BackpackOrLogStrm)
          {
            sendMAVLinkTelemetryToBackpack(CRSFinBuffer);
          }
        }
      }
      else
      {
        // Send all other tlm to handset
        crsfRouter.processMessage(&otaConnector, (crsf_header_t *)CRSFinBuffer);
        sendCRSFTelemetryToBackpack(CRSFinBuffer);
      }
      TelemetryReceiver.Unlock();
  }

  // only send msp data when binding is not active
  if (InBindingMode)
  {
#if defined(RADIO_LR1121)
    // Send half of the bind packets on the 2.4GHz domain
    if (BindingSendCount == BindingSpamAmount / 2) {
      SetRFLinkRate(RATE_DUALBAND_BINDING);
      // Increment BindingSendCount so that SetRFLinkRate is only called once.
      BindingSendCount++;
    }
#endif
    // exit bind mode if package after some repeats
    if (BindingSendCount > BindingSpamAmount) {
      ExitBindingMode();
    }
  }
  else if (!MspSender.IsActive())
  {
    otaConnector.pumpSender();
  }

  if (config.GetLinkMode() == TX_MAVLINK_MODE)
  {
    // Use MspSender for MAVLINK uplink data
    uint8_t *nextPayload = 0;
    uint8_t nextPlayloadSize = 0;
    uint16_t count = uartInputBuffer.size();
    if (count > 0 && !MspSender.IsActive())
    {
        count = std::min(count, (uint16_t)CRSF_PAYLOAD_SIZE_MAX);
        mavlinkSSBuffer[0] = MSP_ELRS_MAVLINK_TLM; // Used on RX to differentiate between std msp opcodes and mavlink
        mavlinkSSBuffer[1] = count;
        // Following n bytes are just raw mavlink
        uartInputBuffer.lock();
        uartInputBuffer.popBytes(mavlinkSSBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, count);
        uartInputBuffer.unlock();
        nextPayload = mavlinkSSBuffer;
        nextPlayloadSize = count + CRSF_FRAME_NOT_COUNTED_BYTES;
        MspSender.SetDataToTransmit(nextPayload, nextPlayloadSize);
    }
  }
}
