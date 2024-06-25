//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
//*******************************************************
// ArduPilot Protocol Stuff
//*******************************************************
#pragma once
#ifndef ARDUPILOT_PROTOCOL_H
#define ARDUPILOT_PROTOCOL_H


//-------------------------------------------------------
// ArduPilot flight modes
//-------------------------------------------------------

// this we got from ArduCopter/mode.h
typedef enum {
    AP_COPTER_FLIGHTMODE_STABILIZE =     0,  // manual airframe angle with manual throttle
    AP_COPTER_FLIGHTMODE_ACRO =          1,  // manual body-frame angular rate with manual throttle
    AP_COPTER_FLIGHTMODE_ALT_HOLD =      2,  // manual airframe angle with automatic throttle
    AP_COPTER_FLIGHTMODE_AUTO =          3,  // fully automatic waypoint control using mission commands
    AP_COPTER_FLIGHTMODE_GUIDED =        4,  // fully automatic fly to coordinate or fly at velocity/direction using GCS immediate commands
    AP_COPTER_FLIGHTMODE_LOITER =        5,  // automatic horizontal acceleration with automatic throttle
    AP_COPTER_FLIGHTMODE_RTL =           6,  // automatic return to launching point
    AP_COPTER_FLIGHTMODE_CIRCLE =        7,  // automatic circular flight with automatic throttle
    AP_COPTER_FLIGHTMODE_LAND =          9,  // automatic landing with horizontal position control
    AP_COPTER_FLIGHTMODE_DRIFT =        11,  // semi-autonomous position, yaw and throttle control
    AP_COPTER_FLIGHTMODE_SPORT =        13,  // manual earth-frame angular rate control with manual throttle
    AP_COPTER_FLIGHTMODE_FLIP =         14,  // automatically flip the vehicle on the roll axis
    AP_COPTER_FLIGHTMODE_AUTOTUNE =     15,  // automatically tune the vehicle's roll and pitch gains
    AP_COPTER_FLIGHTMODE_POSHOLD =      16,  // automatic position hold with manual override, with automatic throttle
    AP_COPTER_FLIGHTMODE_BRAKE =        17,  // full-brake using inertial/GPS system, no pilot input
    AP_COPTER_FLIGHTMODE_THROW =        18,  // throw to launch mode using inertial/GPS system, no pilot input
    AP_COPTER_FLIGHTMODE_AVOID_ADSB =   19,  // automatic avoidance of obstacles in the macro scale - e.g. full-sized aircraft
    AP_COPTER_FLIGHTMODE_GUIDED_NOGPS = 20,  // guided mode but only accepts attitude and altitude
    AP_COPTER_FLIGHTMODE_SMART_RTL =    21,  // SMART_RTL returns to home by retracing its steps
    AP_COPTER_FLIGHTMODE_FLOWHOLD  =    22,  // FLOWHOLD holds position with optical flow without rangefinder
    AP_COPTER_FLIGHTMODE_FOLLOW    =    23,  // follow attempts to follow another vehicle or ground station
    AP_COPTER_FLIGHTMODE_ZIGZAG    =    24,  // ZIGZAG mode is able to fly in a zigzag manner with predefined point A and point B
    AP_COPTER_FLIGHTMODE_SYSTEMID  =    25,  // System ID mode produces automated system identification signals in the controllers
    AP_COPTER_FLIGHTMODE_AUTOROTATE =   26,  // Autonomous autorotation
    AP_COPTER_FLIGHTMODE_AUTO_RTL =     27,  // Auto RTL, this is not a true mode, AUTO will report as this mode if entered to perform a DO_LAND_START Landing sequence
    AP_COPTER_FLIGHTMODE_TURTLE =       28,  // Flip over after crash
} AP_COPTER_FLIGHTMODE_ENUM;


