#if defined(TARGET_RX) && (defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32))

#include "SerialHoTT_TLM.h"
#include "telemetry.h"
#include "FIFO.h"

#define HOTT_MAX_BUF_LEN    64      // max buffer size for serial in data

#define HOTT_POLL_RATE      150     // default HoTT bus poll rate [ms]
#define HOTT_LEAD_OUT       20      // minimum gap between end of payload to next poll

#define DISCOVERY_TIMEOUT   30000   // 30s device discovery time

#define VARIO_MIN_CRSFRATE  1000    // CRSF telemetry packets will be sent if
#define GPS_MIN_CRSFRATE    5000    // min rate timers in [ms] have expired
#define BATT_MIN_CRSFRATE   5000    // or packet value has changed. Fastest to
                                    // be expected update rate will by about 150ms due
                                    // to HoTT bus speed if only a HoTT Vario is connected and
                                    // values change every HoTT bus poll cycle.

#define FRAME_SIZE          45      // HoTT telemetry frame size
#define CMD_LEN             2       // HoTT poll command length
#define DEVICE_INDEX        1       // index of device ID    
#define CRC_INDEX           44      // index of CRC  

#define START_FRAME_B   	0x7C  	// HoTT start of frame marker
#define END_FRAME			0x7D  	// HoTT end of frame marker

#define START_OF_CMD_B		0x80  	// start byte of HoTT binary cmd sequence
#define SENSOR_ID_GPS_B 	0x8A  	// device ID binary mode GPS module
#define SENSOR_ID_GPS_T		0xA0  	// device ID for text mode adressing
#define SENSOR_ID_GAM_B	 	0x8D  	// device ID binary mode GAM module
#define SENSOR_ID_GAM_T		0xD0  	// device ID for text mode adressing
#define SENSOR_ID_EAM_B  	0x8E	// device ID binary mode EAM module
#define SENSOR_ID_EAM_T  	0xE0	// device ID for text mode adressing
#define SENSOR_ID_ESC_B 	0x8C	// device ID binary mode ESC module
#define SENSOR_ID_ESC_T  	0xC0	// device ID for text mode adressing
#define SENSOR_ID_VARIO_B 	0x89	// device ID binary mode VARIO module
#define SENSOR_ID_VARIO_T  	0x90	// device ID for text mode adressing

//
// GAM data frame data structure
//
typedef struct {
	uint8_t StartByte 		= START_FRAME_B;      	//  0 0x7C
	uint8_t Packet_ID		= SENSOR_ID_GAM_B;      //  1 0x8D HOTT_General_AIR_PACKET_ID
	uint8_t WarnBeep 		= 0;       				//  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
	uint8_t Packet_ID_text 	= SENSOR_ID_GAM_T; 		//  3 0xD0 Sensor ID text mode
	uint8_t Inverse_1		= 0;					//  4 Alarm_inverse_1
	uint8_t Inverse_2 		= 0;      				//  5 Alarm_inverse_2
	uint8_t VoltageCell1 	= 0;					//  6 124 = 2,48V (voltage*50)
	uint8_t VoltageCell2 	= 0; 					//  7 132 = 2,64V
	uint8_t VoltageCell3 	= 0; 					//  8
	uint8_t VoltageCell4 	= 0;   					//  9
	uint8_t VoltageCell5 	= 0;   					// 10
	uint8_t VoltageCell6 	= 0;   					// 11
	uint16_t Battery1 		= 0;      				// 12 51 = 5,1V Batt1 voltage
	uint16_t Battery2 		= 0;      				// 14 51 = 5,1V Batt2 voltage
	uint8_t Temperature1 	= 20;   				// 16 46 = 26C, 0 = -20C (temp+20)
	uint8_t Temperature2 	= 20;   				// 17 45 = 25C, 0 = -20C (temp+20)
	uint8_t fuel_scale 		= 0; 					// 18 0..100 (scales to display 0/25/50/75/100 increments)
	uint16_t fuel 			= 0;					// 19 65535 = full
	uint16_t rpm 			= 0;					// 21 (rpm/10)
	uint16_t Altitude 		= 500;      			// 23 500 = 0m (altitude+500)
	uint16_t m_per_sec 		= 30000;				// 25 1 = 0,01m/s 30000 = 0m/s (climb rate/100 + 30000)
	uint8_t m_per_3sec 		= 120;     				// 27 120 = 0m per 3 sek
	uint16_t Current 		= 0;       				// 28 1 = 0.1A (current*10)
	uint16_t InputVoltage 	= 0;  					// 30 166 = 16,6V (voltage*10)
	uint16_t Capacity 		= 0;      				// 32 1 = 10mAh (capacity in mAh/10)
	uint16_t Speed 			= 0;         			// 34 1 = 2km/h (speed/2)
	uint8_t lowcellvoltage 	= 0; 					// 36 124 = 2,48V (voltage*50)
	uint8_t cellnumberlcv 	= 0;  					// 37
	uint16_t second_rpm 	= 0;    				// 38 (rpm/10)
	uint8_t general_error	= 0;    		        // 40 general error number (voice error = 12)
	uint8_t pressure 		= 0;       				// 41 pressure 20 = 2bar (pressure*10)
	uint8_t Version 		= 0;        			// 42 version number
	uint8_t EndByte 		= END_FRAME;        	// 43 0x7D
	uint8_t Crc 			= 0x00;            		// 44 CRC
} PACKED GeneralAirPacket_t; 

