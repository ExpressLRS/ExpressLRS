#include "crc.h"
#include "crsf_protocol.h"

#ifdef PLATFORM_ESP32
void ESP32uartTask(void *pvParameters);
#endif

GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);

