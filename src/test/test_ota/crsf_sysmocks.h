#include <stdint.h>

extern "C" {

uint32_t OpenTx_to_Crsf(int16_t pulse);
int16_t Crsf_to_OpenTx(uint32_t crsfval);
uint32_t OpenTx_to_Us(int16_t pulse);
int16_t Us_to_OpenTx(uint32_t us);
uint32_t Crsf_to_OpenTx_to_Us(uint32_t crsfval);
uint32_t Crsf_to_Uint10_to_Crsf(uint32_t crsfval);
uint32_t Crsf_to_BfUs(uint32_t crsfval);

}