/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * lib/MAVLink/ardupilot_custom_telemetry.h
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

#include <stdint.h>

/**
 * Calc the difference in longitude scaled to degE7 as int32_t without using more then 32bit.
 */
int32_t diff_longitude(int32_t lon1, int32_t lon2);

/**
 * Cordic Algo to calculate the cosine of latitude angle and scale down the longitude.
 * The distance of longitude (West to East) direction depends on latitude, i.e.
 * at the equator, the West to East distance is full while towards the poles, it converges to 0.
 */
int32_t scale_longitude(int32_t latitude, int32_t dlon);

/**
 * Cordic Algo to transform cartesian coordinates into polar coordinates
 * Takes dlat and dlon as int32_t in degE7 scaling.
 * Writes corrected vector length and angle to output pointers.
 * We aim for meter accuracy and degree accuracy.
 */
void cartesian_to_polar_coordinates(int32_t dlat, int32_t dlon, uint32_t *out_phi_deg, uint32_t *out_radius_m);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort::prep_number()
 * This is a proprietary number format that must be known on a value basis between producer and consumer.
 */
uint16_t prep_number(int32_t number, uint8_t digits, uint8_t power);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_ap_status()
 * This is the content of the 0x5001 AP_STATUS value.
 * Note that we don't have certain values via Mavlink.
 * - IMU Temperature
 * - simple/super simple mode flags
 * - specific failsafe flags
 * - fence flags
 */
uint32_t format_ap_status(uint8_t base_mode, uint32_t custom_mode, uint8_t system_status, uint16_t throttle);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_gps_status()
 * This is the content of the 0x5002 GPS_STATUS value.
 */
uint32_t format_gps_status(uint8_t fix_type, int32_t alt_msl_mm, uint16_t eph, uint8_t satellites_visible);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_batt()
 * This is the content of the 0x5003 BATT1 value.
 * -1 on current values indicate invalid thus fields are kept 0
 */
uint32_t format_batt1(uint16_t voltage_mv, int16_t current_ca, int32_t current_consumed);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_home()
 * This is the content of the 0x5004 HOME value.
 */
uint32_t format_home(uint32_t distance_to_home_m, uint32_t altitude_above_home_dm, uint32_t bearing_to_home_deg);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_velandyaw()
 * This is the content of the 0x5005 Velocity and Yaw.
 */
uint32_t format_velandyaw(float climb_mps, float airspeed_mps, float groundspeed_mps, int16_t heading);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_attiandrng()
 * This is the content of the 0x5006 Attitude and RangeFinder.
 * We don't provide Rangefinder here.
 */
uint32_t format_attiandrng(float pitch_rad, float roll_rad);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_param()
 * This is a (8bit parameter id, 24 bit value)-tuple, content of 0x5007 Param.
 */
uint32_t format_param(uint8_t param_id, uint32_t param_value);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_terrain()
 * This is the content of the 0x500B terrain.
 */
uint32_t format_terrain(uint32_t altitude_terrain);

/*
 * Adapted from Ardupilot's AP_Frsky_SPort_Passthrough::calc_waypoint()
 * This is the content of the 0x500B terrain.
 */
uint32_t format_waypoint(uint8_t heading, uint16_t distance, uint16_t number);

