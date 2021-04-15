#ifdef PLATFORM_ESP32

#include "targets.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#endif

#include "ESP32_hwTimer.h"
extern hwTimer hwTimer;

#include "CRSF.h"
extern CRSF crsf;

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <Update.h>

#include "ESP32_WebUpdate.h"

const char *ssid = "ExpressLRS TX Module"; // The name of the Wi-Fi network that will be created
const char *password = "expresslrs";       // The password required to connect to it, leave blank for an open network
const char *myHostname = "elrs_tx";

unsigned int status = WL_IDLE_STATUS;

const byte DNS_PORT = 53;
IPAddress apIP(10, 0, 0, 1);
IPAddress netMsk(255, 255, 255, 0);
DNSServer dnsServer;
WebServer server(80);

/** Is this an IP? */
boolean isIp(String str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9'))
        {
            return false;
        }
    }
    return true;
}

/** IP to String? */
String toStringIp(IPAddress ip)
{
    String res = "";
    for (int i = 0; i < 3; i++)
    {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

bool captivePortal()
{
    if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname) + ".local"))
    {
        Serial.println("Request redirected to captive portal");
        server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
        server.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
        server.client().stop();             // Stop is needed because we sent no content length
        return true;
    }
    return false;
}

void WebUpdateSendcss()
{
    server.send_P(200, "text/css", CSS);
}


void WebUpdateSendReturn()
{
    server.send_P(200, "text/html", GO_BACK);
}

void WebUpdateHandleRoot()
{
    if (captivePortal())
    { // If captive portal redirect instead of displaying the page.
        return;
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send_P(200, "text/html", INDEX_HTML);
}

void WebUpdateHandleNotFound()
{
    if (captivePortal())
    { // If captive portal redirect instead of displaying the error page.
        return;
    }
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += server.uri();
    message += F("\nMethod: ");
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += server.args();
    message += F("\n");

    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += String(F(" ")) + server.argName(i) + F(": ") + server.arg(i) + F("\n");
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(404, "text/plain", message);
}

void BeginWebUpdate()
{
    hwTimer.stop();
    crsf.End();
    Radio.End();

    Serial.println("Begin Webupdater");
    Serial.println("Stopping Radio");

    WiFi.persistent(false);
    WiFi.disconnect();   //added to start with the wifi off, avoid crashing
    WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
    WiFi.setHostname(myHostname);
    delay(500);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(ssid, password);

    server.on("/", WebUpdateHandleRoot);
    server.on("/css.css", WebUpdateSendcss);

    server.on("/generate_204", WebUpdateHandleRoot); // handle Andriod phones doing shit to detect if there is 'real' internet and possibly dropping conn.
    server.on("/gen_204", WebUpdateHandleRoot);
    server.on("/library/test/success.html", WebUpdateHandleRoot);
    server.on("/hotspot-detect.html", WebUpdateHandleRoot);
    server.on("/connectivity-check.html", WebUpdateHandleRoot);
    server.on("/check_network_status.txt", WebUpdateHandleRoot);
    server.on("/ncsi.txt", WebUpdateHandleRoot);
    server.on("/fwlink", WebUpdateHandleRoot);
    server.onNotFound(WebUpdateHandleNotFound);

    server.on(
        "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart(); }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin()) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      } else {
        Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
      } });

    dnsServer.start(DNS_PORT, "*", apIP);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    
    if (!MDNS.begin(myHostname))
    {
      Serial.println("Error starting mDNS");
      return;
    }
    MDNS.addService("http", "tcp", 80);

    server.begin();
}

void HandleWebUpdate()
{
    dnsServer.processNextRequest();
    server.handleClient();
    delay(1);
}

#endif