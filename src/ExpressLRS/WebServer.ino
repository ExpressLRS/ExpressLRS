#include <DNSServer.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <FS.h>
#include "SPIFFS.h"
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiUdp.h>

const char* host = "esp32-webupdate";

//const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

//#ifdef ESP8266
//#include <ESP8266WiFi.h>
//#include <WiFiClient.h>
//#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h>
//ESP8266WebServer server(80);
//const char* host = "esp8266-webupdate";
//#else
//#include <WiFi.h>
//#include <WiFiClient.h>
//#include <WebServer.h>
//#include <ESPmDNS.h>
//WebServer server(80);
//const char* host = "esp32-webupdate";
//#endif


const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer webServer(80);
WiFiClient client = webServer.client();

void HandleWebserver() {
  dnsServer.processNextRequest();
  webServer.handleClient();
}


bool handleFileRead(String path) { // send the right file to the client (if it exists)

  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {
    Serial.println(path);// If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = webServer.streamFile(file, contentType); // And send it to the client
    file.close();
    return true;
  }
  Serial.print("\tFile Not Found: ");
  Serial.println(path);

  return false;                                         // If the file doesn't exist, return false
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".htm")) return "text/css";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}


void WifiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  String MAC = "ABC";
  WiFi.softAP("ESP32_");
}


void initWebSever() {

  SPIFFS.begin();
  delay(500);

  dnsServer.start(DNS_PORT, "*", apIP);


  webServer.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(webServer.uri()))                  // send it if it exists
      webServer.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  webServer.on("/updateIO.html", ProcessIOconfig);

  webServer.on("/generate_204", HTTP_GET, []() {
    webServer.sendHeader("Connection", "close");
    File file = SPIFFS.open("/index.html", "r");                 // Open it
    size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
    file.close();
  });

  webServer.on("/", HTTP_GET, []() {
    webServer.sendHeader("Connection", "close");
    File file = SPIFFS.open("/index.html", "r");                 // Open it
    size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
    file.close();
  });


  webServer.on("/update", HTTP_POST, []() {
    webServer.sendHeader("Connection", "close");
    webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK, module rebooting");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);

#ifdef ESP8266
      WiFiUDP::stopAll();
#endif

      Serial.printf("Update: %s\n", upload.filename.c_str());
#ifdef ESP8266
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
#else
      uint32_t maxSketchSpace = 0x140000;
#endif

      if (!Update.begin(maxSketchSpace)) { //start with max available size
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
    }
    yield();
  });


  webServer.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  client.setNoDelay(1);

}


void ProcessIOconfig() {
  String SerialRXMode = webServer.arg("SerialRXMode");
  String Telm_Output = webServer.arg("Telm_Output");
  String Ble_Mode = webServer.arg("Ble_Mode");


  if (SerialRXMode == "PPM") {
    Serial.println("RC mode set to PPM");
  }
  if (SerialRXMode == "SBUS") {
    Serial.println("RC mode set to SBUS");
    SerRXmode = PROTO_RX_SBUS;
  }
  if (SerialRXMode == "PPX") {
    Serial.println("RC mode set to PXX");
    SerRXmode = PROTO_RX_PXX;
  }
  if (SerialRXMode == "CRSF") {
    Serial.println("RC mode set to CRSF");
    SerRXmode = PROTO_RX_CRSF;
  }

  if (Telm_Output == "NONE") {
    Serial.println("Telemetry mode set to NONE");
  }
  if (Telm_Output == "SPORT") {
    Serial.println("Telemetry mode set to SPORT");
  }
  if (Telm_Output == "CRSF") {
    Serial.println("Telemetry mode set to CRSF");
  }
  if (Telm_Output == "FRSKY") {
    Serial.println("Telemetry mode set to FRSKY");
  }

  if (Ble_Mode == "OFF") {
    Serial.println("Bluetooth mode set to OFF");
  }
  if (Ble_Mode == "Mirror Telemetry") {
    Serial.println("Bluetooth mode set to Mirror Telemetry");
  }
  if (Ble_Mode == "Serial Debug") {
    Serial.println("Bluetooth mode set to Serial Debug");
  }
  webServer.sendHeader("Connection", "close");
  File file = SPIFFS.open("/updateIO.html", "r");                 // Open it
  size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
  file.close();
}