// this we got from ArduPlane/mode.h
typedef enum {
    AP_PLANE_FLIGHTMODE_MANUAL        = 0,
    AP_PLANE_FLIGHTMODE_CIRCLE        = 1,
    AP_PLANE_FLIGHTMODE_STABILIZE     = 2,
    AP_PLANE_FLIGHTMODE_TRAINING      = 3,
    AP_PLANE_FLIGHTMODE_ACRO          = 4,
    AP_PLANE_FLIGHTMODE_FLY_BY_WIRE_A = 5,
    AP_PLANE_FLIGHTMODE_FLY_BY_WIRE_B = 6,
    AP_PLANE_FLIGHTMODE_CRUISE        = 7,
    AP_PLANE_FLIGHTMODE_AUTOTUNE      = 8,
    AP_PLANE_FLIGHTMODE_AUTO          = 10,
    AP_PLANE_FLIGHTMODE_RTL           = 11,
    AP_PLANE_FLIGHTMODE_LOITER        = 12,
    AP_PLANE_FLIGHTMODE_TAKEOFF       = 13,
    AP_PLANE_FLIGHTMODE_AVOID_ADSB    = 14,
    AP_PLANE_FLIGHTMODE_GUIDED        = 15,
    AP_PLANE_FLIGHTMODE_INITIALISING  = 16,
    AP_PLANE_FLIGHTMODE_QSTABILIZE    = 17,
    AP_PLANE_FLIGHTMODE_QHOVER        = 18,
    AP_PLANE_FLIGHTMODE_QLOITER       = 19,
    AP_PLANE_FLIGHTMODE_QLAND         = 20,
    AP_PLANE_FLIGHTMODE_QRTL          = 21,
    AP_PLANE_FLIGHTMODE_QAUTOTUNE     = 22,
    AP_PLANE_FLIGHTMODE_QACRO         = 23,
    AP_PLANE_FLIGHTMODE_THERMAL       = 24,
    AP_PLANE_FLIGHTMODE_LOITER_ALT_QLAND = 25,
} AP_PLANE_FLIGHTMODE_ENUM;


// this we got from Rover/mode.h
typedef enum {
    AP_ROVER_FLIGHTMODE_MANUAL       = 0,
    AP_ROVER_FLIGHTMODE_ACRO         = 1,
    AP_ROVER_FLIGHTMODE_STEERING     = 3,
    AP_ROVER_FLIGHTMODE_HOLD         = 4,
    AP_ROVER_FLIGHTMODE_LOITER       = 5,
    AP_ROVER_FLIGHTMODE_FOLLOW       = 6,
    AP_ROVER_FLIGHTMODE_SIMPLE       = 7,
    AP_ROVER_FLIGHTMODE_DOCK         = 8,
    AP_ROVER_FLIGHTMODE_AUTO         = 10,
    AP_ROVER_FLIGHTMODE_RTL          = 11,
    AP_ROVER_FLIGHTMODE_SMART_RTL    = 12,
    AP_ROVER_FLIGHTMODE_GUIDED       = 15,
    AP_ROVER_FLIGHTMODE_INITIALISING = 16,
} AP_ROVER_FLIGHTMODE_ENUM;


// this we got from ArduSub/defines.h
typedef enum {
    AP_SUB_FLIGHTMODE_STABILIZE =     0,  // manual angle with manual depth/throttle
    AP_SUB_FLIGHTMODE_ACRO =          1,  // manual body-frame angular rate with manual depth/throttle
    AP_SUB_FLIGHTMODE_ALT_HOLD =      2,  // manual angle with automatic depth/throttle
    AP_SUB_FLIGHTMODE_AUTO =          3,  // fully automatic waypoint control using mission commands
    AP_SUB_FLIGHTMODE_GUIDED =        4,  // fully automatic fly to coordinate or fly at velocity/direction using GCS immediate commands
    AP_SUB_FLIGHTMODE_CIRCLE =        7,  // automatic circular flight with automatic throttle
    AP_SUB_FLIGHTMODE_SURFACE =       9,  // automatically return to surface, pilot maintains horizontal control
    AP_SUB_FLIGHTMODE_POSHOLD =      16,  // automatic position hold with manual override, with automatic throttle
    AP_SUB_FLIGHTMODE_MANUAL =       19,  // Pass-through input with no stabilization
    AP_SUB_FLIGHTMODE_MOTOR_DETECT = 20   // Automatically detect motors orientation
} AP_SUB_FLIGHTMODE_ENUM;


