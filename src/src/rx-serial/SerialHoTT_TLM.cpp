#if defined(TARGET_RX) && (defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32))

#include "SerialHoTT_TLM.h"
#include "FIFO.h"
#include "telemetry.h"
#include "common.h"

#define NOT_FOUND 0xff          // no device found indicator

#define HOTT_POLL_RATE 150      // default HoTT bus poll rate [ms]
#define HOTT_LEAD_OUT 10        // minimum gap between end of payload to next poll

#define HOTT_CMD_DELAY 1        // 1 ms delay between CMD byte 1 and 2
#define HOTT_WAIT_TX_COMPLETE 2 // 2 ms wait for CMD bytes transmission complete

#define DISCOVERY_TIMEOUT 30000 // 30s device discovery time

#define VARIO_MIN_CRSFRATE 1000 // CRSF telemetry packets will be sent if
#define GPS_MIN_CRSFRATE 5000   // min rate timers in [ms] have expired
#define BATT_MIN_CRSFRATE 5000  // or packet value has changed. Fastest to
                                // be expected update rate will by about 150ms due
                                // to HoTT bus speed if only a HoTT Vario is connected and
                                // values change every HoTT bus poll cycle.


extern Telemetry telemetry;

int SerialHoTT_TLM::getMaxSerialReadSize()
{
    return HOTT_MAX_BUF_LEN - hottInputBuffer.size();
}

void SerialHoTT_TLM::setTXMode()
{
#if defined(PLATFORM_ESP32)
    pinMode(halfDuplexPin, OUTPUT);                                 // set half duplex GPIO to OUTPUT
    digitalWrite(halfDuplexPin, HIGH);                              // set half duplex GPIO to high level
    pinMatrixOutAttach(halfDuplexPin, UTXDoutIdx, false, false);    // attach GPIO as output of UART TX
#endif
}

void SerialHoTT_TLM::setRXMode()
{
#if defined(PLATFORM_ESP32)
    pinMode(halfDuplexPin, INPUT_PULLUP);                           // set half duplex GPIO to INPUT
    pinMatrixInAttach(halfDuplexPin, URXDinIdx, false);             // attach half duplex GPIO as input to UART RX
#endif
}

void SerialHoTT_TLM::processBytes(uint8_t *bytes, u_int16_t size)
{
    hottInputBuffer.pushBytes(bytes, size);

    uint8_t bufferSize = hottInputBuffer.size();

    if (bufferSize == sizeof(hottBusFrame))
    {
        // frame complete, prepare to poll next device after lead out time elapsed
        lastPoll = millis() - HOTT_POLL_RATE + HOTT_LEAD_OUT;

        // fetch received serial data
        hottInputBuffer.popBytes((uint8_t *)&hottBusFrame, bufferSize);

        // process received frame if CRC is ok
        if (hottBusFrame.payload[STARTBYTE_INDEX] == START_FRAME_B &&
            hottBusFrame.payload[ENDBYTE_INDEX] == END_FRAME &&
            hottBusFrame.payload[CRC_INDEX] == calcFrameCRC((uint8_t *)&hottBusFrame.payload))
        {
            processFrame();
        }
    }
}

void SerialHoTT_TLM::sendQueuedData(uint32_t maxBytesToSend)
{
    uint32_t now = millis();

    if(connectionState != connected)
    {
        // suspend device discovery timer until receiver is connected
        discoveryTimerStart = now;      
    }

    // device discovery timer
    if (discoveryMode && (now - discoveryTimerStart >= DISCOVERY_TIMEOUT))
    {
        discoveryMode = false;
    }

    // device polling scheduler
    scheduleDevicePolling(now);

    // CRSF packet scheduler
    scheduleCRSFtelemetry(now);
}

void SerialHoTT_TLM::scheduleDevicePolling(uint32_t now)
{
    // send CMD byte 1
    if (now - lastPoll >= HOTT_POLL_RATE)
    {
        lastPoll = now;

        // work out next device to be polled. All devices in discovery
        // mode, only detected devices in non-discovery mode)  
        nextDeviceID = NOT_FOUND;

        for (uint i = FIRST_DEVICE; i < LAST_DEVICE; i++)
        {
            if (nextDevice == LAST_DEVICE)
            {
                nextDevice = FIRST_DEVICE;
            }

            if (device[nextDevice].present || discoveryMode)
            {
                nextDeviceID = device[nextDevice].deviceID;

                nextDevice++;
                
                break;
            }

            nextDevice++;
        }

        // no device found, nothing to do
        if (nextDeviceID == NOT_FOUND)
        {
            return;
        }

        // clear serial in buffer
        hottInputBuffer.flush();

        // switch to half duplex TX mode and write CMD byte 1
        setTXMode();
        _outputPort->write(START_OF_CMD_B);
        cmdSendState = HOTT_CMD1SENT;
        return;
    }

    // delay sending CMD byte 2 to accomodate for slow devices
    if ((now - lastPoll >= HOTT_CMD_DELAY) && cmdSendState == HOTT_CMD1SENT)
    {
        _outputPort->write(nextDeviceID);
        cmdSendState = HOTT_CMD2SENT;
        return;
    }

    // wait for the last byte being sent out to switch to RX mode
    if ((now - lastPoll >= HOTT_WAIT_TX_COMPLETE) && cmdSendState == HOTT_CMD2SENT)
    {
        // switch to half duplex listen mode
        setRXMode();
        cmdSendState = HOTT_RECEIVING;
    }
}

