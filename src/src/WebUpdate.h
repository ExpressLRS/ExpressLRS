#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)

extern bool webserverPreventAutoStart;

void wifiOff();

bool handleWebUpdateServer(unsigned long now); // returns true if in wifi mode

#endif
