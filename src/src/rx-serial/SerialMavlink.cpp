#if defined(TARGET_RX)

#include "SerialMavlink.h"
#include "OTA.h"
#include "common.h"
#include "config.h"
#include "device.h"
#include "logging.h"
#include "MavlinkFramer.h"

#if defined(PLATFORM_ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>

#define MAVLINK_RC_PACKET_INTERVAL 10

#define MAVLINK_COMM_NUM_BUFFERS 1
#include "common/mavlink.h"

#define MAV_FTP_OPCODE_OPENFILERO 4

// In WiFi mode the radio is off. The RX then connects the flight controller UART
// to a GCS over UDP. The RX listens on MAVLINK_UDP_LOCAL_PORT. It broadcasts to
// MAVLINK_UDP_GCS_PORT until a GCS sends a packet, then it sends to that GCS only.
#define MAVLINK_UDP_LOCAL_PORT  14555
#define MAVLINK_UDP_GCS_PORT    14550   // The default GCS port of QGroundControl and Mission Planner
#define MAVLINK_UDP_GCS_TIMEOUT 5000    // ms without a packet from the GCS before the RX broadcasts again

struct SerialMavlink::WifiLink
{
    WiFiUDP udp;
    MavlinkFramer<512> framer;
    IPAddress gcsIp;
    uint16_t gcsPort = 0;
    uint32_t lastGcsPacketMs = 0;
};

SerialMavlink::SerialMavlink(Stream &out, Stream &in):
    SerialIO(&out, &in),

    //system ID of the device component sending command to FC, can be set using lua options, 0 is the default value for initialized storage, treat it as 255 which is commonly used as GCS SysID
    this_system_id(config.GetSourceSysId() ? config.GetSourceSysId() : 255),
    //use telemetry radio compId as we are providing radio status messages and pass telemetry
    this_component_id(MAV_COMPONENT::MAV_COMP_ID_TELEMETRY_RADIO),

    // system ID of vehicle we want to control must be the same as target vehicle, can be set using lua options, 0 is the default value for initialized storage, treat it as 1 which is commonly used as UAV SysID in 1:1 networks
    target_system_id(config.GetTargetSysId() ? config.GetTargetSysId() : 1),
    // Send to all components as we may have ex. gimbal that listens to RC instead of using Autopilot driver
    target_component_id(MAV_COMPONENT::MAV_COMP_ID_ALL)
{
}

SerialMavlink::~SerialMavlink()
{
    delete wifi;
}

uint32_t SerialMavlink::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    if (!frameAvailable) {
        return DURATION_IMMEDIATELY;
    }

    const mavlink_rc_channels_override_t rc_override {
        chan1_raw: CRSF_to_US(channelData[0]),
        chan2_raw: CRSF_to_US(channelData[1]),
        chan3_raw: CRSF_to_US(channelData[2]),
        chan4_raw: CRSF_to_US(channelData[3]),
        chan5_raw: CRSF_to_US(channelData[4]),
        chan6_raw: CRSF_to_US(channelData[5]),
        chan7_raw: CRSF_to_US(channelData[6]),
        chan8_raw: CRSF_to_US(channelData[7]),
        target_system: target_system_id,
        target_component: target_component_id,
        chan9_raw: CRSF_to_US(channelData[8]),
        chan10_raw: CRSF_to_US(channelData[9]),
        chan11_raw: CRSF_to_US(channelData[10]),
        chan12_raw: CRSF_to_US(channelData[11]),
        chan13_raw: CRSF_to_US(channelData[12]),
        chan14_raw: CRSF_to_US(channelData[13]),
        chan15_raw: CRSF_to_US(channelData[14]),
        chan16_raw: CRSF_to_US(channelData[15]),
    };

    uint8_t buf[MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES];
    mavlink_message_t msg;
    mavlink_msg_rc_channels_override_encode(this_system_id, this_component_id, &msg, &rc_override);
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    _outputPort->write(buf, len);

    return MAVLINK_RC_PACKET_INTERVAL;
}

int SerialMavlink::getMaxSerialReadSize()
{
    return MAV_INPUT_BUF_LEN - mavlinkInputBuffer.size();
}

void SerialMavlink::processBytes(uint8_t *bytes, u_int16_t size)
{
    if (connectionState == connected)
    {
        mavlinkInputBuffer.atomicPushBytes(bytes, size);
    }
    else if (wifi != nullptr)
    {
        for (uint16_t i = 0; i < size; ++i)
        {
            wifi->framer.push(bytes[i], [this](const uint8_t *data, uint16_t len) { sendToGcs(data, len); });
        }
    }
}

void SerialMavlink::sendRadioStatus()
{
    // Software-based flow control for mavlink
    uint8_t percentage_remaining = ((MAV_INPUT_BUF_LEN - mavlinkInputBuffer.size()) * 100) / MAV_INPUT_BUF_LEN;

    // Populate radio status packet
    mavlink_radio_status_t radio_status {
        rxerrors: 0,
        fixed: 0,
        rssi: (uint8_t)((float)linkStats.uplink_Link_quality * 2.55),
        remrssi: linkStats.uplink_RSSI_1,
        txbuf: percentage_remaining,
        noise: (uint8_t)linkStats.uplink_SNR,
        remnoise: 0,
    };
    if (wifi != nullptr)
    {
        // There is no radio link in WiFi mode, UINT8_MAX is "unknown"
        radio_status.rssi = UINT8_MAX;
        radio_status.remrssi = UINT8_MAX;
        radio_status.noise = UINT8_MAX;
        radio_status.remnoise = UINT8_MAX;
    }

    uint8_t buf[MAVLINK_MSG_ID_RADIO_STATUS_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES];
    mavlink_message_t msg;
    mavlink_msg_radio_status_encode(this_system_id, this_component_id, &msg, &radio_status);
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    _outputPort->write(buf, len);
}