void SerialHoTT_TLM::processFrame()
{
    void *frameData = (void *)&hottBusFrame.payload;

    // store received frame
    switch (hottBusFrame.payload[DEVICE_INDEX])
    {
    case SENSOR_ID_GPS_B:
        device[GPS].present = true;
        memcpy((void *)&gps, frameData, sizeof(gps));
        break;

    case SENSOR_ID_EAM_B:
        device[EAM].present = true;
        memcpy((void *)&eam, frameData, sizeof(eam));
        break;

    case SENSOR_ID_GAM_B:
        device[GAM].present = true;
        memcpy((void *)&gam, frameData, sizeof(gam));
        break;

    case SENSOR_ID_VARIO_B:
        device[VARIO].present = true;
        memcpy((void *)&vario, frameData, sizeof(vario));
        break;

    case SENSOR_ID_ESC_B:
        device[ESC].present = true;
        memcpy((void *)&esc, frameData, sizeof(esc));
        break;
    }
}

uint8_t SerialHoTT_TLM::calcFrameCRC(uint8_t *buf)
{
    uint16_t sum = 0;

    for (uint8_t i = 0; i < FRAME_SIZE - 1; i++)
    {
        sum += buf[i];
    }

    return sum = sum & 0xff;
}

void SerialHoTT_TLM::scheduleCRSFtelemetry(uint32_t now)
{
    // HoTT combined GPS/Vario -> send GPS and vario packet
    if (device[GPS].present)
    {
        sendCRSFgps(now);
        sendCRSFvario(now);
    }
    else
        // HoTT stand alone Vario and no GPS/Vario -> just send vario packet
        if (device[VARIO].present)
        {
            sendCRSFvario(now);
        }

    // HoTT GAM, EAM, ESC -> send battery packet
    if (device[GAM].present || device[EAM].present || device[ESC].present)
    {
        sendCRSFbattery(now);

        // HoTT GAM and EAM but no GPS/Vario or Vario -> send vario packet too
        if ((!device[GPS].present && !device[VARIO].present) && (device[GAM].present || device[EAM].present))
        {
            sendCRSFvario(now);
        }
    }
}

void SerialHoTT_TLM::sendCRSFvario(uint32_t now)
{
    // indicate external sensor is present
    telemetry.SetCrsfBaroSensorDetected();

    // prepare CRSF telemetry packet
    CRSF_MK_FRAME_T(crsf_sensor_baro_vario_t)
    crsfBaro = {0};
    crsfBaro.p.altitude = htobe16(getHoTTaltitude() * 10 + 5000); // Hott 500 = 0m, ELRS 10000 = 0.0m
    crsfBaro.p.verticalspd = htobe16(getHoTTvv() - 30000);
    CRSF::SetHeaderAndCrc((uint8_t *)&crsfBaro, CRSF_FRAMETYPE_BARO_ALTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_baro_vario_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);

    // send packet only if min rate timer expired or values have changed
    if ((now - lastVarioSent >= VARIO_MIN_CRSFRATE) || (lastVarioCRC != crsfBaro.crc))
    {
        lastVarioSent = now;

        telemetry.AppendTelemetryPackage((uint8_t *)&crsfBaro);
    }

    lastVarioCRC = crsfBaro.crc;
}

void SerialHoTT_TLM::sendCRSFgps(uint32_t now)
{
    // prepare CRSF telemetry packet
    CRSF_MK_FRAME_T(crsf_sensor_gps_t)
    crsfGPS = {0};
    crsfGPS.p.latitude = htobe32(getHoTTlatitude());
    crsfGPS.p.longitude = htobe32(getHoTTlongitude());
    crsfGPS.p.groundspeed = htobe16(getHoTTgroundspeed() * 10); // Hott 1 = 1 km/h, ELRS 1 = 0.1km/h
    crsfGPS.p.gps_heading = htobe16(getHoTTheading() * 100);
    crsfGPS.p.altitude = htobe16(getHoTTMSLaltitude() + 1000); // HoTT 1 = 1m, CRSF: 0m = 1000
    crsfGPS.p.satellites_in_use = getHoTTsatellites();
    CRSF::SetHeaderAndCrc((uint8_t *)&crsfGPS, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);

    // send packet only if min rate timer expired or values have changed
    if ((now - lastGPSSent >= GPS_MIN_CRSFRATE) || (lastGPSCRC != crsfGPS.crc))
    {
        lastGPSSent = now;

        telemetry.AppendTelemetryPackage((uint8_t *)&crsfGPS);
    }

    lastGPSCRC = crsfGPS.crc;
}

