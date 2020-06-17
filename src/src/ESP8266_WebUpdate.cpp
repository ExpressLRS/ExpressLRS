#ifdef PLATFORM_ESP8266

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280RadioLib.h"
extern SX1280Driver Radio;
#endif

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include "ESP8266_hwTimer.h"


#define STASSID "ExpressLRS RX"
#define STAPSK "expresslrs"

const char *host = "esp8266-webupdate";
const char *ssid = STASSID;
const char *password = STAPSK;

extern hwTimer hwTimer;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

void BeginWebUpdate(void)
{
  hwTimer.stop();

  Radio.RXdoneCallback = NULL;
  Radio.TXdoneCallback = NULL;

  Serial.println("Begin Webupdater");
  Serial.println("Stopping Radio");
  Radio.End();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
}

void HandleWebUpdate(void)
{
  httpServer.handleClient();
  MDNS.update();
}

#endif