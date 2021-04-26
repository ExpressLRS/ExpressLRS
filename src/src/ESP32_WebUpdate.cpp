#ifdef PLATFORM_ESP32

#include "targets.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
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

#include "config.h"
extern TxConfig config;

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <Update.h>
#include <set>
#include <string.h>

#include "ESP32_WebContent.h"
#include "config.h"

uint8_t target_seen = 0;
uint8_t target_pos = 0;

const char *ssid = "ExpressLRS TX Module"; // The name of the Wi-Fi network that will be created
const char *password = "expresslrs";       // The password required to connect to it, leave blank for an open network
const char *myHostname = "elrs_tx";

unsigned int status = WL_IDLE_STATUS;

const byte DNS_PORT = 53;
IPAddress apIP(10, 0, 0, 1);
IPAddress netMsk(255, 255, 255, 0);
DNSServer dnsServer;
WebServer server(80);

static int numNetworks;

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

void WebUpdateSendCSS()
{
    server.send_P(200, "text/css", CSS);
}

void WebUpdateSendPNG()
{
  server.send_P(200, "image/png", (PGM_P)PNG, sizeof(PNG));
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

void WebUpdateScanHome(void)
{
  WiFi.disconnect();
  numNetworks = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", numNetworks);
  server.send_P(200, "text/html", SCAN_HTML);
}

void WebUpdateSendNetworks()
{
  String s;
  char buf[20];
  std::set<String> vs;
  s+="[";
  for(int i=0 ; i<numNetworks ; i++) {
    String w = WiFi.SSID(i);
    Serial.printf("found %s\n", w.c_str());
    if (vs.find(w)==vs.end() && w.length()>0) {
      if (s.length() > 1) s += ",";
      s += "{\"v\":" + String(itoa(i, buf, 10)) + ",";
      s+="\"t\":\"" + w + "\"}";
      vs.insert(w);
    }
  }
  s+="]";
  server.send(200, "application/json", s);
}

void WebUpdateSetHome(void)
{
  String network = server.arg("network");
  String password = server.arg("password");
  system_event_id_t status = SYSTEM_EVENT_MAX;

  String ssid = WiFi.SSID(network.toInt());
  Serial.printf("Joining: %s\n", ssid.c_str());
  
  WiFi.setHostname(myHostname);
  WiFi.onEvent([&status](WiFiEvent_t event, WiFiEventInfo_t info){status = event;});
  WiFi.begin(ssid.c_str(), password.c_str());

  while(status == SYSTEM_EVENT_MAX) {
    yield();
    delay(50);
  }
  if (status == SYSTEM_EVENT_STA_CONNECTED) {
    Serial.printf("Connected IPAddress=%s\n", WiFi.localIP().toString().c_str());
    config.SetSSID(ssid.c_str());
    config.SetPassword(password.c_str());
    config.Commit();
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    // server.begin();
  } else if (status == SYSTEM_EVENT_STA_DISCONNECTED) {
    Serial.println("Connection failed");
    server.sendHeader("Location", "/scanhome?error", true);
    server.send(302, "text/plain", "");
  }
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
    Radio.End();
    crsf.End();

    Serial.println("Begin Webupdater");
    Serial.println("Stopping Radio");

    WiFi.persistent(false);
    WiFi.disconnect();   //added to start with the wifi off, avoid crashing
    WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
    WiFi.setHostname(myHostname);
    delay(500);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(ssid, password);

    server.on("/", WebUpdateHandleRoot);
    server.on("/main.css", WebUpdateSendCSS);
    server.on("/flag.png", WebUpdateSendPNG);
    server.on("/networks.json", WebUpdateSendNetworks);
    
    server.on("/generate_204", WebUpdateHandleRoot); // handle Andriod phones doing shit to detect if there is 'real' internet and possibly dropping conn.
    server.on("/gen_204", WebUpdateHandleRoot);
    server.on("/library/test/success.html", WebUpdateHandleRoot);
    server.on("/hotspot-detect.html", WebUpdateHandleRoot);
    server.on("/connectivity-check.html", WebUpdateHandleRoot);
    server.on("/check_network_status.txt", WebUpdateHandleRoot);
    server.on("/ncsi.txt", WebUpdateHandleRoot);
    server.on("/fwlink", WebUpdateHandleRoot);
    server.on("/scanhome", WebUpdateScanHome);
    server.on("/sethome", HTTP_POST, WebUpdateSetHome);
    server.onNotFound(WebUpdateHandleNotFound);

    server.on(
        "/update", HTTP_POST, []() {
      server.client().setNoDelay(true);
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", target_seen ? ((Update.hasError()) ? "FAIL" : "OK") : "WRONG FIRMWARE");
      delay(100);
      server.client().stop();
      ESP.restart(); },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin()) { //start with max available size
          Update.printError(Serial);
        }
        target_seen = 0;
        target_pos = 0;
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
        if (!target_seen) {
          for (int i=0 ; i<upload.currentSize ;i++) {
            if (upload.buf[i] == target_name[target_pos]) {
              ++target_pos;
              if (target_pos >= target_name_size) {
                target_seen = 1;
              }
            }
            else {
              target_pos = 0; // Startover
            }
          }
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (target_seen) {
          if (Update.end(true)) { //true to set the size to the current progress
            Serial.printf("Upload Success: %ubytes\nPlease wait for LED to resume blinking before disconnecting power\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        } else {
          Update.abort();
          Serial.printf("Wrong firmware uploaded, not %s, update aborted\n", &target_name[4]);
        }
        Serial.setDebugOutput(false);
      } else if(upload.status == UPLOAD_FILE_ABORTED){
        Update.abort();
        Serial.println("Update was aborted");
      } });

    dnsServer.start(DNS_PORT, "*", apIP);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

    if (!MDNS.begin(myHostname))
    {
      Serial.println("Error starting mDNS");
      return;
    }
    MDNS.addService("http", "tcp", 80);

    if (config.GetSSID()[0]) {
      Serial.println("Connecting...");
      WiFi.setHostname(myHostname);
      WiFi.begin(config.GetSSID(), config.GetPassword());
    }

    server.begin();
}

void HandleWebUpdate()
{
    dnsServer.processNextRequest();
    server.handleClient();
    yield();
}

#endif
