#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ESP8266HTTPUpdateServer.h>
#include "stm32Updater.h"
#include "elrs_eeprom.h"

#include "FS.h"

//reference for spiffs upload https://taillieu.info/index.php/internet-of-things/esp8266/335-esp8266-uploading-files-to-the-server

//#define INVERTED_SERIAL // Comment this out for non-inverted serial
#define USE_WIFI_MANAGER // Comment this out to host an access point rather than use the WiFiManager

const char *ssid = "ESP8266 Access Point"; // The name of the Wi-Fi network that will be created
const char *password = "password";         // The password required to connect to it, leave blank for an open network

MDNSResponder mdns;

ESP8266WebServer server(80);

WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdater;

File fsUploadFile; // a File object to temporarily store the received file
FSInfo fs_info;
String uploadedfilename; // filename of uploaded file

uint32_t TotalUploadedBytes;

uint8_t socketNumber;

String inputString = "";

ELRS_EEPROM eeprom;
uint16_t eppromPointer = 0;

bool webUpdateMode = false;

static const char PROGMEM GO_BACK[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
</head>
<body>
<script>
javascript:history.back();
</script>
</body>
</html>
)rawliteral";

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>

<head>
    <meta name="viewport" content="width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
    <title>TX Log Messages</title>
    <style>
        body {
            background-color: #1E1E1E;
            font-family: Arial, Helvetica, Sans-Serif;
            Color: #69cbf7;
        }

        textarea {
            background-color: #252525;
            Color: #C5C5C5;
            border-radius: 5px;
            border: none;
        }
    </style>
    <script>
        var websock;
        function start() {
            document.getElementById("logField").scrollTop = document.getElementById("logField").scrollHeight;
            websock = new WebSocket('ws://' + window.location.hostname + ':81/');
            websock.onopen = function (evt) {
              console.log('websock open');
              //setting_send('S_power');
              //setting_send('S_telemetry');
              //setting_send('S_rate');
            };
            websock.onclose = function(e) {
              console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);
              setTimeout(function() {
                start();
              }, 1000);
            };
            websock.onerror = function (evt) { console.log(evt); };
            websock.onmessage = function (evt) {
                //console.log(evt);
                var text = evt.data;
                if (text.startsWith("ELRS_setting_")) {
                  var res = text.replace("ELRS_setting_", "");
                  res = res.split("=");
                  setting_set(res[0], res[1]);
                } else {
                  var logger = document.getElementById("logField");
                  var date = new Date();
                  var n=new Date(date.getTime() - (date.getTimezoneOffset() * 60000)).toISOString();
                  logger.value += n + ' ' + text + '\n';
                  logger.scrollTop = logger.scrollHeight;
                }
            };
        }

        function saveTextAsFile() {
            var textToWrite = document.getElementById('logField').innerHTML;
            var textFileAsBlob = new Blob([textToWrite], { type: 'text/plain' });
            var fileNameToSaveAs = "tx_log.txt";

            var downloadLink = document.createElement("a");
            downloadLink.download = fileNameToSaveAs;
            downloadLink.innerHTML = "Download File";
            if (window.webkitURL != null) {
                // Chrome allows the link to be clicked without actually adding it to the DOM.
                downloadLink.href = window.webkitURL.createObjectURL(textFileAsBlob);
            } else {
                // Firefox requires the link to be added to the DOM before it can be clicked.
                downloadLink.href = window.URL.createObjectURL(textFileAsBlob);
                downloadLink.onclick = destroyClickedElement;
                downloadLink.style.display = "none";
                document.body.appendChild(downloadLink);
            }

            downloadLink.click();
        }

        function destroyClickedElement(event) {
            // remove the link from the DOM
            document.body.removeChild(event.target);
        }

        function setting_set(type, value) {
          var elem = document.getElementById(type);
          if (elem)
            elem.selectedIndex = [...elem.options].findIndex (option => option.value === value);
        }

        function setting_send(type, elem=null) {
          if (elem) {
            websock.send(type + "=" + elem.value);
          } else {
            websock.send(type + "?");
          }
        }

    </script>
</head>

