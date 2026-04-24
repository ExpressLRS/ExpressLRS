#include "crsf_sysmocks.h"
#include "crsf_protocol.h"
/**
 * Functions to replicate the CRSF conversion routines in OpenTX / Betaflight
 **/
extern "C" {

#define limit(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define CROSSFIRE_CENTER            0x3E0 // 992
#define CROSSFIRE_CENTER_CH_OFFSET  (0)
#define PPM_CENTER                  1500
#define PPM_CH_CENTER               (PPM_CENTER)

uint32_t OpenTx_to_Crsf(int16_t pulse)
{
    uint32_t val = limit(0, CROSSFIRE_CENTER + (CROSSFIRE_CENTER_CH_OFFSET * 4) / 5 + (pulse * 4) / 5, 2 * CROSSFIRE_CENTER);
    return val;
}

int16_t Crsf_to_OpenTx(uint32_t crsfval)
{
    int16_t val = (crsfval - CROSSFIRE_CENTER - (CROSSFIRE_CENTER_CH_OFFSET * 5 / 4)) * 5 / 4;
    return val;
}

uint32_t OpenTx_to_Us(int16_t pulse)
{
    return PPM_CENTER + (pulse / 2);
}

int16_t Us_to_OpenTx(uint32_t us)
{
    return 2 * ((int32_t)(us) - PPM_CENTER);
}

uint32_t Crsf_to_OpenTx_to_Us(uint32_t crsfval)
{
    return OpenTx_to_Us(Crsf_to_OpenTx(crsfval));
}

uint32_t Crsf_to_Uint10_to_Crsf(uint32_t crsfval)
{
    return UINT10_to_CRSF(CRSF_to_UINT10(crsfval));
}

#define CRSF_RC_CHANNEL_SCALE_LEGACY                0.62477120195241f
uint32_t Crsf_to_BfUs(uint32_t crsfval)
{
    float val = (CRSF_RC_CHANNEL_SCALE_LEGACY * (float)crsfval) + 881;
    //printf("%u=%f ", crsfval, val);
    return val;
}

} // extern "C"