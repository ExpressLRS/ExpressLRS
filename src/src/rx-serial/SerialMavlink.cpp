#if defined(TARGET_RX)

#include "SerialMavlink.h"
#include "device.h"
#include "common.h"
#include "CRSF.h"
#include "config.h"

#define MAVLINK_RC_PACKET_INTERVAL 10

// Variables / constants for Mavlink //
FIFO<MAV_INPUT_BUF_LEN> mavlinkInputBuffer;
FIFO<MAV_OUTPUT_BUF_LEN> mavlinkOutputBuffer;

#if defined(PLATFORM_STM32)
// This is a dummy implementation for STM32, since we don't use Mavlink on STM32

    SerialMavlink::SerialMavlink(Stream &out, Stream &in):
    SerialIO(&out, &in),
    // These init values don't matter here for STM32, since we don't use them
    this_system_id(255),
    this_component_id(0),
    target_system_id(1),
    target_component_id(1)
{
}

uint32_t SerialMavlink::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    return DURATION_NEVER;
}

int SerialMavlink::getMaxSerialReadSize()
{
    return 0;
}

void SerialMavlink::processBytes(uint8_t *bytes, u_int16_t size)
{
}

void SerialMavlink::sendQueuedData(uint32_t maxBytesToSend)
{
}

void SerialMavlink::event()
{
}

#else // ESP-based targets

#define MAVLINK_COMM_NUM_BUFFERS 1
#include "common/mavlink.h"

#define MAV_FTP_OPCODE_OPENFILERO 4

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
}

void SerialMavlink::sendQueuedData(uint32_t maxBytesToSend)
{

    // Send radio messages at 100Hz
    const uint32_t now = millis();
    if ((now - lastSentFlowCtrl) > 10)
    {
        lastSentFlowCtrl = now; 

        // Software-based flow control for mavlink
        uint8_t percentage_remaining = ((MAV_INPUT_BUF_LEN - mavlinkInputBuffer.size()) * 100) / MAV_INPUT_BUF_LEN;

        // Populate radio status packet
        const mavlink_radio_status_t radio_status {
            rxerrors: 0,
            fixed: 0,
            rssi: (uint8_t)((float)CRSF::LinkStatistics.uplink_Link_quality * 2.55),
            remrssi: CRSF::LinkStatistics.uplink_RSSI_1,
            txbuf: percentage_remaining,
            noise: (uint8_t)CRSF::LinkStatistics.uplink_SNR,
            remnoise: 0,
        };

        uint8_t buf[MAVLINK_MSG_ID_RADIO_STATUS_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES];
        mavlink_message_t msg;
        mavlink_msg_radio_status_encode(this_system_id, this_component_id, &msg, &radio_status);
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        _outputPort->write(buf, len);
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

    for (uint8_t i = 0; i < size; ++i)
    {
        uint8_t c = apBuf[i];

        mavlink_message_t msg;
        mavlink_status_t status;

        // Try parse a mavlink message
        if (mavlink_frame_char(MAVLINK_COMM_0, c, &msg, &status))
        {
            // Message decoded successfully

            // Forward message to the UART
            uint8_t buf[MAVLINK_MAX_PACKET_LEN];
            uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
            _outputPort->write(buf, len);
        }
    }
}

void SerialMavlink::event()
{
    if(devicesCheckEvent(DEVEVENT_CONFIGCHANGED))
    {
        this_system_id = config.GetSourceSysId() ? config.GetSourceSysId() : 255;
        target_system_id = config.GetTargetSysId() ? config.GetTargetSysId() : 1;
    }
}
#endif // defined(PLATFORM_STM32)

#endif // defined(TARGET_RX)
