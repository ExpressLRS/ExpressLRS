#pragma once

// activate Extensions (MultiSwitch, SumDV3, LedRGB)
#define WMEXTENSION

// activate 32 channels extension (sbus@serial: ch1-16, sbus@serial2: ch17-ch32)
#define WMCRSF_CHAN_EXT

// output ch17-32 on sumdv3 ch1-16
#define WMSUMDV3_SWAP

// output crsf 0x16 and 0x26 in on go (concatenate both packages), otherwise round robin
#define WMCRSF_CH_OUT_CONCAT

// do not output 0x26 packets / ch17-32
#define WMCRSF_NO_HIGH_CHANNELS

// low-pass filter of channel values
#define WMFILTER32
#define WMFILTER32W 500