// these we find in ArduCopter/mode.h
void ap_copter_flight_mode_name4(char* name4, uint8_t flight_mode)
{
    switch (flight_mode) {
    case AP_COPTER_FLIGHTMODE_STABILIZE:    strcpy(name4, "STAB"); break;
    case AP_COPTER_FLIGHTMODE_ACRO:         strcpy(name4, "ACRO"); break;
    case AP_COPTER_FLIGHTMODE_ALT_HOLD:     strcpy(name4, "ALTH"); break;
    case AP_COPTER_FLIGHTMODE_AUTO:         strcpy(name4, "AUTO"); break; // can be also "ARTL"
    case AP_COPTER_FLIGHTMODE_GUIDED:       strcpy(name4, "GUID"); break;
    case AP_COPTER_FLIGHTMODE_LOITER:       strcpy(name4, "LOIT"); break;
    case AP_COPTER_FLIGHTMODE_RTL:          strcpy(name4, "RTL "); break;
    case AP_COPTER_FLIGHTMODE_CIRCLE:       strcpy(name4, "CIRC"); break;
    case AP_COPTER_FLIGHTMODE_LAND:         strcpy(name4, "LAND"); break;
    case AP_COPTER_FLIGHTMODE_DRIFT:        strcpy(name4, "DRIF"); break;
    case AP_COPTER_FLIGHTMODE_SPORT:        strcpy(name4, "SPRT"); break;
    case AP_COPTER_FLIGHTMODE_FLIP:         strcpy(name4, "FLIP"); break;
    case AP_COPTER_FLIGHTMODE_AUTOTUNE:     strcpy(name4, "ATUN"); break;
    case AP_COPTER_FLIGHTMODE_POSHOLD:      strcpy(name4, "PHLD"); break;
    case AP_COPTER_FLIGHTMODE_BRAKE:        strcpy(name4, "BRAK"); break;
    case AP_COPTER_FLIGHTMODE_THROW:        strcpy(name4, "THRW"); break;
    case AP_COPTER_FLIGHTMODE_AVOID_ADSB:   strcpy(name4, "AVOI"); break;
    case AP_COPTER_FLIGHTMODE_GUIDED_NOGPS: strcpy(name4, "GNGP"); break;
    case AP_COPTER_FLIGHTMODE_SMART_RTL:    strcpy(name4, "SRTL"); break;
    case AP_COPTER_FLIGHTMODE_FLOWHOLD:     strcpy(name4, "FHLD"); break;
    case AP_COPTER_FLIGHTMODE_FOLLOW:       strcpy(name4, "FOLL"); break;
    case AP_COPTER_FLIGHTMODE_ZIGZAG:       strcpy(name4, "ZIGZ"); break;
    case AP_COPTER_FLIGHTMODE_SYSTEMID:     strcpy(name4, "SYSI"); break;
    case AP_COPTER_FLIGHTMODE_AUTOROTATE:   strcpy(name4, "AROT"); break;
    case AP_COPTER_FLIGHTMODE_AUTO_RTL:     strcpy(name4, "ARTL"); break;
    case AP_COPTER_FLIGHTMODE_TURTLE:       strcpy(name4, "TRTL"); break;
    default:
        strcpy(name4, "");
    }
}


