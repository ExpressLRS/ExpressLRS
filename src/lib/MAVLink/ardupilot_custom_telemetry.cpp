/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * lib/MAVLink/ardupilot_custom_telemetry.cpp
 *
 * Ardupilot Custom Telemetry forging from Mavlink Messages.
 * Cordic algorithm to determine bearing to home and distance to home.
 *
 * Copyright (C) 2025 Patrick Menschel (menschel.p@posteo.de)
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Contributed to ExpressLRS project.
 * https://github.com/ExpressLRS/ExpressLRS/pull/3077
 */

#include "ardupilot_custom_telemetry.h"
#include "common/mavlink.h"

/*
 * Known Issues:
 * - Battery Capacity is currently not available via Mavlink BATTERY_INFO message is not implemented on Ardupilot side.
 *   This can be worked around by setting the battery capacity via Yaapu Config option.
 * - Distance to Home and Bearing to Home are highly volatile until armed due to updates
 *   of both HOME Position and Current Position with low GPS accuracy. Ardupilot stops sending updates of HOME Position when armed.
 */

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

#define NUMBER_OF_CORDIC_ITERATIONS (6U)
/**
 * static lookup for the angle corresponding tan(2^-i) * 1024
 * essentially the degree that corresponds to a tan(degree) = [1, 0.5, 0.25, 0.125 0.0625, ...]
 * scaled by 1024 to swap a (x/1000) with (x >> 10)
 */
static const uint16_t lookup_theta[NUMBER_OF_CORDIC_ITERATIONS] = {46080, 27203, 14373, 7296, 3662, 1832};

/**
 * COS(degree) of phi [0.7071067811865476, 0.8944271909999159, 0.9701425001453319, 0.9922778767136676, 0.9980525784828885, 0.9995120760870788]
 * product of all factors 0.607351770141296
 */


/**
 * Calc the difference in longitude scaled to degE7 as int32_t without using more then 32bit.
 */
int32_t diff_longitude(int32_t lon1, int32_t lon2)
{
    int32_t dlon = 0;
    if (((((uint32_t) lon1) ^ ((uint32_t) lon2)) & 0x80000000U) > 0U){
        // special case, unequal signs
        // 1st find out if the closest distance is on atlantic or on pacific side
        uint32_t abs_lon = ((uint32_t) ((lon1 > 0) ? lon1 : -lon1)) + ((uint32_t) ((lon2 > 0) ? lon2 : -lon2));
        int8_t sign = (lon1 < 0) ? -1 : 1;
        if (abs_lon > 1800000000U){
            // closest distance is on pacific side
            abs_lon = 3600000000U-abs_lon;
            sign *= -1;
        }
        dlon = ((int32_t)abs_lon) * sign;
    } else {
        // simple case, subtract lon1-lon2
        dlon = lon1 - lon2;
    }
    return dlon;
}

/**
 * Cordic Algo to calculate the cosine of latitude angle and scale down the longitude.
 * The distance of longitude (West to East) direction depends on latitude, i.e.
 * at the equator, the West to East distance is full while towards the poles, it converges to 0.
 */
int32_t scale_longitude(int32_t latitude, int32_t dlon){
    uint32_t dlon_calc = 0U;
    int32_t dlon_scaled = 0;
    int32_t x = 64; // we aim for a correction factor accuracy of ~1% ~= 1/64 = 1.5625% error
    int32_t y = 0;
    int32_t theta = 0;

    // Approximation of target_theta x*1024 / 10'000'000 = x*0.0001024 = (x/2097152)*215, 0.06% error
    uint32_t calc_latitude = 0U;
    if (latitude < 0){
        calc_latitude = (uint32_t)(-latitude);
    } else {
        calc_latitude = (uint32_t)(latitude);
    }
    const int32_t target_theta = (int32_t)((calc_latitude >> 21) * 215);

    int32_t nx = 0;
    int32_t ny = 0;

    for (uint8_t index=0; index < NUMBER_OF_CORDIC_ITERATIONS; index++){
        if (theta < target_theta){
            //Note: x and y are guaranteed to be positive here
            nx = x - (y >> index);
            ny = y + (x >> index);
            theta += lookup_theta[index];
        } else {
            nx = x + (y >> index);
            ny = y - (x >> index);
            theta -= lookup_theta[index];
        }
        x = nx;
        y = ny;
    }
    // correct for K 0.607351770141296 ~= 39/64
    x = (x * 39) >> 6;
    if (dlon < 0){
        dlon_calc = (uint32_t)(-dlon);

        // this is a complicated way of saying, we can handle up to 4*90degE7 in uint32_t,
        // so divide by 16, multiply with something that can be 64 and then divide by 4 again.
        dlon_calc = ((dlon_calc >> 4) * x) >> 2;

        dlon_scaled = -((int32_t) dlon_calc);
    } else {
        dlon_calc = (uint32_t)(dlon);

        // this is a complicated way of saying, we can handle up to 4*90degE7 in uint32_t,
        // so divide by 16, multiply with something that can be 64 and then divide by 4 again.
        dlon_calc = ((dlon_calc >> 4) * x) >> 2;

        dlon_scaled = ((int32_t) dlon_calc);
    }
    return dlon_scaled;
}


