#if (defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)) && defined(RADIO_LR1121)

#include <ESPAsyncWebServer.h>

void addLR1121Handlers(AsyncWebServer &server);

#endif