void SerialHoTT_TLM::sendCRSFbattery(uint32_t now)
{
    // indicate external sensor is present
    telemetry.SetCrsfBatterySensorDetected();

    // prepare CRSF telemetry packet
    CRSF_MK_FRAME_T(crsf_sensor_battery_t)
    crsfBatt = {0};
    crsfBatt.p.voltage = htobe16(getHoTTvoltage());
    crsfBatt.p.current = htobe16(getHoTTcurrent());
    crsfBatt.p.capacity = htobe24(getHoTTcapacity() * 10); // HoTT: 1 = 10mAh, CRSF: 1 ? 1 = 1mAh
    crsfBatt.p.remaining = getHoTTremaining();
    CRSF::SetHeaderAndCrc((uint8_t *)&crsfBatt, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);

    // send packet only if min rate timer expired or values have changed
    if ((now - lastBatterySent >= BATT_MIN_CRSFRATE) || (lastBatteryCRC != crsfBatt.crc))
    {
        lastBatterySent = now;

        telemetry.AppendTelemetryPackage((uint8_t *)&crsfBatt);
    }

    lastBatteryCRC = crsfBatt.crc;
}

// HoTT telemetry data getters
uint16_t SerialHoTT_TLM::getHoTTvoltage()
{
    if (device[EAM].present)
    {
        return eam.mainVoltage;
    }
    else if (device[GAM].present)
    {
        return (gam.inputVoltage);
    }
    else if (device[ESC].present)
    {
        return esc.inputVoltage;
    }

    return 0;
}

uint16_t SerialHoTT_TLM::getHoTTcurrent()
{
    if (device[EAM].present)
    {
        return eam.current;
    }
    else if (device[GAM].present)
    {
        return gam.current;
    }
    else if (device[ESC].present)
    {
        return esc.current;
    }
    
    return 0;
}

uint32_t SerialHoTT_TLM::getHoTTcapacity()
{
    if (device[EAM].present)
    {
        return eam.capacity;
    }
    else if (device[GAM].present)
    {
        return (gam.capacity);
    }
    else if (device[ESC].present)
    {
        return esc.capacity;
    }

    return 0;
}

int16_t SerialHoTT_TLM::getHoTTaltitude()
{
    if (device[GPS].present)
    {
        return gps.altitude;
    }
    else if (device[VARIO].present)
    {
        return vario.altitude;
    }
    else if (device[EAM].present)
    {
        return eam.altitude;
    }
    else if (device[GAM].present)
    {
        return gam.altitude;
    }

    return 0;
}

int16_t SerialHoTT_TLM::getHoTTvv()
{
    if (device[GPS].present)
    {
        return (gps.mPerSec);
    }
    else if (device[VARIO].present)
    {
        return (vario.mPerSec);
    }
    else if (device[EAM].present)
    {
        return (eam.mPerSec);
    }
    else if (device[GAM].present)
    {
        return (gam.mPerSec);
    }

    return 0;
}

uint8_t SerialHoTT_TLM::getHoTTremaining()
{
    if (device[GAM].present)
    {
        return gam.fuelScale;
    }

    return 0;
}

int32_t SerialHoTT_TLM::getHoTTlatitude()
{
    if (!device[GPS].present)
    {
        return 0;
    }

    uint8_t deg = gps.latDegMin / DegMinScale;

    int32_t Lat = deg * DegScale +
                  ((gps.latDegMin - (deg * DegMinScale)) * MinScale) / MinDivide +
                  (gps.latSec * SecScale) / MinDivide;

    if (gps.latNS != 0)
    {
        Lat = -Lat;
    }

    return (Lat);
}

int32_t SerialHoTT_TLM::getHoTTlongitude()
{
    if (!device[GPS].present)
    {
        return 0;
    }

    uint8_t deg = gps.lonDegMin / DegMinScale;

    int32_t Lon = deg * DegScale +
                  ((gps.lonDegMin - (deg * DegMinScale)) * MinScale) / MinDivide +
                  (gps.lonSec * SecScale) / MinDivide;

    if (gps.lonEW != 0)
        Lon = -Lon;

    return Lon;
}

uint16_t SerialHoTT_TLM::getHoTTgroundspeed()
{
    if (device[GPS].present)
    {
        return gps.speed;
    }

    return 0;
}

uint16_t SerialHoTT_TLM::getHoTTheading()
{
    if (!device[GPS].present)
    {
        return 0;
    }

    uint16_t heading = gps.direction * 2;

    if (heading > 180)
    {
        heading -= 360;
    }

    return (heading);
}

uint8_t SerialHoTT_TLM::getHoTTsatellites()
{
    if (device[GPS].present)
    {
        return gps.satellites;
    }

    return 0;
}

uint16_t SerialHoTT_TLM::getHoTTMSLaltitude()
{
    if (device[GPS].present)
    {
        return gps.mslAltitude;
    }

    return 0;
}

uint32_t SerialHoTT_TLM::htobe24(uint32_t val)
{
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return val;
#else
    uint8_t *ptrByte = (uint8_t *)&val;

    uint8_t swp = ptrByte[0];
    ptrByte[0] = ptrByte[2];
    ptrByte[2] = swp;

    return val;
#endif
}

#endif
