#include "MAVLink.h"
#include "ardupilot_protocol.h"
#include <math.h>

/*
 * Known Issues:
 * - Battery Capacity is currently not available via Mavlink BATTERY_INFO message is not implemented on Ardupilot side.
 *   This can be worked around by setting the battery capacity via Yaapu Config option.
 */

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))


#define NUMBER_OF_CORDIC_ITERATIONS (6U)
/**
 * static lookup for the angle corresponding tan(2^-i) * 1000
 * essentially the degree that corresponds to a tan(degree) = [1, 0.5, 0.25, 0.125 0.0625, ...]
 */
static uint16_t lookup_theta[NUMBER_OF_CORDIC_ITERATIONS] = {45000, 26565, 14036, 7125, 3576, 1789};

/**
 * COS(degree) of phi [0.7071067811865476, 0.8944271909999159, 0.9701425001453319, 0.9922778767136676, 0.9980525784828885, 0.9995120760870788]
 * product of all factors 0.607351770141296 * 1000
 */
#define KFACTOR1000 (607)

/**
 * Cordic Algo to transform cartesian coordinates into polar coordinates
 */
static void cordic_cartesion_to_polar(int32_t north, int32_t east, int32_t *phi, int32_t *radius);
static void cordic_cartesion_to_polar(int32_t north, int32_t east, int32_t *phi, int32_t *radius)
{
    int32_t x = north;
    int32_t y = east;
    int32_t offset = 0;
    // compared to the academic examples, by swapping x and y, we have mirrored the coordinate system along the main diagonale
    // this also makes the angle and the sign of the angle correct for the typical heading where North is 0 and positive is clockwise.
    if (x < 0){
        // the algo requires x to be positive
        x = -x;
        y = -y;
        offset = 180;
    }
    int8_t sign = 1;
    int32_t theta = 0;
    
    for (uint8_t index=0; index < NUMBER_OF_CORDIC_ITERATIONS; index++){
        sign = (y >= 0) ? 1 : -1;
        x -= (abs(y) >> index) * sign;
        y += (abs(x) >> index) * sign;
        theta += lookup_theta[index] * sign;
    }
    *phi = (theta/1000) + offset;
    *radius = (x * KFACTOR1000) / 1000;
    return;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort::prep_number()
 * This is a proprietary number format that must be known on a value basis between producer and consumer.
 */
static uint16_t prep_number(int32_t number, uint8_t digits, uint8_t power);
static uint16_t prep_number(int32_t number, uint8_t digits, uint8_t power)
{
    uint16_t res = 0;
    uint32_t abs_number = abs(number);

    if ((digits == 2) && (power == 0)) { // number encoded on 7 bits, client side needs to know if expected range is 0,127 or -63,63
        uint8_t max_value = number < 0 ? (0x1<<6)-1 : (0x1<<7)-1;
        res = constrain(abs_number,0,max_value);
        if (number < 0) {   // if number is negative, add sign bit in front
            res |= 1U<<6;
        }
    } else if ((digits == 2) && (power == 1)) { // number encoded on 8 bits: 7 bits for digits + 1 for 10^power
        if (abs_number < 100) {
            res = abs_number<<1;
        } else if (abs_number < 1270) {
            res = ((abs_number / 10)<<1)|0x1;
        } else { // transmit max possible value (0x7F x 10^1 = 1270)
            res = 0xFF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<8;
        }
    } else if ((digits == 2) && (power == 2)) { // number encoded on 9 bits: 7 bits for digits + 2 for 10^power
        if (abs_number < 100) {
            res = abs_number<<2;
        } else if (abs_number < 1000) {
            res = ((abs_number / 10)<<2)|0x1;
        } else if (abs_number < 10000) {
            res = ((abs_number / 100)<<2)|0x2;
        } else if (abs_number < 127000) {
            res = ((abs_number / 1000)<<2)|0x3;
        } else { // transmit max possible value (0x7F x 10^3 = 127000)
            res = 0x1FF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<9;
        }
    } else if ((digits == 3) && (power == 1)) { // number encoded on 11 bits: 10 bits for digits + 1 for 10^power
        if (abs_number < 1000) {
            res = abs_number<<1;
        } else if (abs_number < 10240) {
            res = ((abs_number / 1000)<<1)|0x1;
        } else { // transmit max possible value (0x3FF x 10^1 = 10230)
            res = 0x7FF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<11;
        }
    } else if ((digits == 3) && (power == 2)) { // number encoded on 12 bits: 10 bits for digits + 2 for 10^power
        if (abs_number < 1000) {
            res = abs_number<<2;
        } else if (abs_number < 10000) {
            res = ((abs_number / 10)<<2)|0x1;
        } else if (abs_number < 100000) {
            res = ((abs_number / 100)<<2)|0x2;
        } else if (abs_number < 1024000) {
            res = ((abs_number / 1000)<<2)|0x3;
        } else { // transmit max possible value (0x3FF x 10^3 = 1023000)
            res = 0xFFF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<12;
        }
    }
    return res;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_ap_status()
 * This is the content of the 0x5001 AP_STATUS value.
 * Note that we don't have certain values via Mavlink.
 * - IMU Temperature
 * - simple/super simple mode flags
 * - specific failsafe flags
 * - fence flags
 */
static uint32_t format_ap_status(uint8_t base_mode, uint32_t custom_mode, uint8_t system_status, uint16_t throttle);
static uint32_t format_ap_status(uint8_t base_mode, uint32_t custom_mode, uint8_t system_status, uint16_t throttle)
{
#define AP_CONTROL_MODE_LIMIT       0x1F
#define AP_FLYING_OFFSET            7
#define AP_ARMED_OFFSET             8
#define AP_FS_OFFSET                12
#define AP_THROTTLE_OFFSET          19

    // control/flight mode number (limit to 31 (0x1F) since the value is stored on 5 bits)
    uint32_t ap_status = (uint8_t)((custom_mode+1) & AP_CONTROL_MODE_LIMIT);
    // is_flying flag
    if (system_status == MAV_STATE_ACTIVE) {
        ap_status |= (1 << AP_FLYING_OFFSET);
    }
    // armed flag
    if (base_mode & MAV_MODE_FLAG_SAFETY_ARMED) {
        ap_status |= (1 << AP_ARMED_OFFSET);
    }
    // generic failsafe
    if (system_status == MAV_STATE_CRITICAL) {
        ap_status |= (1 << AP_FS_OFFSET);
    }
    // signed throttle [-100,100] scaled down to [-63,63] on 7 bits, MSB for sign + 6 bits for 0-63
    ap_status |= prep_number(throttle*63/100, 2, 0)<<AP_THROTTLE_OFFSET;
    return ap_status;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_gps_status()
 * This is the content of the 0x5002 GPS_STATUS value.
 */
static uint32_t format_gps_status(uint8_t fix_type, int32_t alt_msl_mm, uint16_t eph, uint8_t satellites_visible);
static uint32_t format_gps_status(uint8_t fix_type, int32_t alt_msl_mm, uint16_t eph, uint8_t satellites_visible)
{
#define GPS_SATS_LIMIT              0xF
#define GPS_STATUS_LIMIT            0x3
#define GPS_STATUS_OFFSET           4
#define GPS_HDOP_OFFSET             6
#define GPS_ADVSTATUS_OFFSET        14
#define GPS_ALTMSL_OFFSET           22
    // number of GPS satellites visible (limit to 15 (0xF) since the value is stored on 4 bits)
    uint32_t gps_status = (satellites_visible < GPS_SATS_LIMIT) ? satellites_visible : GPS_SATS_LIMIT;
    // GPS receiver status (limit to 0-3 (0x3) since the value is stored on 2 bits: NO_GPS = 0, NO_FIX = 1, GPS_OK_FIX_2D = 2, GPS_OK_FIX_3D or GPS_OK_FIX_3D_DGPS or GPS_OK_FIX_3D_RTK_FLOAT or GPS_OK_FIX_3D_RTK_FIXED = 3)
    gps_status |= ((fix_type < GPS_STATUS_LIMIT) ? fix_type : GPS_STATUS_LIMIT)<<GPS_STATUS_OFFSET;
    // GPS horizontal dilution of precision in dm
    gps_status |= prep_number(eph / 10,2,1)<<GPS_HDOP_OFFSET;
    // GPS receiver advanced status (0: no advanced fix, 1: GPS_OK_FIX_3D_DGPS, 2: GPS_OK_FIX_3D_RTK_FLOAT, 3: GPS_OK_FIX_3D_RTK_FIXED)
    gps_status |= ((fix_type > GPS_STATUS_LIMIT) ? fix_type-GPS_STATUS_LIMIT : 0)<<GPS_ADVSTATUS_OFFSET;
    // Altitude MSL in dm
    gps_status |= prep_number(alt_msl_mm / 100,2,2)<<GPS_ALTMSL_OFFSET;
    return gps_status;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_batt()
 * This is the content of the 0x5003 BATT1 value.
 */
static uint32_t format_batt1(uint16_t voltage_mv, uint16_t current_ca, uint32_t current_consumed);
static uint32_t format_batt1(uint16_t voltage_mv, uint16_t current_ca, uint32_t current_consumed)
{
#define BATT_VOLTAGE_LIMIT          0x1FF
#define BATT_CURRENT_OFFSET         9
#define BATT_TOTALMAH_LIMIT         0x7FFF
#define BATT_TOTALMAH_OFFSET        17
    // battery voltage in decivolts, can have up to a 12S battery (4.25Vx12S = 51.0V)
    uint32_t batt = ((voltage_mv / 100) & BATT_VOLTAGE_LIMIT);
    // battery current draw in deciamps
    batt |= prep_number(current_ca / 10, 2, 1)<<BATT_CURRENT_OFFSET;
    // battery current drawn since power on in mAh (limit to 32767 (0x7FFF) since value is stored on 15 bits)
    batt |= ((current_consumed < BATT_TOTALMAH_LIMIT) ? (current_consumed & BATT_TOTALMAH_LIMIT) : BATT_TOTALMAH_LIMIT)<<BATT_TOTALMAH_OFFSET;
    return batt;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_home()
 * This is the content of the 0x5004 HOME value.
 */
static uint32_t format_home(uint32_t distance_to_home_m, uint32_t altitude_above_home_dm, uint8_t bearing_to_home_deg);
static uint32_t format_home(uint32_t distance_to_home_m, uint32_t altitude_above_home_dm, uint8_t bearing_to_home_deg)
{
#define HOME_ALT_OFFSET             12
#define HOME_BEARING_LIMIT          0x7F
#define HOME_BEARING_OFFSET         25
    // distance between vehicle and home_loc in meters
    uint32_t home = prep_number(distance_to_home_m, 3, 2);
    // angle from front of vehicle to the direction of home_loc in 3 degree increments (just in case, limit to 127 (0x7F) since the value is stored on 7 bits)
    home |= ((bearing_to_home_deg/3) & HOME_BEARING_LIMIT)<<HOME_BEARING_OFFSET;
    // altitude between vehicle and home_loc in 0.1 meters.
    home |= prep_number(altitude_above_home_dm, 3, 2)<<HOME_ALT_OFFSET;
    return home;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_velandyaw()
 * This is the content of the 0x5005 Velocity and Yaw.
 */
static uint32_t format_velandyaw(float climb_mps, float groundspeed_mps, int16_t heading);
static uint32_t format_velandyaw(float climb_mps, float groundspeed_mps, int16_t heading)
{
#define VELANDYAW_XYVEL_OFFSET      9
#define VELANDYAW_YAW_LIMIT         0x7FF
#define VELANDYAW_YAW_OFFSET        17
    // vertical velocity in dm/s
    uint32_t velandyaw = prep_number(climb_mps * 10, 2, 1);

    // horizontal velocity in dm/s
    velandyaw |= prep_number(groundspeed_mps * 10, 2, 1)<<VELANDYAW_XYVEL_OFFSET;

    // idiotic scaling from int to int*5
    velandyaw |= ((heading * 5) & VELANDYAW_YAW_LIMIT)<<VELANDYAW_YAW_OFFSET;
    return velandyaw;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_attiandrng()
 * This is the content of the 0x5006 Attitude and RangeFinder.
 * We don't provide Rangefinder here.
 */
static uint32_t format_attiandrng(float pitch_rad, float roll_rad);
static uint32_t format_attiandrng(float pitch_rad, float roll_rad)
{
#define ATTIANDRNG_ROLL_LIMIT       0x7FF
#define ATTIANDRNG_PITCH_LIMIT      0x3FF
#define ATTIANDRNG_PITCH_OFFSET     11
#define ATTIANDRNG_RNGFND_OFFSET    21
    uint32_t attiandrng = ((((uint16_t)(roll_rad * 286.0f)) + 900) & ATTIANDRNG_ROLL_LIMIT);
    attiandrng |= ((((uint16_t)(pitch_rad * 286.0f)) + 450) & ATTIANDRNG_PITCH_LIMIT)<<ATTIANDRNG_PITCH_OFFSET;
    return attiandrng;
}

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_terrain()
 * This is the content of the 0x500B terrain.
 */
static uint32_t format_terrain(uint32_t altitude_terrain);
static uint32_t format_terrain(uint32_t altitude_terrain)
{
    uint32_t value = prep_number(altitude_terrain * 10, 3, 2);
    return value;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_waypoint()
 * This is the content of the 0x500B terrain.
 */
static uint32_t format_waypoint(uint8_t heading, uint16_t distance, uint16_t number);
static uint32_t format_waypoint(uint8_t heading, uint16_t distance, uint16_t number)
{
#define WP_NUMBER_LIMIT             2047
#define WP_DISTANCE_LIMIT           1023000
#define WP_DISTANCE_OFFSET          11
#define WP_BEARING_OFFSET           23
    uint32_t value = MIN(number, WP_NUMBER_LIMIT);
    // distance to next waypoint
    value |= prep_number(distance, 3, 2) << WP_DISTANCE_OFFSET;
    // bearing encoded in 3 degrees increments
    value |= (heading * 2 / 3) << WP_BEARING_OFFSET;
    return value;
}

/*
 * Helper function to send an ardupilot specific CRSF passthrough frame
 * with a single data item appid is the function that produces the data.
 */
static void ap_send_crsf_passthrough_single(uint16_t appid, uint32_t data);
static void ap_send_crsf_passthrough_single(uint16_t appid, uint32_t data)
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

    CRSF::SetHeaderAndCrc((uint8_t *)&crsfpassthrough, CRSF_FRAMETYPE_ARDUPILOT_RESP, CRSF_FRAME_SIZE(sizeof(crsfpassthrough)), CRSF_ADDRESS_FLIGHT_CONTROLLER);
    handset->sendTelemetryToTX((uint8_t *)&crsfpassthrough);
    return;
}

/*
 * Helper function to send an ardupilot specific CRSF passthrough frame
 * with a text payload.
 */
static void ap_send_crsf_passthrough_text(const char *text, uint8_t severity);
static void ap_send_crsf_passthrough_text(const char *text, uint8_t severity)
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

    CRSF::SetHeaderAndCrc((uint8_t *)&crsftext, CRSF_FRAMETYPE_ARDUPILOT_RESP, CRSF_FRAME_SIZE(sizeof(crsftext)), CRSF_ADDRESS_FLIGHT_CONTROLLER);
    handset->sendTelemetryToTX((uint8_t *)&crsftext);

}

/*
 * Helper function to send an ardupilot specific CRSF passthrough frame
 * broadcasting a parameter as (id, value)-tuple via 0x5007
 */
static void ap_send_crsf_passthrough_parameter(uint8_t param_id, uint32_t param_value);
static void ap_send_crsf_passthrough_parameter(uint8_t param_id, uint32_t param_value)
{
#define PARAM_ID_OFFSET             24
#define PARAM_VALUE_LIMIT           0xFFFFFF
    ap_send_crsf_passthrough_single(0x5007, (param_id << PARAM_ID_OFFSET) | (param_value & PARAM_VALUE_LIMIT));
}


void convert_mavlink_to_crsf_telem(uint8_t *CRSFinBuffer, uint8_t count, Handset *handset)
{
    // Store the relative altitude for GPS altitude
    static int32_t relative_alt = 0;

    // Store the throttle value for AP_STATUS concatenation
    static uint32_t throttle = 0;

    // Store the home position for distance and bearing calculation
    static int32_t home_latitude = 0;
    static int32_t home_longitude = 0;

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
                crsfbatt.p.current = htobe16(battery_status.current_battery / 10);
                crsfbatt.p.capacity = htobe32(battery_status.current_consumed & 0x0FFF);
                crsfbatt.p.remaining = battery_status.battery_remaining;
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfbatt, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfbatt);

                // send the batt1 message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(0x5003, format_batt1(battery_status.voltages[0], battery_status.current_battery, battery_status.current_consumed));
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
                crsfgps.p.altitude = htobe16((uint16_t)(relative_alt / 1000 + 1000));
#endif
                // cm/s -> km/h / 10
                crsfgps.p.groundspeed = htobe16(gps_int.vel * 36 / 100);
                crsfgps.p.latitude = htobe32(gps_int.lat);
                crsfgps.p.longitude = htobe32(gps_int.lon);
                crsfgps.p.gps_heading = htobe16(gps_int.cog);
                crsfgps.p.satellites_in_use = gps_int.satellites_visible;
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfgps, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfgps);

                // send the gps_status message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(0x5002, format_gps_status(gps_int.fix_type, gps_int.alt, gps_int.eph, gps_int.satellites_visible));


                // send the home message to Yaapu Telemetry Script
                int32_t bearing = 0;
                int32_t distance_to_home = 0;
                if ((home_latitude != 0) && (home_longitude != 0)){
                    int32_t north_m = (home_latitude - gps_int.lat) * 111318;
                    int32_t east_m = (home_longitude - gps_int.lon) * 111318;
                    cordic_cartesion_to_polar(north_m, east_m, &bearing, &distance_to_home);
                }
                ap_send_crsf_passthrough_single(0x5004, format_home(distance_to_home, relative_alt/100, bearing));
                break;
            }
            case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
                mavlink_global_position_int_t global_pos;
                mavlink_msg_global_position_int_decode(&msg, &global_pos);
                CRSF_MK_FRAME_T(crsf_sensor_vario_t)
                crsfvario = {0};
                // store relative altitude for GPS Alt so we don't have 2 Alt sensors
                relative_alt = global_pos.relative_alt;
                crsfvario.p.verticalspd = htobe16(-global_pos.vz); // MAVLink vz is positive down
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfvario, CRSF_FRAMETYPE_VARIO, CRSF_FRAME_SIZE(sizeof(crsf_sensor_vario_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfvario);
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
                CRSF::SetHeaderAndCrc((uint8_t *)&crsfatt, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_attitude_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsfatt);

                // send the attitude message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(0x5006, format_attiandrng(attitude.pitch, attitude.roll));
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
                CRSF::SetHeaderAndCrc((uint8_t *)&crsffm, CRSF_FRAMETYPE_FLIGHT_MODE, CRSF_FRAME_SIZE(sizeof(crsffm)), CRSF_ADDRESS_CRSF_TRANSMITTER);
                handset->sendTelemetryToTX((uint8_t *)&crsffm);

                // send the ap_status message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(0x5001, format_ap_status(heartbeat.base_mode, heartbeat.custom_mode, heartbeat.system_status, throttle));
                // send the frame_type parameter to Yaapu Telemetry Script
                ap_send_crsf_passthrough_parameter(1, heartbeat.type);
                break;
            }
            case MAVLINK_MSG_ID_STATUSTEXT: {
                mavlink_statustext_t statustext;
                mavlink_msg_statustext_decode(&msg, &statustext);
                // send status_text message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_text(statustext.text, statustext.severity);
                break;
            }
            case MAVLINK_MSG_ID_VFR_HUD: {
                mavlink_vfr_hud_t vfr_hud;
                mavlink_msg_vfr_hud_decode(&msg, &vfr_hud);
                // stash the throttle value
                throttle = vfr_hud.throttle;
                // send the velocity and yaw message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(0x5005, format_velandyaw(vfr_hud.climb, vfr_hud.groundspeed, vfr_hud.heading));
                break;
            }
            case MAVLINK_MSG_ID_HOME_POSITION: {
                mavlink_home_position_t home_pos;
                mavlink_msg_home_position_decode(&msg, &home_pos);
                home_latitude = home_pos.latitude;
                home_longitude = home_pos.longitude;
                break;
            }
            case MAVLINK_MSG_ID_ALTITUDE: {
                mavlink_altitude_t altitude_data;
                mavlink_msg_altitude_decode(&msg, &altitude_data);
                // send the terrain message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(0x500B, format_terrain(altitude_data.altitude_terrain));
                break;
            }
            case MAVLINK_MSG_ID_HIGH_LATENCY2: {
                mavlink_high_latency2_t high_latency_data;
                mavlink_msg_high_latency2_decode(&msg, &high_latency_data);
                // send the waypoint message to Yaapu Telemetry Script
                ap_send_crsf_passthrough_single(0x500D, format_waypoint(high_latency_data.target_heading, high_latency_data.target_distance, high_latency_data.wp_num));
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
    mavlink_command_int_t commandMsg;
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