//
// GPS data frame data structure
//
typedef struct {
	uint8_t  StartByte 		= START_FRAME_B;      	//  0 0x7C
	uint8_t  Packet_ID 		= SENSOR_ID_GPS_B;      //  1 0x8A HOTT_GPS_PACKET_ID
	uint8_t  WarnBeep 		= 0;       				//  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
	uint8_t  Packet_ID_text = SENSOR_ID_GPS_T; 		//  3 0xA0 Sensor ID text mode
	uint8_t  alarmInverse 	= 0;					//  4 0 Inverse status
	uint8_t  GPSInverse		= 1;					//  5 1 = no GPS signal
	uint8_t  Direction		= 0;					//  6 1 = 2 degrees; 0 = N, 90 = E, 180 = S, 270 = W
	uint16_t Speed			= 0;					//  7 1 = 1 km/h
	uint8_t  Lat_NS			= 0;					//  9 example: N48D39'0988'', 0 = N 
	uint16_t Lat_DegMin		= 0;					// 10 48D39' = 4839 = 0x12e7
	uint16_t Lat_Sec    	= 0;					// 12 0988'' = 988 = 0x03DC
	uint8_t  Lon_EW			= 0;					// 14 example: E09D25'9360'', 0 = E 
	uint16_t Lon_DegMin		= 0;					// 15 09D25' = 0925 = 0x039D
	uint16_t Lon_Sec    	= 0;					// 17 9360'' = 9360 = 0x2490
	uint16_t Distance		= 0;					// 19 1 = 1m
	uint16_t Altitude 		= 500;					// 21 500 = 0m
	uint16_t m_per_sec 		= 30000;				// 23 30000 = 0.00m/s (1 = 0.01m/s)
	uint8_t  m_per_3sec 	= 120;				    // 25 120 = 0m/3s (1 = 1m/3s)
	uint8_t  Satellites 	= 0;					// 26 n visible satellites
	uint8_t  FixChar 		= ' ';					// 27 GPS fix character. Display if DGPS, 2D oder 3D
	uint8_t  HomeDir 		= 0;					// 28 GPS home direction 1 = 2 degreed
	int8_t   Roll 			= 0;					// 29 signed roll angle 1 = 2 degrees
	int8_t   Pitch			= 0;					// 30 signed pitch angle 1 = 2 degrees
	int8_t   Yaw			= 0;					// 31 signed yaw angle 1 = 2 degrees
	uint8_t  TimeHours		= 0;					// 32 GPS time hours
	uint8_t  TimeMinutes	= 0;					// 33 GPS time minutes	
	uint8_t  TimeSeconds	= 0;					// 34 GPS time seconds
	uint8_t  TimeHundreds	= 0;					// 35 GPS time 1/100 seconds
	uint16_t MSLAltitude 	= 0;					// 36 1 = 1m
	uint8_t  Vibrations		= 0;					// 38 vibration level in %
	uint8_t  ASCII1			= '-';					// 39 free ASCII character 1
	uint8_t  ASCII2			= ' ';					// 40 free ASCII character 2
	uint8_t  ASCII3			= '-';					// 41 free ASCII character 3
	uint8_t  Version 		= 0;        			// 42 version number
	uint8_t  EndByte 		= END_FRAME;        	// 43 0x7D
	uint8_t  Crc 			= 0x00;            		// 44 CRC
} PACKED GPSPacket_t;

