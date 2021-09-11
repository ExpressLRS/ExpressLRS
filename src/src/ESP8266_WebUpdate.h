#ifdef PLATFORM_ESP8266
#include <ESP8266WiFi.h>

void BeginWebUpdate(void);
void HandleWebUpdate(void);

extern bool IsWebUpdateMode;
#endif
