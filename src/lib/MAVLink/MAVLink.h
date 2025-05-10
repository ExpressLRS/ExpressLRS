#define MAVLINK_COMM_NUM_BUFFERS 1
#include <CRSFHandset.h>

// Takes a MAVLink message wrapped in CRSF and possibly converts it to a CRSF telemetry message
void convert_mavlink_to_crsf_telem(CRSFConnector *origin, uint8_t *CRSFinBuffer, uint8_t count);

bool isThisAMavPacket(uint8_t *buffer, uint16_t bufferSize);
uint16_t buildMAVLinkELRSModeChange(uint8_t mode, uint8_t *buffer);