/**
 * Cordic Algo to transform cartesian coordinates into polar coordinates
 * Takes dlat and dlon as int32_t in degE7 scaling.
 * Writes corrected vector length and angle to output pointers.
 * We aim for meter accuracy and degree accuracy.
 */
void cartesian_to_polar_coordinates(int32_t dlat, int32_t dlon, uint32_t *out_phi_deg, uint32_t *out_radius_m)
{
    // loop values
    int32_t theta = 0;

    // Initial rotation North becomes x-axis and East becomes negative on y-axis
    int32_t x = dlat;
    int32_t y = -dlon;

    // latch values for x and y, because one depends on the other and cannot be calculated at the same time
    int32_t nx = 0;
    int32_t ny = 0;

    // the output values
    uint32_t phi = 0U;
    uint32_t radius = 0U;

    // quadrant solving by rotation of +/-90deg
    // in general, the angle grows clockwise, but we rotate the coordinate system counter clockwise for positive angle
    if (x < 0){ // if we're in the south direction or left (negative) side of coord system
        if (y < 0){
            // SE or bottom left quadrant, we need to rotate -90deg to get on the right (positive) side of the coordinate system
            // example we have a real heading of 120deg, we are more then 90deg, we rotate 90 deg ccw
            // and end up in the NE (right bottom quadrant), cordic will rotate more ccw and add up the remaining 30deg angle
            nx = -y;
            ny = x;
            theta = 90*1024; // 90deg ccw
        } else {
            // SW or top left quadrant, we need to rotate 90 deg to get on the right (positive) side of the coordinate system
            // example we have a real heading of 200deg, we are more then -90deg, we rotate 90deg cw
            // and end up in the NW (right top quadrant), cordic will rotate more cw and subtract the remaining 70deg angle
            nx = y;
            ny = -x;
            theta = -90*1024; // 90deg cw
        }
        x = nx;
        y = ny;
    }

    for (uint8_t index=0; index < NUMBER_OF_CORDIC_ITERATIONS; index++){
        if (y < 0){
            // x grows by taking bits of y, obviously the sign bit must not be part of the shift
            nx = x + ((-y) >> index);
            // y converges to 0 by giving bits to x
            ny = y + (x >> index);
            // positive angle change cw
            theta += lookup_theta[index];
        } else {
            // x grows by taking bits of y
            nx = x + (y >> index);
            // y converges to 0 by giving bits to x
            ny = y - (x >> index);
            // negative angle change ccw
            theta -= lookup_theta[index];
        }

        // at the end of the loop, latch out x and y
        x = nx;
        y = ny;
    }
    // In theory, theta can have a value between -180deg and +180deg coming out of the above loop.
    // If negative, we use the other angle to scale to 0-359
    if (theta < 0){
        theta += 360*1024;
    };
    phi = ((uint32_t) theta) >> 10; // final scale to deg

    // Vector Length Correction and Latitude/Longitude to Meter Conversion
    // Factor for COS Correction 0.607351770141296
    // Factor for LatLonDegE7 to M Conversion 0.011131884502145034
    // Combined 0.00676097 ~= 7/1024, 1.1% error
    radius = (uint32_t)((x < 0) ? -x : x);
    // this is a complicated way of saying, we can handle up to 2*180degE7 in uint32_t,
    // so divide by 4, multiply with 7 and then divide by 256 again.
    radius = ((radius >> 2) * 7) >> 8;
    *out_phi_deg = phi;
    *out_radius_m = radius;
    return;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort::prep_number()
 * This is a proprietary number format that must be known on a value basis between producer and consumer.
 */
uint16_t prep_number(int32_t number, uint8_t digits, uint8_t power)
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
uint32_t format_ap_status(uint8_t base_mode, uint32_t custom_mode, uint8_t system_status, uint16_t throttle)
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
uint32_t format_gps_status(uint8_t fix_type, int32_t alt_msl_mm, uint16_t eph, uint8_t satellites_visible)
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
    gps_status |= prep_number(MAX(0,alt_msl_mm / 100),2,2)<<GPS_ALTMSL_OFFSET;
    return gps_status;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_batt()
 * This is the content of the 0x5003 BATT1 value.
 * -1 on current values indicate invalid thus fields are kept 0
 */
uint32_t format_batt1(uint16_t voltage_mv, int16_t current_ca, int32_t current_consumed)
{
#define BATT_VOLTAGE_LIMIT          0x1FF
#define BATT_CURRENT_OFFSET         9
#define BATT_TOTALMAH_LIMIT         0x7FFF
#define BATT_TOTALMAH_OFFSET        17
    // battery voltage in decivolts, can have up to a 12S battery (4.25Vx12S = 51.0V)
    uint32_t batt = ((voltage_mv / 100) & BATT_VOLTAGE_LIMIT);
    if (current_ca > 0){
        // battery current draw in deciamps
        batt |= prep_number(current_ca / 10, 2, 1)<<BATT_CURRENT_OFFSET;
    }
    if (current_consumed > 0){
        // battery current drawn since power on in mAh (limit to 32767 (0x7FFF) since value is stored on 15 bits)
        batt |= ((current_consumed < BATT_TOTALMAH_LIMIT) ? (current_consumed & BATT_TOTALMAH_LIMIT) : BATT_TOTALMAH_LIMIT)<<BATT_TOTALMAH_OFFSET;
    }
    return batt;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_home()
 * This is the content of the 0x5004 HOME value.
 */
uint32_t format_home(uint32_t distance_to_home_m, uint32_t altitude_above_home_dm, uint32_t bearing_to_home_deg)
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
uint32_t format_velandyaw(float climb_mps, float airspeed_mps, float groundspeed_mps, int16_t heading)
{
    static bool send_airspeed = false;
    float speed_mps = groundspeed_mps;
#define VELANDYAW_XYVEL_OFFSET      9
#define VELANDYAW_YAW_LIMIT         0x7FF
#define VELANDYAW_YAW_OFFSET        17
#define VELANDYAW_ARSPD_OFFSET      28
    // vertical velocity in dm/s
    uint32_t velandyaw = prep_number(climb_mps * 10, 2, 1);

    if (send_airspeed){
        speed_mps = airspeed_mps;
        velandyaw |= 1U<<VELANDYAW_ARSPD_OFFSET;
    }

    // horizontal velocity in dm/s
    velandyaw |= prep_number(speed_mps * 10, 2, 1)<<VELANDYAW_XYVEL_OFFSET;

    // toggle the switch
    send_airspeed = !send_airspeed;

    // idiotic scaling from int to int*5
    velandyaw |= ((heading * 5) & VELANDYAW_YAW_LIMIT)<<VELANDYAW_YAW_OFFSET;
    return velandyaw;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_attiandrng()
 * This is the content of the 0x5006 Attitude and RangeFinder.
 * We don't provide Rangefinder here.
 */
uint32_t format_attiandrng(float pitch_rad, float roll_rad)
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
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_param()
 * This is a (8bit parameter id, 24 bit value)-tuple, content of 0x5007 Param.
 */
uint32_t format_param(uint8_t param_id, uint32_t param_value)
{
#define PARAM_ID_OFFSET             24
#define PARAM_VALUE_LIMIT           0xFFFFFF
    uint32_t param_data = ((param_id << PARAM_ID_OFFSET) | (param_value & PARAM_VALUE_LIMIT));
    return param_data;
}

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_terrain()
 * This is the content of the 0x500B terrain.
 */
uint32_t format_terrain(uint32_t altitude_terrain)
{
    uint32_t value = prep_number(altitude_terrain * 10, 3, 2);
    return value;
}


/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_waypoint()
 * This is the content of the 0x500B terrain.
 */
uint32_t format_waypoint(uint8_t heading, uint16_t distance, uint16_t number)
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