//
// EAM data frame data structure
//
typedef struct {
	uint8_t StartByte 		= START_FRAME_B;        //  0 0x7C
	uint8_t Packet_ID		= SENSOR_ID_EAM_B;      //  1 0x8E HOTT_Electric_Air_ID
	uint8_t WarnBeep 		= 0;       				//  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
	uint8_t Packet_ID_text 	= SENSOR_ID_EAM_T; 		//  3 0xE0 Sensor ID text mode
    uint8_t alarm_invers1	= 0;                    //  4 alarm bitmask. Value is displayed inverted
                                                    	//Bit#  Alarm field
														// 0    mAh
														// 1    Battery 1
														// 2    Battery 2
														// 3    Temperature 1
														// 4    Temperature 2
														// 5    Altitude
														// 6    Current
														// 7    Main power voltage
    uint8_t alarm_invers2 	= 0;                    //  5 alarm bitmask. Value is displayed inverted
														//Bit#  Alarm Field
														// 0    m/s
														// 1    m/3s
														// 2    Altitude (duplicate?)
														// 3    m/s (duplicate?)
														// 4    m/3s (duplicate?)
														// 5    unknown/unused
														// 6    unknown/unused
														// 7    "ON" sign/text msg active
    uint8_t cell_L[7]		= { 0 };                // 06 cell 1 voltage lower value. 0.02V steps, 124=2.48V
    uint8_t cell_H[7]		= { 0 };                // 13 cell 1 voltage high value. 0.02V steps, 124=2.48V
    uint16_t batt1_voltage	= 0;                    // 20 battery 1 voltage lower value in 100mv steps, 50=5V. optionally cell8_L value 0.02V steps
    uint16_t batt2_voltage	= 0;                    // 22 battery 2 voltage lower value in 100mv steps, 50=5V. optionally cell8_H value. 0.02V steps
    uint8_t temp1 			= 20;                   // 24 Temperature sensor 1. 20=0�, 46=26� - offset of 20.
    uint8_t temp2 			= 20;                   // 25 temperature sensor 2
    uint16_t altitude 		= 500;                  // 26 Altitude lower value. unit: meters. Value of 500 = 0m
    uint16_t current		= 0;                    // 28 Current in 0.1 steps
    uint16_t main_voltage	= 0;                    // 30 Main power voltage (drive) in 0.1V steps
    uint16_t batt_cap		= 0;                    // 32 used battery capacity in 10mAh steps
    uint16_t climbrate		= 30000;                // 34 climb rate in 0.01m/s. Value of 30000 = 0.00 m/s
    uint8_t climbrate3s 	= 120;  	            // 36 climbrate in m/3sec. Value of 120 = 0m/3sec
    uint16_t rpm 			= 0;                   	// 37 RPM. Steps: 10 U/min  
    uint8_t electric_min 	= 0;                    // 39 Electric minutes. Time does start, when motor current is > 3 A
    uint8_t electric_sec 	= 0;                    // 40 Electric seconds.
    uint16_t speed 			= 0;                    // 41 speed in km/h. Steps 1km/h
    uint8_t EndByte 		= END_FRAME;        	// 43 0x7D
    uint8_t Crc;            	                    // 44 CRC
} PACKED ElectricAirPacket_t;

