#if defined(TARGET_RX)

#include "SerialMavlink.h"
#include "device.h"
#include "common.h"
#include "CRSF.h"

#include "common/mavlink.h"

#define MAV_FTP_OPCODE_OPENFILERO 4

// Variables / constants for Airport //
FIFO_GENERIC<AP_MAX_BUF_LEN> mavlinkInputBuffer;
FIFO_GENERIC<AP_MAX_BUF_LEN> mavlinkOutputBuffer;


void SerialMavlink::setLinkQualityStats(uint16_t lq, uint16_t rssi)
{
    // unsupported
}

void SerialMavlink::sendLinkStatisticsToFC()
{
    // unsupported
}

uint32_t SerialMavlink::sendRCFrameToFC(bool frameAvailable, uint32_t *channelData)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;
    mavlink_msg_rc_channels_override_pack(255, 0, &msg, 1, 1, 
        CRSF_to_US(channelData[0]),
        CRSF_to_US(channelData[1]),
        CRSF_to_US(channelData[2]),
        CRSF_to_US(channelData[3]),
        CRSF_to_US(channelData[4]),
        CRSF_to_US(channelData[5]),
        CRSF_to_US(channelData[6]),
        CRSF_to_US(channelData[7]),
        CRSF_to_US(channelData[8]),
        1500,
        1600,
        1700,
        1800,
        1900,
        2000,
        1000,
        1500,
        2000);
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    // _outputPort->write(buf, len);
    
    return DURATION_IMMEDIATELY;
}

void SerialMavlink::sendMSPFrameToFC(uint8_t* data)
{
    // unsupported
}

int SerialMavlink::getMaxSerialReadSize()
{
    return AP_MAX_BUF_LEN - mavlinkInputBuffer.size();
}

void SerialMavlink::processBytes(uint8_t *bytes, u_int16_t size)
{
    if (connectionState == connected)
    {
        mavlinkInputBuffer.pushBytes(bytes, size);
    }
}

void SerialMavlink::handleUARTout()
{
    // Software-based flow control for mavlink
    uint16_t percentageFull = (mavlinkInputBuffer.size() * 100) / AP_MAX_BUF_LEN;

    uint32_t now = millis();

    if (now > lastSentFlowCtrl + 10 || percentageFull > 50)
    {
        lastSentFlowCtrl = now;
        
        uint8_t rssi = 0;
        uint8_t remrssi = 0;
        uint8_t txbuf = 0;
        uint8_t noise = 0;
        uint8_t remnoise = 0;
        uint16_t rxerrors = 0;
        uint16_t fixed = 0;

        // Scale the actual buf percentage to a more agressive value
        // Rates borrowed and adjusted from mLRS
        if (percentageFull > 60)
        {
            txbuf = 0;      // ArduPilot:  0-19  -> +60 ms,    PX4:  0-24  -> *0.8
        }
        else if (percentageFull > 50)
        {
            txbuf = 30;     // ArduPilot: 20-49  -> +20 ms,    PX4: 25-34  -> *0.975
        }
        else if (percentageFull < 5)
        {
            txbuf = 100;    // ArduPilot: 96-100 -> -40 ms,    PX4: 51-100 -> *1.025
        }
        else if (percentageFull < 10)
        {
            txbuf = 91;     // ArduPilot: 91-95  -> -20 ms,    PX4: 51-100 -> *1.025
        }
        else
        {
            txbuf = 50;     // ArduPilot: 50-90  -> no change, PX4: 35-50  -> no change
        }

        uint8_t buf[MAVLINK_MSG_ID_RADIO_STATUS_LEN];
        mavlink_message_t msg;
        mavlink_msg_radio_status_pack(255, 0, &msg, rssi, remrssi, txbuf, noise, remnoise, rxerrors, fixed);
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        _outputPort->write(buf, len);
    }

    auto size = mavlinkOutputBuffer.size();
    uint8_t apBuf[size];
    mavlinkOutputBuffer.popBytes(apBuf, size);

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
                uint8_t buf[MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL_LEN];
                uint16_t len = mavlink_msg_file_transfer_protocol_get_payload(&msg, buf);

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
                        return;
                    }
                }
            }
            
            uint8_t buf[MAVLINK_MAX_PAYLOAD_LEN];
            uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
            _outputPort->write(buf, len);
        }
    }
}
#endif
