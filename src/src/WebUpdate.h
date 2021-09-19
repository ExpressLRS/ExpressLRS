#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)

extern bool webserverPreventAutoStart;

extern void wifiOff();

extern void beginWebServer();
extern bool handleWebUpdateServer(unsigned long now); // returns true if in wifi mode

#endif
