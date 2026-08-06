#pragma once

//#include "helper_3dmath.h"
#include "gyro_types.h"
#include "../drivers/imu_driver.h"
#include "biasFilter.h"

class AHRS
{
    public:
        unsigned long read_errors = 0;
        unsigned long int_errors = 0;
        
        float gyro_rpy[3];     // [roll, pitch, yaw] Gyro Deg/sec
        float acc_rpy[3];      // [roll, pitch, yaw] accelearion Gs (M/S2)
        float angle_rpy[3];    // [roll, pitch, yaw] angles

        bool initialize(IMU_Driver *driver);
        IMU_Driver* getImuDriver() { return driver; };
        
        void start();
        void pause();
        int  tick();
        uint8_t event();

        bool readAndUpdate();
       
        void calibrate(bool save);
        void OrientationHorizontalExecute();
        void OrientationVerticalExecute();
        bool isRunning();

    protected:
        IMU_Driver *driver;

        bool initialized = false;
        bool isCalibrating = false;

        BiasFilter gyroBias;
        unsigned long lastGyroUpdate_us = 0;
        float  AHRS_KP =  2.0;
        float  AHRS_KI =  0.0;

        // Orientation related variables
        bool orientationIsWrong;    // flag to say that orientation is wrong and so avoid any process of raw data
        uint8_t imuOrientationH=0;
        uint8_t imuOrientationV=0;

        uint8_t orientationX;       // contain the index 0,1, 2 of aRaw[] and gRaw[] to be moved in oax and ogx
        uint8_t orientationY;       // idem for oay and ogy
        uint8_t orientationZ;       // idem for oaz and ogz
        int8_t orientationSignX;    // contains the sign (1 or -1 ) to apply to oax and ogx
        int8_t orientationSignY;    // idem for oay and ogy
        int8_t orientationSignZ;    // idem for oaz and ogz

        rx_config_gyro_calibration_t calGyroOffsets, calAccelOffets;

        Quaternion  q = Quaternion();        // [w, x, y, z]         quaternion container
        
        VectorFloat gravity; // [x, y, z]  gravity vector    
        VectorInt16 v_gyro, v_accel;

        void findGravity(int32_t ax, int32_t ay, int32_t az, uint8_t &idx);
        uint8_t readAndGetGravity();

        void setupOrientation();
        void applyOrientation(VectorInt16 *v);

        boolean isAccelHeathty(VectorFloat acc, float *kp, float *ki);
        void Mahony_update(bool useAcc, 
                           float ax, float ay, float az, 
                             float gx, float gy, float gz, 
                             Quaternion *q,
                             float deltat, float kp, float ki);

        bool calibrateGyro(int8_t loops, rx_config_gyro_calibration_t *offsets);
        bool calibrateAccel(int8_t loops, rx_config_gyro_calibration_t *offsets);

        #ifdef DEBUG_GYRO_STATS
        void printGyroStats(long nowMicros);
        #endif
        
};