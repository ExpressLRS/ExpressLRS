#include "ESP8266_WebUpdate.h"

#ifdef RAPIDFIRE_BACKPACK
  #define STASSID "ExpressLRS Rapidfire Backpack"
#endif
#ifdef GENERIC_BACKPACK
  #define STASSID "ExpressLRS VRx Backpack"
#endif
#ifdef TARGET_TX_BACKPACK
  #define STASSID "ExpressLRS TX Backpack"
#endif

#define STAPSK "expresslrs"
const char *myHostname = "expresslrs";

const char *ssid = STASSID;
const char *password = STAPSK;

const byte DNS_PORT = 53;
IPAddress apIP(10, 0, 0, 1);
DNSServer dnsServer;
ESP8266WebServer server(80);

ESP8266HTTPUpdateServer httpUpdater;

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
  Serial.println("Begin Webupdater");

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

  WiFi.persistent(false);
  WiFi.disconnect();   //added to start with the wifi off, avoid crashing
  WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
  WiFi.setOutputPower(13);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.softAP(ssid, password);
  delay(1000);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  dnsServer.start(DNS_PORT, "*", apIP);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  MDNS.begin(myHostname);
  httpUpdater.setup(&server);
  server.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://10.0.0.1 in your browser\n");
}

void HandleWebUpdate(void)
{
  server.handleClient();
  MDNS.update();
  yield();
  delay(1);
}
