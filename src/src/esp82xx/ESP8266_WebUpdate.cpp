#include "debug_elrs.h"

#define USE_WIFI_MANAGER 1

//#define STATIC_IP_AP     "192.168.4.1"
//#define STATIC_IP_CLIENT "192.168.1.50"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#if USE_WIFI_MANAGER
#include <WiFiManager.h>
#endif /* USE_WIFI_MANAGER */

#define STASSID "ExpressLRS RX AP"
#define STAPSK "expresslrs"

MDNSResponder mdns;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

void BeginWebUpdate(void)
{
    const char *host = "elrs_rx";

    DEBUG_PRINTLN("Begin Webupdater");

    wifi_station_set_hostname(host);

    IPAddress addr;

#if USE_WIFI_MANAGER
    WiFiManager wifiManager;
    //WiFiManagerParameter header("<p>Express LRS ESP82xx RX</p>");
    //wifiManager.addParameter(&header);

#ifdef STATIC_IP_AP
    /* Static portal IP (default: 192.168.4.1) */
    addr.fromString(STATIC_IP_CLIENT);
    wifiManager.setAPStaticIPConfig(addr,
                                    IPAddress(192,168,4,1),
                                    IPAddress(255,255,255,0));
#endif /* STATIC_IP_AP */
#ifdef STATIC_IP_CLIENT
    /* Static client IP */
    addr.fromString(STATIC_IP_CLIENT);
    wifiManager.setSTAStaticIPConfig(addr,
                                     IPAddress(192,168,1,1),
                                     IPAddress(255,255,255,0));
#endif /* STATIC_IP_CLIENT */

    wifiManager.setConfigPortalTimeout(60);
    //wifiManager.autoConnect(STASSID, STAPSK);
    if (wifiManager.autoConnect(STASSID)) // start unsecure portal AP
    {
        addr = WiFi.localIP();
    }
    else
#endif /* USE_WIFI_MANAGER */
    {
        // No wifi found, start AP
        WiFi.mode(WIFI_AP);
        WiFi.softAP(STASSID, STAPSK);
        addr = WiFi.softAPIP();
    }

    if (mdns.begin(host, addr))
    {
        mdns.addService("http", "tcp", 80);
        mdns.update();
    }

    httpUpdater.setup(&httpServer);
    httpServer.begin();

    DEBUG_PRINTF("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
}

void HandleWebUpdate(void)
{
    httpServer.handleClient();
    mdns.update();
}