//
// ESC data frame data structure
//
typedef struct HOTT_AIRESC_MSG_s {
	uint8_t StartByte 		= START_FRAME_B;        //  0 0x7C
	uint8_t Packet_ID		= SENSOR_ID_ESC_B;      //  1 0x8C HOTT_General_AIR_PACKET_ID
	uint8_t WarnBeep 		= 0;       				//  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
	uint8_t Packet_ID_text 	= SENSOR_ID_ESC_T; 		//  3 0xC0 Sensor ID text mode
    uint8_t alarm_invers1  	= 0;       				//  4 TODO: more info
    uint8_t alarm_invers2  	= 0;       				//  5 TODO: more info
    uint16_t input_v     	= 0;       				//  6 Input voltage
    uint16_t input_v_min  	= 0;       				//  8 Input min. voltage
    uint16_t capacity		= 0;       				// 10 battery capacity in 10mAh steps
    uint8_t esc_temp       	= 0;       				// 12 ESC temperature
    uint8_t esc_max_temp   	= 0;       				// 13 ESC max. temperature
    uint16_t current     	= 0;       				// 14 Current in 0.1 steps
    uint16_t current_max 	= 0;       				// 16 Current max. in 0.1 steps
    uint16_t rpm			= 0;       				// 18 RPM in 10U/min steps
    uint16_t rpm_max		= 0;       				// 20 RPM max
    uint8_t motor_temp		= 0;       				// 22 motor temp
    uint8_t motor_temo_max	= 0;       				// 23 motor temp max
    uint16_t speed			= 0;       				// 24 Speed
    uint16_t speed_max		= 0;       				// 26 Speed max
    uint8_t pwm				= 0;       				// 28 PWM in %
    uint8_t throttle		= 0;       				// 29 throttle in %
    uint8_t bec_v			= 0;	       			// 30 BEC voltage
    uint8_t bec_v_min		= 0;       				// 31 BEC voltage min
    uint8_t bec_current		= 0;       				// 32 BEC current
    uint8_t bec_temp		= 0;       				// 34 BEC temperature
    uint8_t capacitor_temp	= 0;       				// 35 Capacitor temperature
    uint8_t motor_timing	= 0;       				// 36 motor timing
    uint8_t aux_temp		= 0;       				// 37 advanced timing/aux temp
    uint16_t thrust			= 0;       				// 38 thrust/force
    uint8_t pump_temp		= 0;       				// 40 pump temperature
    uint8_t turbine_number	= 0;       				// 41 engine number (200 = #1, 201 = #2)
    uint8_t version			= 3;       				// 42 version number = 3 for new protocol
    uint8_t EndByte 		= END_FRAME;        	// 43 0x7D
	uint8_t Crc;            	                    // 44 CRC
} PACKED AirESCPacket_t;