// these we find in ArduPlane/mode.h
void ap_plane_flight_mode_name4(char* name4, uint8_t flight_mode)
{
    switch (flight_mode) {
    case AP_PLANE_FLIGHTMODE_MANUAL:        strcpy(name4, "MANU"); break;
    case AP_PLANE_FLIGHTMODE_CIRCLE:        strcpy(name4, "CIRC"); break;
    case AP_PLANE_FLIGHTMODE_STABILIZE:     strcpy(name4, "STAB"); break;
    case AP_PLANE_FLIGHTMODE_TRAINING:      strcpy(name4, "TRAN"); break;
    case AP_PLANE_FLIGHTMODE_ACRO:          strcpy(name4, "ACRO"); break;
    case AP_PLANE_FLIGHTMODE_FLY_BY_WIRE_A: strcpy(name4, "FBWA"); break;
    case AP_PLANE_FLIGHTMODE_FLY_BY_WIRE_B: strcpy(name4, "FBWB"); break;
    case AP_PLANE_FLIGHTMODE_CRUISE:        strcpy(name4, "CRUS"); break;
    case AP_PLANE_FLIGHTMODE_AUTOTUNE:      strcpy(name4, "ATUN"); break;
    case AP_PLANE_FLIGHTMODE_AUTO:          strcpy(name4, "AUTO"); break;
    case AP_PLANE_FLIGHTMODE_RTL:           strcpy(name4, "RTL "); break;
    case AP_PLANE_FLIGHTMODE_LOITER:        strcpy(name4, "LOIT"); break;
    case AP_PLANE_FLIGHTMODE_TAKEOFF:       strcpy(name4, "TKOF"); break;
    case AP_PLANE_FLIGHTMODE_AVOID_ADSB:    strcpy(name4, "AVOI"); break;
    case AP_PLANE_FLIGHTMODE_GUIDED:        strcpy(name4, "GUID"); break;
    case AP_PLANE_FLIGHTMODE_INITIALISING:  strcpy(name4, "INIT"); break;
    case AP_PLANE_FLIGHTMODE_QSTABILIZE:    strcpy(name4, "QSTB"); break;
    case AP_PLANE_FLIGHTMODE_QHOVER:        strcpy(name4, "QHOV"); break;
    case AP_PLANE_FLIGHTMODE_QLOITER:       strcpy(name4, "QLOT"); break;
    case AP_PLANE_FLIGHTMODE_QLAND:         strcpy(name4, "QLND"); break;
    case AP_PLANE_FLIGHTMODE_QRTL:          strcpy(name4, "QRTL"); break;
    case AP_PLANE_FLIGHTMODE_QAUTOTUNE:     strcpy(name4, "QATN"); break;
    case AP_PLANE_FLIGHTMODE_QACRO:         strcpy(name4, "QACO"); break;
    case AP_PLANE_FLIGHTMODE_THERMAL:       strcpy(name4, "THML"); break;
    case AP_PLANE_FLIGHTMODE_LOITER_ALT_QLAND: strcpy(name4, "L2QL"); break;
    default:
        strcpy(name4, "");
    }
}


// these we find in Rover/mode.h
void ap_rover_flight_mode_name4(char* name4, uint8_t flight_mode)
{
    switch (flight_mode) {
    case AP_ROVER_FLIGHTMODE_MANUAL:        strcpy(name4, "MANU"); break;
    case AP_ROVER_FLIGHTMODE_ACRO:          strcpy(name4, "ACRO"); break;
    case AP_ROVER_FLIGHTMODE_STEERING:      strcpy(name4, "STER"); break;
    case AP_ROVER_FLIGHTMODE_HOLD:          strcpy(name4, "HOLD"); break;
    case AP_ROVER_FLIGHTMODE_LOITER:        strcpy(name4, "LOIT"); break;
    case AP_ROVER_FLIGHTMODE_FOLLOW:        strcpy(name4, "FOLL"); break;
    case AP_ROVER_FLIGHTMODE_SIMPLE:        strcpy(name4, "SMPL"); break;
    case AP_ROVER_FLIGHTMODE_DOCK:          strcpy(name4, "DOCK"); break;
    case AP_ROVER_FLIGHTMODE_AUTO:          strcpy(name4, "AUTO"); break;
    case AP_ROVER_FLIGHTMODE_RTL:           strcpy(name4, "RTL "); break;
    case AP_ROVER_FLIGHTMODE_SMART_RTL:     strcpy(name4, "SRTL"); break;
    case AP_ROVER_FLIGHTMODE_GUIDED:        strcpy(name4, "GUID"); break;
    case AP_ROVER_FLIGHTMODE_INITIALISING:  strcpy(name4, "INIT"); break;
    default:
        strcpy(name4, "");
    }
}


