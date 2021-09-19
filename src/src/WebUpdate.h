#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)

extern bool webserverPreventAutoStart;

extern void wifiOff();

extern void beginWebServer();
extern void handleWebUpdateServer(unsigned long now);

#endif