//
// VARIO data frame data structure
//
typedef struct HOTT_VARIO_MSG_s {
	uint8_t StartByte 		= START_FRAME_B;        //  0 0x7C
	uint8_t Packet_ID		= SENSOR_ID_VARIO_B;    //  1 0x89 HOTT_Vario_PACKET_ID
	uint8_t WarnBeep 		= 0;       				//  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
	uint8_t Packet_ID_text 	= SENSOR_ID_VARIO_T; 	//  3 0x90 Sensor ID text mode
    uint8_t alarm_invers1  	= 0;       				//  4 Inverse display (alarm?) bitmask
    uint16_t altitude		= 500;       		    //  5 Altitude low uint8_t. In meters. A value of 500 means 0m
    uint16_t altitude_max	= 500;       			//  7 Max. measured altitude low uint8_t. In meters. A value of 500 means 0m
    uint16_t altitude_min	= 500;       			//  9 Min. measured altitude low uint8_t. In meters. A value of 500 means 0m
    uint16_t climbrate		= 30000;       			// 11 Climb rate in m/s. Steps of 0.01m/s. Value of 30000 = 0.00 m/s
    uint16_t climbrate3s	= 30000;       			// 13 Climb rate in m/3s. Steps of 0.01m/3s. Value of 30000 = 0.00 m/3s
    uint16_t climbrate10s	= 30000;       			// 15 Climb rate m/10s. Steps of 0.01m/10s. Value of 30000 = 0.00 m/10s
    uint8_t text_msg[21];        					// 17 Free ASCII text message
    uint8_t free_char1		= ' ';       			// 38 Free ASCII character.  appears right to home distance
    uint8_t free_char2		= ' ';       			// 39 Free ASCII character.  appears right to home direction
    uint8_t free_char3		= ' ';       			// 40 Free ASCII character.  appears? TODO: Check where this char appears
    uint8_t compass_dir		= 0;       				// 41 Compass heading in 2 degrees steps. 1 = 2 degrees
    uint8_t version			= 0;       				// 42 version number = 3 for new protocol
    uint8_t EndByte 		= END_FRAME;        	// 43 0x7D
	uint8_t Crc;     								// 44 CRC
} PACKED VarioPacket_t;

typedef struct hottBusframe_s {
    uint8_t cmdMirror[CMD_LEN];
    uint8_t payload[FRAME_SIZE]; 
} PACKED hottBusframe_t;

typedef struct hottDevice_s {
    uint8_t deviceID;
    bool present;
} hottDevice_t;

enum HoTTDevices { FIRST_DEVICE = 0, GPS = FIRST_DEVICE, EAM, GAM, ESC, VARIO, LAST_DEVICE } ;

static hottDevice_t device[LAST_DEVICE] = {
    { SENSOR_ID_GPS_B, false },
    { SENSOR_ID_EAM_B, false },
    { SENSOR_ID_GAM_B, false },
    { SENSOR_ID_ESC_B, false },
    { SENSOR_ID_VARIO_B, false }
};

// HoTT telemetry packets
static GPSPacket_t gps;
static GeneralAirPacket_t gam;
static AirESCPacket_t esc;
static VarioPacket_t vario;
static ElectricAirPacket_t eam;

typedef struct crsf_sensor_gps_s {
    int32_t latitude;                               // degree / 10,000,000 big endian
    int32_t longitude;                              // degree / 10,000,000 big endian
    uint16_t groundspeed;                           // km/h / 10 big endian
    uint16_t heading;                               // GPS heading, degree/100 big endian
    uint16_t altitude;                              // meters, +1000m big endian
    uint8_t satellites;                             // satellites
} PACKED crsf_sensor_gps_t;

FIFO<HOTT_MAX_BUF_LEN> hottInputBuffer;

static hottBusframe_t hottBusFrame;

static bool discoveryMode = true;
static uint8_t nextDevice = FIRST_DEVICE;

extern Telemetry telemetry;


int SerialHoTT_TLM::getMaxSerialReadSize()
{
    return HOTT_MAX_BUF_LEN - hottInputBuffer.size();
}

void SerialHoTT_TLM::processBytes(uint8_t *bytes, u_int16_t size)
{
    hottInputBuffer.pushBytes(bytes, size);
}