void SerialMavlink::sendQueuedData(uint32_t maxBytesToSend)
{
    handleWifi();

    // Send radio messages at 100Hz
    const uint32_t now = millis();
    if ((now - lastSentFlowCtrl) > 10)
    {
        lastSentFlowCtrl = now;
        sendRadioStatus();
    }

    auto size = mavlinkOutputBuffer.size();
    if (size == 0)
    {
        // nothing to send
        return;
    }

    uint8_t apBuf[size];
    mavlinkOutputBuffer.lock();
    mavlinkOutputBuffer.popBytes(apBuf, size);
    mavlinkOutputBuffer.unlock();

    for (uint16_t i = 0; i < size; ++i)
    {
        uint8_t c = apBuf[i];

        mavlink_message_t msg;
        mavlink_status_t status;

        // Try parse a mavlink message
        if (mavlink_frame_char(MAVLINK_COMM_0, c, &msg, &status) == MAVLINK_FRAMING_OK)
        {
            // Message decoded successfully

            // Forward message to the UART
            uint8_t buf[MAVLINK_MAX_PACKET_LEN];
            uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
            _outputPort->write(buf, len);
        }
    }
}

static IPAddress wifiBroadcastIP()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return IPAddress((uint32_t)WiFi.localIP() | ~(uint32_t)WiFi.subnetMask());
    }
    // The netmask of the access point is 255.255.255.0, see devWIFI
    return IPAddress((uint32_t)WiFi.softAPIP() | ~(uint32_t)IPAddress(255, 255, 255, 0));
}

void SerialMavlink::sendToGcs(const uint8_t *data, const uint16_t len)
{
    if (wifi->gcsPort != 0 && millis() - wifi->lastGcsPacketMs < MAVLINK_UDP_GCS_TIMEOUT)
    {
        wifi->udp.beginPacket(wifi->gcsIp, wifi->gcsPort);
    }
    else
    {
        wifi->udp.beginPacket(wifiBroadcastIP(), MAVLINK_UDP_GCS_PORT);
    }
    wifi->udp.write(data, len);
    wifi->udp.endPacket();
}

void SerialMavlink::handleWifi()
{
    if (connectionState != wifiUpdate)
    {
        if (wifi != nullptr)
        {
            delete wifi;
            wifi = nullptr;
        }
        return;
    }

    if (wifi == nullptr)
    {
        // The UDP socket needs the network stack, which starts with the WiFi
        if (WiFi.getMode() == WIFI_OFF)
        {
            return;
        }
        wifi = new WifiLink();
        wifi->udp.begin(MAVLINK_UDP_LOCAL_PORT);
        DBGLN("MAVLink UDP on port %u", MAVLINK_UDP_LOCAL_PORT);
    }

    // GCS to flight controller: write each datagram to the UART
    while (wifi->udp.parsePacket() > 0)
    {
        wifi->gcsIp = wifi->udp.remoteIP();
        wifi->gcsPort = wifi->udp.remotePort();
        wifi->lastGcsPacketMs = millis();

        uint8_t buf[128];
        int len;
        while ((len = wifi->udp.read(buf, sizeof(buf))) > 0)
        {
            _outputPort->write(buf, len);
        }
    }

    // Flight controller to GCS: processBytes() puts the frames in the framer, send them now
    wifi->framer.flush([this](const uint8_t *data, uint16_t len) { sendToGcs(data, len); });
}

void SerialMavlink::event()
{
    this_system_id = config.GetSourceSysId() ? config.GetSourceSysId() : 255;
    target_system_id = config.GetTargetSysId() ? config.GetTargetSysId() : 1;
}

void SerialMavlink::forwardMessage(const uint8_t *data)
{
    mavlinkOutputBuffer.atomicPushBytes(data + 2, data[1]);
}

bool SerialMavlink::GetNextPayload(uint8_t* nextPayloadSize, uint8_t *payloadData)
{
    if (mavlinkInputBuffer.size() == 0)
    {
        return false;
    }
    const uint16_t count = std::min(mavlinkInputBuffer.size(), (uint16_t)CRSF_PAYLOAD_SIZE_MAX); // Constrain to CRSF max payload size to match SS
    payloadData[0] = CRSF_ADDRESS_USB; // device_addr - used on TX to differentiate between std tlm and mavlink
    payloadData[1] = count;
    // The following 'n' bytes are just raw mavlink
    mavlinkInputBuffer.popBytes(payloadData + CRSF_FRAME_NOT_COUNTED_BYTES, count);
    *nextPayloadSize = count + CRSF_FRAME_NOT_COUNTED_BYTES;
    return true;
}

#endif // defined(TARGET_RX)
