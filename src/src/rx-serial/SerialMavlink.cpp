#if defined(TARGET_RX)

#include "SerialMavlink.h"
#include "device.h"
#include "common.h"
#include "CRSF.h"

#include "common/mavlink.h"

#define MAV_FTP_OPCODE_OPENFILERO 4

// Variables / constants for Mavlink //
FIFO<AP_MAX_BUF_LEN> mavlinkInputBuffer;
FIFO<AP_MAX_BUF_LEN> mavlinkOutputBuffer;

SerialMavlink::SerialMavlink(Stream &out, Stream &in):
    SerialIO(&out, &in),
    // 255 is typically used by the GCS, for RC override to work in ArduPilot `SYSID_MYGCS` must be set to this value (255 is the default)
    this_system_id(255),
    // Strictly this is not a valid source component ID
    this_component_id(MAV_COMPONENT::MAV_COMP_ID_ALL),
    // Assume vehicle system ID is 1, ArduPilot's `SYSID_THISMAV` parameter. (1 is the default)
    target_system_id(1),
    // Send to AutoPilot component
    target_component_id(MAV_COMPONENT::MAV_COMP_ID_AUTOPILOT1)
{
}

uint32_t SerialMavlink::sendRCFrame(bool frameAvailable, uint32_t *channelData)
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
    
    return DURATION_IMMEDIATELY;
}

int SerialMavlink::getMaxSerialReadSize()
{
    return AP_MAX_BUF_LEN - mavlinkInputBuffer.size();
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
        uint8_t percentage_remaining = ((AP_MAX_BUF_LEN - mavlinkInputBuffer.size()) * 100) / AP_MAX_BUF_LEN;

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
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status))
        {
            // Message decoded successfully

            // Check if its the ftp file request
            if (msg.msgid == MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL)
            {
                // If it is, we want to deliberately hack the ftp url, so that the request fails
                // Borrowed and modified from mLRS
                uint8_t buf[MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES];
                mavlink_msg_file_transfer_protocol_get_payload(&msg, buf);

                uint8_t target_component = buf[0];
                uint8_t opcode = buf[3];
                char* url = (char*)(buf + 12);

                if (((target_component == MAV_COMP_ID_AUTOPILOT1) || (target_component == MAV_COMP_ID_ALL)) &&
                    (opcode == MAV_FTP_OPCODE_OPENFILERO))
                {
                    if (!strncmp(url, "@PARAM/param.pck", 16))
                    {
                        url[1] = url[7] = url[13] = 'x'; // now fake it to "@xARAM/xaram.xck"
                        mavlink_message_t msg2;
                        mavlink_msg_file_transfer_protocol_pack(msg.sysid, msg.compid, &msg2, 0, 0, target_component, (uint8_t*)url);
                        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg2);
                        _outputPort->write(buf, len);
                    }
                }
            }
            else
            {
                uint8_t buf[MAVLINK_MAX_PACKET_LEN];
                uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
                _outputPort->write(buf, len);
            }
            
        }
    }
}
#endif