void SerialHoTT_TLM::handleUARTout()
{  
    uint32_t now = millis();

    static uint32_t nextPoll = now + HOTT_POLL_RATE;
    static uint32_t discoveryTimer = now + DISCOVERY_TIMEOUT;

    // device discovery timer
    if(discoveryMode && now >= discoveryTimer)
        discoveryMode = false;

    // device polling scheduler
    if(now >= nextPoll) {          
        nextPoll = now + HOTT_POLL_RATE; 

        // start up in device discovery mode, after timeout regular operation 
        pollNextDevice();
    }

    uint8_t size = hottInputBuffer.size(); 

    if(size >= sizeof(hottBusFrame)) {
        // prepare polling next device 
        nextPoll = now + HOTT_LEAD_OUT;

        //fetch received serial data
        hottInputBuffer.popBytes((uint8_t *)&hottBusFrame, size);

        // process received frame if CRC is ok
        if(hottBusFrame.payload[CRC_INDEX] == calcFrameCRC((uint8_t *)&hottBusFrame.payload))
            processFrame();
    }

    scheduleCRSFtelemetry(now);
}

void SerialHoTT_TLM::pollNextDevice() {
    // clear serial in buffer    
    hottInputBuffer.flush();

    // work out next device to be polled in discovery mode (just poll all devices)
    if(discoveryMode) {                
        if(nextDevice == LAST_DEVICE)
            nextDevice = FIRST_DEVICE;

        pollDevice(device[nextDevice++].deviceID);

        return;
    }

    // work out next device to be polled in normal op mode (only poll discovered ones)
    for(uint i = 0; i < LAST_DEVICE; i++) {
        if(nextDevice == LAST_DEVICE)
            nextDevice = FIRST_DEVICE; 
        
        if(device[nextDevice].present) {
            pollDevice(device[nextDevice++].deviceID);

            break;
        }

        nextDevice++;
    }
}

void SerialHoTT_TLM::pollDevice(uint8_t id) {
    // send data request to device
    _outputPort->write(START_OF_CMD_B);
    _outputPort->write(id);
}

void SerialHoTT_TLM::processFrame() {
    void *frameData = (void *)&hottBusFrame.payload;

    // store received frame
    switch(hottBusFrame.payload[DEVICE_INDEX]) {
        case SENSOR_ID_GPS_B:
            device[GPS].present = true;
            memcpy((void *)&gps, frameData, sizeof(gps));
            break;

        case SENSOR_ID_EAM_B:
            device[EAM].present = true;
            memcpy((void *)&eam, frameData, sizeof(eam));
            break;

        case SENSOR_ID_GAM_B:
            device[GAM].present = true;
            memcpy((void *)&gam, frameData, sizeof(gam));
            break;

        case SENSOR_ID_VARIO_B:
            device[VARIO].present = true;
            memcpy((void *)&vario, frameData, sizeof(vario));
            break;

        case SENSOR_ID_ESC_B:
            device[ESC].present = true;
            memcpy((void *)&esc, frameData, sizeof(esc));
            break;
    }
}

uint8_t SerialHoTT_TLM::calcFrameCRC(uint8_t *buf) {
    uint16_t sum = 0;
        
    for(uint8_t i = 0 ; i < FRAME_SIZE-1; i++)
        sum += buf[i];
    return sum = sum & 0xff;
}

void SerialHoTT_TLM::scheduleCRSFtelemetry(uint32_t now) {
    // HoTT combined GPS/Vario -> send GPS and vario packet
    if(device[GPS].present) {
        sendCRSFgps(now);
        sendCRSFvario(now);
    } else
        // HoTT stand alone Vario and no GPS/Vario -> just send vario packet
        if(device[VARIO].present)
            sendCRSFvario(now);

    // HoTT GAM, EAM, ESC -> send batter packet
    if(device[GAM].present || device[EAM].present || device[ESC].present) {
        sendCRSFbattery(now);

        // HoTT GAM and EAM but no GPS/Vario or Vario -> send vario packet too
        if((!device[GPS].present && !device[VARIO].present) && (device[GAM].present || device[EAM].present))
            sendCRSFvario(now);
    }
}

