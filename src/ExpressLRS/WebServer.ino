//#include <DNSServer.h>
//#include <WiFiClient.h>
//#include <FS.h>
//#include <WiFiUdp.h>
//
//#if defined(ARDUINO_ESP32_DEV)
//
//#include <WebServer.h>
//#include <ESPmDNS.h>
//#include "SPIFFS.h"
//#include <Update.h>
//#include "ESPAsyncWebServer.h"
//
//#else
//
//#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h>
//#include <Updater.h>
//
//#endif
//
//////////Variables that the webserver needs access to////////////////////
//
//extern SerialRXmode SerRXmode;
//extern TrainerMode TrainTXmode;
//extern SerialTXmode SerTXmode;
//extern SerialTelmMode SerTelmMode;
//extern BluetoothMode BleMode;
//
///////////////////////////////////////////////////////////////////////////
//
//
//String getMacAddress() {
//  byte mac[6];
//  WiFi.macAddress(mac);
//  String cMac = "";
//  for (int i = 0; i < 6; ++i) {
//    cMac += String(mac[i], HEX);
//    if (i < 5)
//      cMac += "-";
//  }
//  cMac.toUpperCase();
//  Serial.print("Mac Addr:");
//  Serial.println(cMac);
//  return cMac;
//}
//
//
//#if defined(ARDUINO_ESP32_DEV)
////WebServer webServer(80);
//AsyncWebServer  webServer(80);
//#else
//ESP8266WebServer webServer(80);
//#endif
//
//const byte DNS_PORT = 53;
//IPAddress apIP(192, 168, 4, 1);
//DNSServer dnsServer;
////WiFiClient client = webServer.client();
//
//void HandleWebserver() {
//  dnsServer.processNextRequest();
//  //webServer.handleClient();
//}
//
//
//bool handleFileRead(String path) { // send the right file to the client (if it exists)
//#ifdef ARDUINO_ESP8266_ESP01
//  noInterrupts();
//#endif
//
//  //#if defined(ARDUINO_ESP32_DEV)
//  //  if (path.endsWith("/")) path += "index.html";
//  //#else
//  //  if (path.endsWith("/")) path += "index_recv.html";  //if the complier detects the reciever module we will transparently send the index file for the reciever instead
//  //#endif
//
//  // If a folder is requested, send the index file
//  String contentType = getContentType(path);            // Get the MIME type
//  if (SPIFFS.exists(path)) {
//    Serial.println(path);// If the file exists
//    File file = SPIFFS.open(path, "r");                 // Open it
//    size_t sent = webServer.streamFile(file, contentType); // And send it to the client
//    file.close();
//    return true;
//  }
//  Serial.print("\tFile Not Found: ");
//  Serial.println(path);
//
//#ifdef ARDUINO_ESP8266_ESP01
//  interrupts();
//#endif
//
//  return false;                                         // If the file doesn't exist, return false
//}
//
//String getContentType(String filename) { // convert the file extension to the MIME type
//  if (filename.endsWith(".html")) return "text/html";
//  else if (filename.endsWith(".htm")) return "text/css";
//  else if (filename.endsWith(".css")) return "text/css";
//  else if (filename.endsWith(".js")) return "application/javascript";
//  else if (filename.endsWith(".ico")) return "image/x-icon";
//  else if (filename.endsWith(".svg")) return "image/svg+xml";
//  return "text/plain";
//}
//
//
//void WifiAP() {
//
//  WiFi.mode(WIFI_AP);
//  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
//  String MAC = "ABC";
//#if defined(ARDUINO_ESP32_DEV)
//  WiFi.softAP("ExpressLRS TX");
//#else
//  WiFi.softAP("ExpressLRS RX");
//#endif
//}
//
//
//void initWebSever() {
//
//  SPIFFS.begin();
//  delay(1000);
//
//  dnsServer.start(DNS_PORT, "*", apIP);
//
//
//  webServer.onNotFound([]() {                              // If the client requests any URI
//    if (!handleFileRead(webServer.uri()))                  // send it if it exists
//      webServer.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
//  });
//
//  webServer.on("/updateIO.html", ProcessIOconfig);
//
//  webServer.on("/generate_204", HTTP_GET, []() {
//#ifdef ARDUINO_ESP8266_ESP01
//    noInterrupts();
//#endif
//    webServer.sendHeader("Connection", "close");
//    File file = SPIFFS.open("/index.html", "r");                 // Open it
//    size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
//    file.close();
//#ifdef ARDUINO_ESP8266_ESP01
//    interrupts();
//#endif
//  });
//
//  webServer.on("/", HTTP_GET, []() {
//#ifdef ARDUINO_ESP8266_ESP01
//    noInterrupts();
//#endif
//    webServer.sendHeader("Connection", "close");
//#ifdef ARDUINO_ESP8266_ESP01
//    File file = SPIFFS.open("/index_recv.html", "r");
//#endif
//
//#ifdef ARDUINO_ESP32_DEV
//    File file = SPIFFS.open("/index.html", "r");
//#endif
//    // Open it
//    size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
//    file.close();
//#ifdef ARDUINO_ESP8266_ESP01
//    interrupts();
//#endif
//  });
//
//
//  webServer.on("/update", HTTP_POST, []() {
//    webServer.sendHeader("Connection", "close");
//    webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK, module rebooting");
//    ESP.restart();
//  }, []() {
//    HTTPUpload& upload = webServer.upload();
//    if (upload.status == UPLOAD_FILE_START) {
//      Serial.setDebugOutput(true);
//
//#ifdef ARDUINO_ESP8266_ESP01
//      WiFiUDP::stopAll();
//#endif
//
//      Serial.printf("Update: %s\n", upload.filename.c_str());
//#ifdef ARDUINO_ESP8266_ESP01
//      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
//#else
//      uint32_t maxSketchSpace = 0x140000;
//#endif
//
//      if (!Update.begin(maxSketchSpace)) { //start with max available size
//        Update.printError(Serial);
//      }
//    } else if (upload.status == UPLOAD_FILE_WRITE) {
//      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
//        Update.printError(Serial);
//      }
//    } else if (upload.status == UPLOAD_FILE_END) {
//      if (Update.end(true)) { //true to set the size to the current progress
//        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
//      } else {
//        Update.printError(Serial);
//      }
//      Serial.setDebugOutput(false);
//    }
//    yield();
//  });
//
//
//  webServer.begin();                           // Actually start the server
//  Serial.println("HTTP server started");
//  client.setNoDelay(1);
//
//}
//
//void ProcessIOconfig() {
//
//#ifdef ARDUINO_ESP8266_ESP01
//  noInterrupts();
//#endif
//
//  String SerialRXMode = webServer.arg("SerialRXMode");
//  String Telm_Output = webServer.arg("Telm_Output");
//  String Ble_Mode = webServer.arg("Ble_Mode");
//
//  if (SerialRXMode == "PPM") {
//    Serial.println("RC mode set to PPM");
//    SerRXmode = PROTO_RX_NONE;
//  }
//  if (SerialRXMode == "SBUS") {
//    Serial.println("RC mode set to SBUS");
//    SerRXmode = PROTO_RX_SBUS;
//  }
//  if (SerialRXMode == "PPX") {
//    Serial.println("RC mode set to PXX");
//    SerRXmode = PROTO_RX_PXX;
//  }
//  if (SerialRXMode == "CRSF") {
//    Serial.println("RC mode set to CRSF");
//    SerRXmode = PROTO_RX_CRSF;
//  }
//
//  if (Telm_Output == "NONE") {
//    Serial.println("Telemetry mode set to NONE");
//    SerTelmMode = TLM_None;
//  }
//  if (Telm_Output == "SPORT") {
//    Serial.println("Telemetry mode set to SPORT");
//    SerTelmMode = PROTO_TLM_SPORT;
//  }
//  if (Telm_Output == "CRSF") {
//    Serial.println("Telemetry mode set to CRSF");
//    SerTelmMode = PROTO_TLM_CRSF;
//  }
//  if (Telm_Output == "FRSKY") {
//    Serial.println("Telemetry mode set to FRSKY");
//  }
//
//  if (Ble_Mode == "OFF") {
//    Serial.println("Bluetooth mode set to OFF");
//    BleMode = BLE_OFF;
//  }
//  if (Ble_Mode == "Mirror Telemetry") {
//    Serial.println("Bluetooth mode set to Mirror Telemetry");
//    BleMode = BLE_MIRROR_TLM;
//  }
//  if (Ble_Mode == "Serial Debug") {
//    Serial.println("Bluetooth mode set to Serial Debug");
//    BleMode = BLE_SER_DBG;
//  }
//
//  webServer.sendHeader("Connection", "close");
//  File file = SPIFFS.open("/redirect.html", "r");                 // Open it
//  size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
//  file.close();
//
//  SaveEEPROMvars(); //save updated variables to EEPROM
//
//#ifdef ARDUINO_ESP8266_ESP01
//  interrupts();
//#endif
//
//}
//
//void ProcessRFconfig() {
//
//#ifdef ARDUINO_ESP8266_ESP01
//  noInterrupts();
//#endif
//
//  String SerialRXMode = webServer.arg("SerialRXMode");
//  String Telm_Output = webServer.arg("Telm_Output");
//  String Ble_Mode = webServer.arg("Ble_Mode");
//
//  webServer.sendHeader("Connection", "close");
//  File file = SPIFFS.open("/redirect.html", "r");                 // Open it
//  size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
//  file.close();
//
//  SaveEEPROMvars(); //save updated variables to EEPROM
//
//#ifdef ARDUINO_ESP8266_ESP01
//  interrupts();
//#endif
//}
//


//#include <WiFi.h>
//#include <ESPmDNS.h>
//#include <ArduinoOTA.h>
////#include <Hash.h>
//#include <FS.h>
//#include <SPIFFS.h>
//#include <SPIFFSEditor.h>
//#include <AsyncTCP.h>
//#include <ESPAsyncWebServer.h>
//
//
//// SKETCH BEGIN
//AsyncWebServer server(80);
//AsyncWebSocket ws("/ws");
//AsyncEventSource events("/events");

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

const char * hostName = "esp-async";
const char* http_username = "admin";
const char* http_password = "admin";

void initWebSever(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(hostName);

  //Send OTA events to the browser
  ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
  ArduinoOTA.onEnd([]() { events.send("Update End", "ota"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
    else if(error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
    else if(error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
    else if(error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    else if(error == OTA_END_ERROR) events.send("End Failed", "ota");
  });
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  MDNS.addService("http","tcp",80);

  SPIFFS.begin();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(SPIFFS, http_username,http_password));

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
  server.begin();
}

//void loop(){
//  ArduinoOTA.handle();
//}

