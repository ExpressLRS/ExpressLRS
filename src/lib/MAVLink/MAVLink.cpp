#include "MAVLink.h"

#include "CRSFRouter.h"
#include "common/mavlink.h"

#include "ardupilot_custom_telemetry.h"
#include "ardupilot_protocol.h"

/*
 * Helper function to send an ardupilot specific CRSF passthrough frame
 * with a single data item appid is the function that produces the data.
 */
static void ap_send_crsf_passthrough_single(crsf_addr_e destination, uint16_t appid, uint32_t data)
{
#define CRSF_AP_CUSTOM_TELEM_SINGLE_PACKET_PASSTHROUGH (0xF0)
    struct PACKED ap_crsf_passthrough_single_t {
        uint8_t sub_type;
        uint16_t appid;
        uint32_t data;
    };
    CRSF_MK_FRAME_T(ap_crsf_passthrough_single_t)
    crsfpassthrough = {0};
    crsfpassthrough.p.sub_type = CRSF_AP_CUSTOM_TELEM_SINGLE_PACKET_PASSTHROUGH;
    crsfpassthrough.p.appid = appid;
    crsfpassthrough.p.data = data;

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfpassthrough, CRSF_FRAMETYPE_ARDUPILOT_RESP, CRSF_FRAME_SIZE(sizeof(crsfpassthrough)));
    crsfRouter.deliverMessageTo(destination, &crsfpassthrough.h);
}

/*
 * Helper function to send an ardupilot specific CRSF passthrough frame
 * with a text payload.
 */
static void ap_send_crsf_passthrough_text(crsf_addr_e destination, const char *text, uint8_t severity)
{
#define CRSF_AP_CUSTOM_TELEM_STATUS_TEXT (0xF1)
    struct PACKED ap_crsf_status_text_t {
        uint8_t sub_type;
        uint8_t severity;
        char text[50];  // identical to mavlink message
    };
    CRSF_MK_FRAME_T(ap_crsf_status_text_t)
    crsftext = {0};
    crsftext.p.sub_type = CRSF_AP_CUSTOM_TELEM_STATUS_TEXT;
    crsftext.p.severity = severity;
    memcpy(crsftext.p.text, text, sizeof(crsftext.p.text));

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsftext, CRSF_FRAMETYPE_ARDUPILOT_RESP, CRSF_FRAME_SIZE(sizeof(crsftext)));
    crsfRouter.deliverMessageTo(destination, &crsftext.h);
}

/*
 * Helper function to send an ardupilot specific CRSF passthrough frame
 * with 2 data item appid is the function that produces the data.
 */
static void ap_send_crsf_passthrough_multi(crsf_addr_e destination, uint16_t appid, uint32_t data, uint16_t appid2, uint32_t data2)
{
#define CRSF_AP_CUSTOM_TELEM_MULTI_PACKET_PASSTHROUGH (0xF2)
    struct PACKED ap_crsf_passthrough_tuple_t {
        uint16_t appid;
        uint32_t data;
    };
    struct PACKED ap_crsf_passthrough_multi_t {
        uint8_t sub_type;
        uint8_t count;
        ap_crsf_passthrough_tuple_t items[2];
    };
    CRSF_MK_FRAME_T(ap_crsf_passthrough_multi_t)
    crsfpassthrough = {0};
    crsfpassthrough.p.sub_type = CRSF_AP_CUSTOM_TELEM_MULTI_PACKET_PASSTHROUGH;
    crsfpassthrough.p.count = 2;
    crsfpassthrough.p.items[0].appid = appid;
    crsfpassthrough.p.items[0].data = data;
    crsfpassthrough.p.items[1].appid = appid2;
    crsfpassthrough.p.items[1].data = data2;

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfpassthrough, CRSF_FRAMETYPE_ARDUPILOT_RESP, CRSF_FRAME_SIZE(sizeof(crsfpassthrough)));
    crsfRouter.deliverMessageTo(destination, &crsfpassthrough.h);
}