<body onload="javascript:start();">
  <center>
    <h2>TX Log Messages</h2>
    <textarea id="logField" rows="40" cols="100" style="margin: 0px; height: 621px; width: 968px;"></textarea>
    <br>
    <button type="button" onclick="saveTextAsFile()" value="save" id="save">Save log to file...</button>
    <hr/>
    <h2>Settings</h2>
    <table><tr>
      <td style="padding: 1px 20px 1px 1px;">
        Rate:
        <select name="rate" onchange="setting_send('S_rate', this)" id="rates_input">
          <option value="0">200Hz</option>
          <option value="1">100Hz</option>
          <option value="2">50Hz</option>
        </select>
      </td>

      <td style="padding: 1px 20px 1px 20px;">
        Power:
        <select name="power" onchange="setting_send('S_power', this)" id="power_input">
          <option value="0">Dynamic</option>
          <option value="1">10mW</option>
          <option value="2">25mW</option>
          <option value="3">50mW</option>
          <option value="4">100mW</option>
          <option value="5">250mW</option>
          <option value="6">500mW</option>
          <option value="7">1000mW</option>
          <option value="8">2000mW</option>
          <option value="ff">Reset</option>
        </select>
      </td>

      <td style="padding: 1px 1px 1px 20px;">
        Telemetry:
        <select name="telemetry" onchange="setting_send('S_telemetry', this)" id="tlm_input">
          <option value="0">Off</option>
          <option value="1">1/128</option>
          <option value="2">1/64</option>
          <option value="3">1/32</option>
          <option value="4">1/16</option>
          <option value="5">1/8</option>
          <option value="6">1/4</option>
          <option value="7">1/2</option>
          <option value="ff">Reset</option>
        </select>
      </td>
    </tr></table>

    <hr/>
	  <h2>Danger Zone</h2>
    <div>
      <form method='POST' action='/update' enctype='multipart/form-data'>
          Self Firmware:
          <input type='file' accept='.bin' name='firmware'>
          <input type='submit' value='Upload and Flash Self'>
      </form>
    </div>
    <br>
    <div>
      <form method='POST' action='/upload' enctype='multipart/form-data'>
          R9M Firmware:
          <input type='file' accept='.bin' name='filesystem'>
          <input type='submit' value='Upload and Flash R9M'>
      </form>
    </div>

  </center>
  <hr/>
  <pre>
The following command can be used to connect to the websocket using curl, which is a lot faster over the terminal than Chrome.

curl --include \
     --output - \
     --no-buffer \
     --header "Connection: Upgrade" \
     --header "Upgrade: websocket" \
     --header "Host: example.com:80" \
     --header "Origin: http://example.com:80" \
     --header "Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==" \
     --header "Sec-WebSocket-Version: 13" \
     http://elrs_tx.local:81/
  </pre>
</body>
</html>
)rawliteral";

static uint8_t settings_rate = 1;
static uint8_t settings_power = 4;
static uint8_t settings_tlm = 7;
String settings_out;

void handleSettingRate(const char * input, uint8_t num)
{
  settings_out = "[INTERNAL ERROR] something went wrong";
  if (input == NULL || *input == '?') {
    settings_out = "ELRS_setting_rates_input=";
    settings_out += settings_rate;
  } else if (*input == '=') {
    input++;
    settings_out = "Rate: ";
    settings_out += input;
  }
  webSocket.sendTXT(num, settings_out);
}

void handleSettingPower(const char * input, uint8_t num)
{
  settings_out = "[INTERNAL ERROR] something went wrong";
  if (input == NULL || *input == '?') {
    settings_out = "ELRS_setting_power_input=";
    settings_out += settings_power;
  } else if (*input == '=') {
    input++;
    settings_out = "Power: ";
    settings_out += input;
  }
  webSocket.sendTXT(num, settings_out);
}

void handleSettingTlm(const char * input, uint8_t num)
{
  settings_out = "[INTERNAL ERROR] something went wrong";
  if (input == NULL || *input == '?') {
    settings_out = "ELRS_setting_tlm_input=";
    settings_out += settings_tlm;
  } else if (*input == '=') {
    input++;
    settings_out = "Telemetry: ";
    settings_out += input;
  }
  webSocket.sendTXT(num, settings_out);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  char * temp;
  //Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);

  switch (type)
  {
  case WStype_DISCONNECTED:
    //Serial.printf("[%u] Disconnected!\r\n", num);
    break;
  case WStype_CONNECTED:
  {
    //IPAddress ip = webSocket.remoteIP(num);
    //Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
    socketNumber = num;

    String info = "My IP address = ";
#ifdef USE_WIFI_MANAGER
    info += WiFi.localIP().toString();
#else
    info += WiFi.softAPIP().toString();
#endif
    info += "\n";
    webSocket.sendTXT(num, info);

    // Send settings
    handleSettingRate(NULL, num);
    handleSettingPower(NULL, num);
    handleSettingTlm(NULL, num);
  }
  break;
  case WStype_TEXT:
    //Serial.printf("[%u] get Text: %s\r\n", num, payload);
    // send data to all connected clients
    //webSocket.broadcastTXT(payload, length);

    temp = strstr((char*)payload, "S_rate");
    if (temp) {
      handleSettingRate(&temp[6], num);
      break;
    }
    temp = strstr((char*)payload, "S_power");
    if (temp) {
      handleSettingPower(&temp[7], num);
      break;
    }
    temp = strstr((char*)payload, "S_telemetry");
    if (temp) {
      handleSettingTlm(&temp[11], num);
    }

    break;
  case WStype_BIN:
    //Serial.printf("[%u] get binary length: %u\r\n", num, length);
    hexdump(payload, length);

    // echo data back to browser
    webSocket.sendBIN(num, payload, length);
    break;
  default:
    //Serial.printf("Invalid WStype [%d]\r\n", type);
    webSocket.broadcastTXT("Invalid WStype: " + type);
    break;
  }
}

