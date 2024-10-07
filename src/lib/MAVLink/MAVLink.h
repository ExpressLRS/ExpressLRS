#include "CRSF.h"
#define MAVLINK_COMM_NUM_BUFFERS 1
#include "common/mavlink.h"
#include <CRSFHandset.h>

// Takes a MAVLink message wrapped in CRSF and possibly converts it to a CRSF telemetry message
void convert_mavlink_to_crsf_telem(uint8_t *CRSFinBuffer, uint8_t count, Handset *handset);

bool isThisAMavPacket(uint8_t *buffer, uint16_t bufferSize);
uint16_t buildMAVLinkELRSModeChange(uint8_t mode, uint8_t *buffer);