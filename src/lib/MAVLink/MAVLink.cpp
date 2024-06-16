#include "MAVLink.h"
#if !defined(PLATFORM_STM32)
    #include "ardupilot_protocol.h"
#endif

void convert_mavlink_to_crsf_telem(uint8_t *CRSFinBuffer, uint8_t count, Handset *handset)
{
#if !defined(PLATFORM_STM32)
    for (uint8_t i = 0; i < count; i++)
    {
        mavlink_message_t msg;
        mavlink_status_t status;
        bool have_message = mavlink_parse_char(MAVLINK_COMM_0, CRSFinBuffer[CRSF_FRAME_NOT_COUNTED_BYTES + i], &msg, &status);
        // convert mavlink messages to CRSF messages
        if (have_message)
        {
            switch (msg.msgid)
            {
            case MAVLINK_MSG_ID_BATTERY_STATUS: {
                mavlink_battery_status_t battery_status;
                mavlink_msg_battery_status_decode(&msg, &battery_status);
                CRSF_MK_FRAME_T(crsf_sensor_battery_t)
                crsfbatt = {0};
                // mV -> mv*100
                crsfbatt.p.voltage = htobe16(battery_status.voltages[0] / 100);
                // cA -> mA*100
                crsfbatt.p.current = htobe16(battery_status.current_battery / 10);
                crsfbatt.p.capacity = htobe32(battery_status.current_consumed) & 0x0FFF;
                crsfbatt.p.remaining = battery_status.battery_remaining;
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfbatt, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfbatt);
                break;
            }
            case MAVLINK_MSG_ID_GPS_RAW_INT: {
                mavlink_gps_raw_int_t gps_int;
                mavlink_msg_gps_raw_int_decode(&msg, &gps_int);
                CRSF_MK_FRAME_T(crsf_sensor_gps_t)
                crsfgps = {0};
                // mm -> meters + 1000
                crsfgps.p.altitude = htobe16(gps_int.alt / 1000 + 1000);
                // cm/s -> km/h / 10
                crsfgps.p.groundspeed = htobe16(gps_int.vel * 36 / 100);
                crsfgps.p.latitude = htobe32(gps_int.lat);
                crsfgps.p.longitude = htobe32(gps_int.lon);
                crsfgps.p.gps_heading = htobe16(gps_int.cog);
                crsfgps.p.satellites_in_use = gps_int.satellites_visible;
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfgps, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfgps);
                break;
            }
            case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
                mavlink_global_position_int_t global_pos;
                mavlink_msg_global_position_int_decode(&msg, &global_pos);
                CRSF_MK_FRAME_T(crsf_sensor_baro_vario_t)
                crsfbaro = {0};
                crsfbaro.p.altitude = htobe16(global_pos.relative_alt / 100 + 10000);
                // force the top bit of altitude low to represent that it's in decimeters
                crsfbaro.p.altitude &= 0x7FFF;
                crsfbaro.p.verticalspd = htobe16(global_pos.vz);
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfbaro, CRSF_FRAMETYPE_BARO_ALTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_baro_vario_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfbaro);
                break;
            }
            case MAVLINK_MSG_ID_ATTITUDE: {
                mavlink_attitude_t attitude;
                mavlink_msg_attitude_decode(&msg, &attitude);
                CRSF_MK_FRAME_T(crsf_sensor_attitude_t)
                crsfatt = {0};
                crsfatt.p.pitch = htobe16(attitude.pitch * 10000);
                crsfatt.p.roll = htobe16(attitude.roll * 10000);
                crsfatt.p.yaw = htobe16(attitude.yaw * 10000);
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfatt, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_attitude_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfatt);
                break;
            }
            case MAVLINK_MSG_ID_HEARTBEAT: {
                mavlink_heartbeat_t heartbeat;
                mavlink_msg_heartbeat_decode(&msg, &heartbeat);
                CRSF_MK_FRAME_T(crsf_flight_mode_t)
                crsffm = {0};
                ap_flight_mode_name4(crsffm.p.flight_mode, ap_vehicle_from_mavtype(heartbeat.type), heartbeat.custom_mode);
                CRSF::SetHeaderAndCrc((uint8_t *)&crsffm, CRSF_FRAMETYPE_FLIGHT_MODE, CRSF_FRAME_SIZE(sizeof(crsffm)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsffm);
                break;
            }
            }
        }
    }
#endif
}