// there are these? made them up myself
void ap_sub_flight_mode_name4(char* name4, uint8_t flight_mode)
{
    switch (flight_mode) {
    case AP_SUB_FLIGHTMODE_STABILIZE:       strcpy(name4, "STAB"); break;
    case AP_SUB_FLIGHTMODE_ACRO:            strcpy(name4, "ACRO"); break;
    case AP_SUB_FLIGHTMODE_ALT_HOLD:        strcpy(name4, "AHLD"); break;
    case AP_SUB_FLIGHTMODE_AUTO:            strcpy(name4, "AUTO"); break;
    case AP_SUB_FLIGHTMODE_GUIDED:          strcpy(name4, "GUID"); break;
    case AP_SUB_FLIGHTMODE_CIRCLE:          strcpy(name4, "CIRC"); break;
    case AP_SUB_FLIGHTMODE_SURFACE:         strcpy(name4, "SURF"); break; // ?
    case AP_SUB_FLIGHTMODE_POSHOLD:         strcpy(name4, "PHLD"); break;
    case AP_SUB_FLIGHTMODE_MANUAL:          strcpy(name4, "MANU"); break;
    case AP_SUB_FLIGHTMODE_MOTOR_DETECT:    strcpy(name4, "MDET"); break; // ?
    default:
        strcpy(name4, "");
    }
}


//-------------------------------------------------------
// ArduPilot vehicles
//-------------------------------------------------------

typedef enum {
    AP_VEHICLE_GENERIC = 0,
    AP_VEHICLE_COPTER,
    AP_VEHICLE_PLANE,
    AP_VEHICLE_ROVER,
    AP_VEHICLE_SUB,
} AP_VEHICLE_ENUM;


// this we deduce by searching for MAV_TYPE in ArduPilot code
uint8_t ap_vehicle_from_mavtype(uint8_t mav_type)
{
    switch (mav_type) {
    case MAV_TYPE_COAXIAL:
    case MAV_TYPE_HELICOPTER:
    case MAV_TYPE_TRICOPTER:
    case MAV_TYPE_QUADROTOR:
    case MAV_TYPE_HEXAROTOR:
    case MAV_TYPE_OCTOROTOR:
    case MAV_TYPE_DECAROTOR:
    case MAV_TYPE_DODECAROTOR:
        return AP_VEHICLE_COPTER;

    case MAV_TYPE_SURFACE_BOAT:
    case MAV_TYPE_GROUND_ROVER:
        return AP_VEHICLE_ROVER;

    case MAV_TYPE_SUBMARINE:
        return AP_VEHICLE_SUB;

    case MAV_TYPE_GENERIC:
        return AP_VEHICLE_GENERIC; // sadly we don't know nothing
    }

    return AP_VEHICLE_PLANE; // let's guess it's a plane
}


//-------------------------------------------------------
// more
//-------------------------------------------------------

void ap_flight_mode_name4(char* name4, uint8_t vehicle, uint8_t flight_mode)
{
    switch (vehicle) {
    case AP_VEHICLE_COPTER:
        ap_copter_flight_mode_name4(name4, flight_mode);
        return;
    case AP_VEHICLE_PLANE:
        ap_plane_flight_mode_name4(name4, flight_mode);
        return;
    case AP_VEHICLE_ROVER:
        ap_rover_flight_mode_name4(name4, flight_mode);
        return;
    case AP_VEHICLE_SUB:
        ap_sub_flight_mode_name4(name4, flight_mode);
        return;
    }

    strcpy(name4, "");
}

#endif // ARDUPILOT_PROTOCOL_H