void SerialHoTT_TLM::sendCRSFvario(uint32_t now) {
    static uint32_t lastVarioSent = 0;
    static uint32_t lastVarioCRC = 0;

    // indicate external sensor is present
    telemetry.SetCrsfBaroSensorDetected(true);

    // prepare CRSF telemetry packet
    CRSF_MK_FRAME_T(crsf_sensor_baro_vario_t) crsfBaro = {0};
    crsfBaro.p.altitude    = htobe16(getHoTTaltitude()*10 + 5000);      // Hott 500 = 0m, ELRS 10000 = 0.0m
    crsfBaro.p.verticalspd = htobe16(getHoTTvv() - 30000);
    CRSF::SetHeaderAndCrc((uint8_t *)&crsfBaro, CRSF_FRAMETYPE_BARO_ALTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_baro_vario_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
    
    // send packet only if min rate timer expired or values have changed
    if((now >= lastVarioSent + VARIO_MIN_CRSFRATE) || (lastVarioCRC != crsfBaro.crc)) {
        lastVarioSent = now;
    
        telemetry.AppendTelemetryPackage((uint8_t *)&crsfBaro);
    }


    lastVarioCRC = crsfBaro.crc;
}

void SerialHoTT_TLM::sendCRSFgps(uint32_t now) {
    static uint32_t lastGPSSent = 0;
    static uint32_t lastGPSCRC = 0;

    // prepare CRSF telemetry packet
    CRSF_MK_FRAME_T(crsf_sensor_gps_t) crsfGPS = {0};
    crsfGPS.p.latitude    = htobe32(getHoTTlatitude());
    crsfGPS.p.longitude   = htobe32(getHoTTlongitude());
    crsfGPS.p.groundspeed = htobe16(getHoTTgroundspeed()*10);           // Hott 1 = 1 km/h, ELRS 1 = 0.1km/h 
    crsfGPS.p.heading     = htobe16(getHoTTheading()*100);
    crsfGPS.p.altitude    = htobe16(getHoTTMSLaltitude() + 1000);       // HoTT 1 = 1m, CRSF: 0m = 1000
    crsfGPS.p.satellites  = getHoTTsatellites();
    CRSF::SetHeaderAndCrc((uint8_t *)&crsfGPS, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
    
    // send packet only if min rate timer expired or values have changed
    if((now >= lastGPSSent + GPS_MIN_CRSFRATE) || (lastGPSCRC != crsfGPS.crc)) {
        lastGPSSent = now;
    
        telemetry.AppendTelemetryPackage((uint8_t *)&crsfGPS);
    }

    lastGPSCRC = crsfGPS.crc;
}

void SerialHoTT_TLM::sendCRSFbattery(uint32_t now) {
    static uint32_t lastBatterySent = 0;
    static uint32_t lastBatteryCRC = 0;

    // indicate external sensor is present
    telemetry.SetCrsfBatterySensorDetected(true);

    // prepare CRSF telemetry packet
    CRSF_MK_FRAME_T(crsf_sensor_battery_t) crsfBatt = {0};
    crsfBatt.p.voltage   = htobe16(getHoTTvoltage());   
    crsfBatt.p.current   = htobe16(getHoTTcurrent());   
    crsfBatt.p.capacity  = htobe24(getHoTTcapacity()*10);               // HoTT: 1 = 10mAh, CRSF: 1 ? 1 = 1mAh
    crsfBatt.p.remaining = getHoTTremaining();
    CRSF::SetHeaderAndCrc((uint8_t *)&crsfBatt, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
    
    // send packet only if min rate timer expired or values have changed
    if((now >= lastBatterySent + BATT_MIN_CRSFRATE) || (lastBatteryCRC != crsfBatt.crc)) {
        lastBatterySent = now;
    
        telemetry.AppendTelemetryPackage((uint8_t *)&crsfBatt);
    }

    lastBatteryCRC = crsfBatt.crc;
}

// HoTT telemetry data getters
uint16_t SerialHoTT_TLM::getHoTTvoltage() { 
    if(device[EAM].present) 
        return eam.main_voltage;
    else 
    if(device[GAM].present) 
        return (gam.InputVoltage);
    else 
    if(device[ESC].present) 
        return esc.input_v;
    else
        return 0;
}

uint16_t SerialHoTT_TLM::getHoTTcurrent() {
    if(device[EAM].present) 
        return eam.current;
    else 
    if(device[GAM].present) 
        return gam.Current;
    else 
    if(device[ESC].present) 
        return esc.current;
    else
        return 0;
}

uint32_t SerialHoTT_TLM::getHoTTcapacity() { 
    if(device[EAM].present) 
        return eam.batt_cap;
    else 
    if(device[GAM].present) 
        return (gam.Capacity);
    else 
    if(device[ESC].present) 
        return esc.capacity;
    else
        return 0;
}

int16_t SerialHoTT_TLM::getHoTTaltitude() { 
    if(device[GPS].present) 
        return gps.Altitude;
    else 
    if(device[VARIO].present) 
        return vario.altitude;
    else 
    if(device[EAM].present) 
        return eam.altitude;
    else
    if(device[GAM].present) 
        return gam.Altitude;
    else
        return 0;
}

int16_t SerialHoTT_TLM::getHoTTvv() {
    if(device[GPS].present) 
        return (gps.m_per_sec);
    else 
    if(device[VARIO].present) 
        return (vario.climbrate);
    else 
    if(device[EAM].present) 
        return (eam.climbrate);
    else
    if(device[GAM].present) 
        return (gam.m_per_sec);
    else
        return 0;
}

uint8_t SerialHoTT_TLM::getHoTTremaining() {
    if(!device[GAM].present)
        return 0;
        
    return gam.fuel_scale; 
}

int32_t SerialHoTT_TLM::getHoTTlatitude() { 
    if(!device[GPS].present)
        return 0;

    uint8_t deg = gps.Lat_DegMin/100;

    float fLat = (float)(deg)*10000000.0 + 
                    (float)(gps.Lat_DegMin-deg*100)*1000000.0/6.0 +
                    (float)(gps.Lat_Sec)*100.0/6.0;

    if(gps.Lat_NS != 0)
        fLat = -fLat;

    return(fLat);
}

int32_t SerialHoTT_TLM::getHoTTlongitude() { 
    if(!device[GPS].present)
        return 0;
            
    uint8_t deg = gps.Lon_DegMin/100;

    float fLon = (float)(deg)*10000000.0 + 
                    (float)(gps.Lon_DegMin-deg*100)*1000000.0/6.0 +
                    (float)(gps.Lon_Sec)*100.0/6.0;

    if(gps.Lon_EW != 0)
        fLon = -fLon;

    return fLon;
}

uint16_t SerialHoTT_TLM::getHoTTgroundspeed() {
    if(!device[GPS].present)
        return 0;
            
    return gps.Speed; 
}

uint16_t SerialHoTT_TLM::getHoTTheading() { 
    if(!device[GPS].present)
        return 0;

    uint16_t heading = gps.Direction * 2;
    
    if(heading > 180)
        heading -= 360;

    return (heading); 
}

uint8_t SerialHoTT_TLM::getHoTTsatellites() {
    if(!device[GPS].present)
        return 0;
        
    return gps.Satellites;
}

uint16_t SerialHoTT_TLM::getHoTTMSLaltitude() {
    if(!device[GPS].present)
        return 0;
        
    return gps.MSLAltitude;
}

uint32_t SerialHoTT_TLM::htobe24(uint32_t val) {
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return val;
#else
    uint8_t *ptrByte = (uint8_t *)&val;

    uint8_t swp = ptrByte[0];
    ptrByte[0]  = ptrByte[2];
    ptrByte[2]  = swp;

    return val;
#endif
}

#endif