void convert_mavlink_to_crsf_telem(crsf_addr_e destination, uint8_t *CRSFinBuffer, uint8_t count)
{
    // Store the relative altitude for GPS altitude
    static int32_t relative_alt_mm = 0;

    // Store the throttle value for AP_STATUS concatenation
    static uint32_t throttle_prc = 0;

    // Store the home position for distance and bearing calculation
    static int32_t home_latitude_degE7 = 0;
    static int32_t home_longitude_degE7 = 0;

    for (uint8_t i = 0; i < count; i++)
    {
        mavlink_message_t msg;
        mavlink_status_t status;
        bool have_message = mavlink_frame_char(MAVLINK_COMM_0, CRSFinBuffer[CRSF_FRAME_NOT_COUNTED_BYTES + i], &msg, &status);
        // convert mavlink messages to CRSF messages
        if (have_message)
        {
            // Only parse heartbeats from the autopilot (not GCS)
            if (msg.compid != MAV_COMP_ID_AUTOPILOT1)
            {
                continue;
            }
            switch (msg.msgid)
            {
            case MAVLINK_MSG_ID_BATTERY_STATUS: {
                mavlink_battery_status_t battery_status;
                mavlink_msg_battery_status_decode(&msg, &battery_status);
                if (battery_status.id != 0) {
                    break;
                }
                CRSF_MK_FRAME_T(crsf_sensor_battery_t)
                crsfbatt = {0};
                // mV -> mv*100
                crsfbatt.p.voltage = htobe16(battery_status.voltages[0] / 100);
                // cA -> mA*100
                crsfbatt.p.current = 0;
                if (battery_status.current_battery > 0){ // int16_t, -1 means invalid
                    crsfbatt.p.current = htobe16(((uint16_t) battery_status.current_battery) / 10);
                }
                // mAh
                crsfbatt.p.capacity = 0;
                if (battery_status.current_consumed > 0){ // int32_t, -1 means invalid
                    crsfbatt.p.capacity = htobe32(std::min(((uint32_t) battery_status.current_consumed), (uint32_t) 0xFFFFFFU)); // 24bit value
                }
                // 0-100%
                crsfbatt.p.remaining = 0;
                if (battery_status.battery_remaining > 0){ // int8_t, -1 means invalid
                    crsfbatt.p.remaining = (uint8_t) battery_status.battery_remaining;
                }
                crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfbatt, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)));
                crsfRouter.deliverMessageTo(destination, &crsfbatt.h);

                // send the batt1 message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(destination, 0x5003, format_batt1(battery_status.voltages[0], battery_status.current_battery, battery_status.current_consumed));
                break;
            }
            case MAVLINK_MSG_ID_GPS_RAW_INT: {
                mavlink_gps_raw_int_t gps_int;
                mavlink_msg_gps_raw_int_decode(&msg, &gps_int);
                CRSF_MK_FRAME_T(crsf_sensor_gps_t)
                crsfgps = {0};
// We use altitude relative to home for GPS altitude, by default, but we can also use GPS altitude if USE_MAVLINK_GPS_ALTITUDE is defined
#if defined(USE_MAVLINK_GPS_ALTITUDE)
                // mm -> meters + 1000
                crsfgps.p.altitude = htobe16(gps_int.alt / 1000 + 1000);
#else
                crsfgps.p.altitude = htobe16((uint16_t)(relative_alt_mm / 1000 + 1000));
#endif
                // cm/s -> km/h / 10
                crsfgps.p.groundspeed = htobe16(gps_int.vel * 36 / 100);
                crsfgps.p.latitude = htobe32(gps_int.lat);
                crsfgps.p.longitude = htobe32(gps_int.lon);
                crsfgps.p.gps_heading = htobe16(gps_int.cog);
                crsfgps.p.satellites_in_use = gps_int.satellites_visible;
                crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfgps, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)));
                crsfRouter.deliverMessageTo(destination, &crsfgps.h);

                // send the gps_status message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(destination, 0x5002, format_gps_status(gps_int.fix_type, gps_int.alt, gps_int.eph, gps_int.satellites_visible));

                // send the home message to Yaapu Telemetry Script
                uint32_t bearing_deg = 0;
                uint32_t distance_to_home_m = 0;
                if ((home_latitude_degE7 != 0) && (home_longitude_degE7 != 0)){
                    int32_t dlon = diff_longitude(home_longitude_degE7,gps_int.lon);
                    dlon = scale_longitude((home_latitude_degE7 + gps_int.lat) >> 1,
                                           dlon);
                    cartesian_to_polar_coordinates(home_latitude_degE7 - gps_int.lat,
                                                   dlon,
                                                   &bearing_deg, &distance_to_home_m);
                }
                ap_send_crsf_passthrough_single(destination, 0x5004, format_home(distance_to_home_m, relative_alt_mm/100, bearing_deg));
                break;
            }
            case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
                mavlink_global_position_int_t global_pos;
                mavlink_msg_global_position_int_decode(&msg, &global_pos);
                CRSF_MK_FRAME_T(crsf_sensor_vario_t)
                crsfvario = {0};
                // store relative altitude for GPS Alt so we don't have 2 Alt sensors
                relative_alt_mm = global_pos.relative_alt;
                crsfvario.p.verticalspd = htobe16(-global_pos.vz); // MAVLink vz is positive down
                crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfvario, CRSF_FRAMETYPE_VARIO, CRSF_FRAME_SIZE(sizeof(crsf_sensor_vario_t)));
                crsfRouter.deliverMessageTo(destination, &crsfvario.h);
                break;
            }
            case MAVLINK_MSG_ID_ATTITUDE: {
                mavlink_attitude_t attitude;
                mavlink_msg_attitude_decode(&msg, &attitude);
                CRSF_MK_FRAME_T(crsf_sensor_attitude_t)
                crsfatt = {0};
                crsfatt.p.pitch = htobe16(attitude.pitch * 10000); // in Betaflight & INAV, CRSF positive pitch is nose down, but in Ardupilot, it's nose up - we follow Ardupilot
                crsfatt.p.roll = htobe16(attitude.roll * 10000);
                crsfatt.p.yaw = htobe16(attitude.yaw * 10000);
                crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfatt, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_attitude_t)));
                crsfRouter.deliverMessageTo(destination, &crsfatt.h);

                // send the attitude message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(destination, 0x5006, format_attiandrng(attitude.pitch, attitude.roll));
                break;
            }
            case MAVLINK_MSG_ID_HEARTBEAT: {
                mavlink_heartbeat_t heartbeat;
                mavlink_msg_heartbeat_decode(&msg, &heartbeat);
                CRSF_MK_FRAME_T(crsf_flight_mode_t)
                crsffm = {0};
                ap_flight_mode_name4(crsffm.p.flight_mode, ap_vehicle_from_mavtype(heartbeat.type), heartbeat.custom_mode);
                // if we have a good flight mode, and we're not armed, suffix the flight mode with a * - see Ardupilot's AP_CRSF_Telem::calc_flight_mode() and CRSF_FM_DISARM_STAR option
                size_t len = strnlen(crsffm.p.flight_mode, sizeof(crsffm.p.flight_mode));
                if (len > 0 && (len + 1 < sizeof(crsffm.p.flight_mode)) && !(heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED)) {
                    crsffm.p.flight_mode[len] = '*';
                    crsffm.p.flight_mode[len + 1] = '\0';
                }
                crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsffm, CRSF_FRAMETYPE_FLIGHT_MODE, CRSF_FRAME_SIZE(sizeof(crsffm)));
                crsfRouter.deliverMessageTo(destination, &crsffm.h);

                /**
                 * There is a mandatory order for these message items.
                 * 1. send the frame_type parameter to Yaapu Telemetry Script
                 * 2. send the ap_status message to Yaapu Telemetry Script
                 * Otherwise the Yaapu script will not display flightmode until the next heartbeat is received.
                 */
                ap_send_crsf_passthrough_multi(destination,
                                               0x5007,
                                               format_param(1,
                                                            heartbeat.type
                                                            ),
                                               0x5001,
                                               format_ap_status(heartbeat.base_mode,
                                                                heartbeat.custom_mode,
                                                                heartbeat.system_status,
                                                                throttle_prc
                                                                )
                                               );
                break;
            }
            case MAVLINK_MSG_ID_STATUSTEXT: {
                mavlink_statustext_t statustext;
                mavlink_msg_statustext_decode(&msg, &statustext);
                // send status_text message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_text(destination, statustext.text, statustext.severity);
                break;
            }
            case MAVLINK_MSG_ID_VFR_HUD: {
                mavlink_vfr_hud_t vfr_hud;
                mavlink_msg_vfr_hud_decode(&msg, &vfr_hud);
                // stash the throttle value
                throttle_prc = vfr_hud.throttle;
                ap_send_crsf_passthrough_multi(destination,
                                               0x5005,
                                               format_velandyaw(vfr_hud.climb, vfr_hud.airspeed, vfr_hud.groundspeed, vfr_hud.heading),
                                               0x5005,
                                               format_velandyaw(vfr_hud.climb, vfr_hud.airspeed, vfr_hud.groundspeed, vfr_hud.heading)
                                               );
                break;
            }
            case MAVLINK_MSG_ID_HOME_POSITION: {
                mavlink_home_position_t home_pos;
                mavlink_msg_home_position_decode(&msg, &home_pos);
                home_latitude_degE7 = home_pos.latitude;
                home_longitude_degE7 = home_pos.longitude;
                break;
            }
            case MAVLINK_MSG_ID_ALTITUDE: {
                mavlink_altitude_t altitude_data;
                mavlink_msg_altitude_decode(&msg, &altitude_data);
                // send the terrain message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(destination, 0x500B, format_terrain(altitude_data.altitude_terrain));
                break;
            }
            case MAVLINK_MSG_ID_HIGH_LATENCY2: {
                mavlink_high_latency2_t high_latency_data;
                mavlink_msg_high_latency2_decode(&msg, &high_latency_data);
                // send the waypoint message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(destination, 0x500D, format_waypoint(high_latency_data.target_heading, high_latency_data.target_distance, high_latency_data.wp_num));
                break;
            }
            }
        }
    }
}

bool isThisAMavPacket(uint8_t *buffer, uint16_t bufferSize)
{
    for (uint8_t i = 0; i < bufferSize; ++i)
    {
        uint8_t c = buffer[i];

        mavlink_message_t msg;
        mavlink_status_t status;

        // Try parse a mavlink message
        if (mavlink_frame_char(MAVLINK_COMM_0, c, &msg, &status))
        {
            // Message decoded successfully
            return true;
        }
    }
    return false;
}

uint16_t buildMAVLinkELRSModeChange(uint8_t mode, uint8_t *buffer)
{
    constexpr uint8_t ELRS_MODE_CHANGE = 0x8;
    mavlink_command_int_t commandMsg{};
    commandMsg.target_system = 255;
    commandMsg.target_component = MAV_COMP_ID_UDP_BRIDGE;
    commandMsg.command = MAV_CMD_USER_1;
    commandMsg.param1 = ELRS_MODE_CHANGE;
    commandMsg.param2 = mode;
    mavlink_message_t msg;
    mavlink_msg_command_int_encode(255, MAV_COMP_ID_TELEMETRY_RADIO, &msg, &commandMsg);
    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    return len;
}