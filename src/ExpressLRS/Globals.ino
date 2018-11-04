//Variables that are shared between various sub functions 

#define numRCchannels 16 //number of RC channels, 16 should be enough for now 
char RCdataIN[numRCchannels];
char RCdataOUT[numRCchannels];

int RCdata[numRCchannels]; //holds the current value of the RC (the actual control channels)

unsigned long CURRfrequnecy;
unsigned int TimeOnAir = 0; //how long in micros the radio spends transmitting
