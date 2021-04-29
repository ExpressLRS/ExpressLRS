#ifdef PLATFORM_ESP8266
#include "ESP8266_WebUpdate.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#endif

#define QUOTE(arg) #arg
#define STR(macro) QUOTE(macro)
const char target_name[] = "\xBE\xEF\xCA\xFE" STR(TARGET_NAME);
uint8_t target_seen = 0;
uint8_t target_pos = 0;

#define STASSID "ExpressLRS RX"
#define STAPSK "expresslrs"
const char *myHostname = "elrs_rx";

const char *ssid = STASSID;
const char *password = STAPSK;

extern hwTimer hwTimer;

const byte DNS_PORT = 53;
IPAddress apIP(10, 0, 0, 1);
IPAddress netMsk(255, 255, 255, 0);
DNSServer dnsServer;
//MDNSResponder mdns;
ESP8266WebServer server(80);

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

void BeginWebUpdate(void)
{
  hwTimer.stop();

  Radio.RXdoneCallback = NULL;
  Radio.TXdoneCallback = NULL;

  Serial.println("Begin Webupdater");
  Serial.println("Stopping Radio");
  Radio.End();

  WiFi.persistent(false);
  WiFi.disconnect();   //added to start with the wifi off, avoid crashing
  WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
  WiFi.setOutputPower(13);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  wifi_station_set_hostname(myHostname);
  delay(500);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid, password);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

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

    // handler for the /update form POST (once file upload finishes)
  server.on("/update", HTTP_POST, [&](){
      server.client().setNoDelay(true);
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", target_seen ? ((Update.hasError()) ? "FAIL" : "OK") : "WRONG FIRMWARE");
      delay(100);
      server.client().stop();
      ESP.restart(); },
    []() {
      HTTPUpload& upload = server.upload();
      if(upload.status == UPLOAD_FILE_START){
          Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
          Update.printError(Serial);
        }
        target_seen = 0;
        target_pos = 0;
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          Update.printError(Serial);
        }
        if (!target_seen) {
          for (size_t i=0 ; i<upload.currentSize ;i++) {
            if (upload.buf[i] == target_name[target_pos]) {
              ++target_pos;
              if (target_pos >= sizeof(target_name)) {
                target_seen = 1;
              }
            }
            else {
              target_pos = 0; // Startover
            }
          }
        }
      } else if(upload.status == UPLOAD_FILE_END){
        if (target_seen) {
          if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        } else {
          Update.end();
          Serial.printf("Wrong firmware uploaded, not %s, update aborted\n", &target_name[4]);
        }
        Serial.setDebugOutput(false);
      } else if(upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        Serial.println("Update was aborted");
      }
      delay(0);
    });

  server.begin();
  Serial.printf("HTTPUpdateServer ready! Open http://%s in your browser\n", myHostname);
}

void HandleWebUpdate(void)
{
  server.handleClient();
  dnsServer.processNextRequest();
  //mdns.update();
  yield();
}
#endif
