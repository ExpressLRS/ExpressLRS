#include "debug.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#define STASSID "ExpressLRS RX"
#define STAPSK "expresslrs"

const char *host = "esp8266-webupdate";
const char *ssid = STASSID;
const char *password = STAPSK;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

void BeginWebUpdate(void)
{

    DEBUG_PRINTLN("Begin Webupdater");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    MDNS.begin(host);

    httpUpdater.setup(&httpServer);
    httpServer.begin();

    MDNS.addService("http", "tcp", 80);
    DEBUG_PRINTF("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
}

void HandleWebUpdate(void)
{
    httpServer.handleClient();
    MDNS.update();
}