void sendReturn()
{
  server.send_P(200, "text/html", GO_BACK);
}

void handleRoot()
{
  server.send_P(200, "text/html", INDEX_HTML);
}


bool flashR9M()
{
  webSocket.broadcastTXT("R9M Firmware Flash Requested!");
  stm32flasher_hardware_init();
  webSocket.broadcastTXT("Going to flash the following file: " + uploadedfilename);
  bool result = esp8266_spifs_write_file(uploadedfilename.c_str());
  if (result == 0)
    reset_stm32_to_app_mode(); // boot into app
  Serial.begin(460800);
  return result;
}

void handleFileUpload()
{ // upload a new file to the SPIFFS
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {

    if (SPIFFS.info(fs_info))
    {
      String output;
      output += "Filesystem: used: ";
      output += String(fs_info.usedBytes);
      output += " / free: ";
      output += String(fs_info.totalBytes);
      webSocket.broadcastTXT(output);
    }
    else
    {
      webSocket.broadcastTXT("SPIFFs Failed to init!");
      return;
    }
    uploadedfilename = upload.filename;

    webSocket.broadcastTXT("Uploading file: " + uploadedfilename);

    if (!uploadedfilename.startsWith("/"))
    {
      uploadedfilename = "/" + uploadedfilename;
    }
    fsUploadFile = SPIFFS.open(uploadedfilename, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (fsUploadFile)
    {
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
      TotalUploadedBytes += int(upload.currentSize);
      String output = "";
      output += "Uploaded: ";
      output += String(TotalUploadedBytes);
      output += " bytes";
      webSocket.broadcastTXT(output);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)
    {                       // If the file was successfully created
      fsUploadFile.close(); // Close the file again
      String totsize = String(upload.totalSize);
      webSocket.broadcastTXT("Total uploaded size: " + totsize);
      TotalUploadedBytes = 0;
      server.send(100);
      if (flashR9M())
      {
        server.sendHeader("Location", "/return"); // Redirect the client to the success page
        server.send(303);
        webSocket.broadcastTXT("Update Sucess!!!");
      }
      else
      {
        server.sendHeader("Location", "/return"); // Redirect the client to the success page
        server.send(303);
        webSocket.broadcastTXT("Update Failed!!!");
      }
    }
    else
    {
      server.send(500, "text/plain", "500: couldn't create file");
      SPIFFS.format();
    }
  }
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup()
{

#ifdef INVERTED_SERIAL
  Serial.begin(460800, SERIAL_8N1, SERIAL_FULL, 1, true); // inverted serial
#else
  Serial.begin(460800); // non-inverted serial
#endif

  SPIFFS.begin();

  wifi_station_set_hostname("elrs_tx");

#ifdef USE_WIFI_MANAGER
  WiFiManager wifiManager;
  //Serial.println("Starting ESP WiFiManager captive portal...");
  wifiManager.autoConnect("ESP WiFiManager");
#else
  //Serial.println("Starting ESP softAP...");
  WiFi.softAP(ssid, password);
  //Serial.print("Access Point \"");
  //Serial.print(ssid);
  //Serial.println("\" started");

  //Serial.print("IP address:\t");
  //Serial.println(WiFi.softAPIP());
#endif

  if (mdns.begin("elrs_tx", WiFi.localIP()))
  {
    //Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else
  {
    //Serial.println("MDNS.begin failed");
  }

  //Serial.print("Connect to http://elrs_tx.local or http://");
#ifdef USE_WIFI_MANAGER
  //Serial.println(WiFi.localIP());
#else
  //Serial.println(WiFi.softAPIP());
#endif

  server.on("/", handleRoot);
  server.on("/return", sendReturn);

  server.on(
      "/upload", HTTP_POST,       // if the client posts to the upload page
      []() { server.send(200); }, // Send status 200 (OK) to tell the client we are ready to receive
      handleFileUpload            // Receive and save the file
  );

  server.onNotFound(handleRoot);
  httpUpdater.setup(&server);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

int serialEvent()
{
  char inChar;
  while (Serial.available())
  {
    inChar = (char)Serial.read();
    if (inChar == '\r') {
      continue;
    } else if (inChar == '\n') {
      if (0 <= inputString.indexOf("400000"))
        Serial.write("TEST\n");
      return 0;
    }
    inputString += inChar;
  }
  return -1;
}

void loop()
{
  if (0 <= serialEvent())
  {
    webSocket.broadcastTXT(inputString);
    inputString = "";
  }

  server.handleClient();
  webSocket.loop();
  mdns.update();
}
