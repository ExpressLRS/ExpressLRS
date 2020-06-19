#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include "stm32Updater.h"

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

uint16_t eppromPointer = 0;

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
            websock.onopen = function (evt) { console.log('websock open'); };
            websock.onclose = function(e) {
              console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);
              setTimeout(function() {
                start();
              }, 1000);
            };
            websock.onerror = function (evt) { console.log(evt); };
            websock.onmessage = function (evt) {
                console.log(evt);
                var logger = document.getElementById("logField");
                var date = new Date();
                // match to timezone
                var n=new Date(date.getTime() - (date.getTimezoneOffset() * 60000)).toISOString();
                logger.value += n + ' ' + evt.data + '\n';
                logger.scrollTop = logger.scrollHeight;
            };
        }

        function saveTextAsFile() {
            var textToWrite = document.getElementById('logField').value;
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
    </script>
</head>

<body onload="javascript:start();">
    <center>
        <h1>TX Log Messages</h1>
        The following command can be used to connect to the websocket using curl, which is a lot faster over the terminal than Chrome. <br> Alternatively, you can use the textfield below to view messages.
        <br><br>
        <textarea id="curlCmd" rows="40" cols="100" style="margin: 0px; height: 170px; width: 968px;">
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
    </textarea>
    <br><br>
        <textarea id="logField" rows="40" cols="100" style="margin: 0px; height: 621px; width: 968px;">BEGIN LOG
</textarea>
        <br><br>
        <button type="button" onclick="saveTextAsFile()" value="save" id="save">Save log to file...</button>
        <br><br>
		
	<h1>Danger Zone</h1>
	Update ESP8266 module and R9M Firmware<br><br>
	<b>ESP8266 Flashing:</b><br>
	Select new .bin and press 'Upload and Flash Self'<br>
	<br>
	<b>R9M Flashing:</b><br>
	Select new .bin and press 'Upload and Flash R9M'<br><br><br>
    <form method='POST' action='/update' enctype='multipart/form-data'>
         Self Firmware:
         <input type='file' accept='.bin' name='firmware'>&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp
         <input type='submit' value='Upload and Flash Self'>
    </form>
	<br>
    <form method='POST' action='/upload' enctype='multipart/form-data'>
         R9M Firmware: 
         <input type='file' accept='.bin' name='filesystem'>
         <input type='submit' value='Upload and Flash R9M'>
    </form>
	</center>
	
</body>
</html>
)rawliteral";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);

  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[%u] Disconnected!\r\n", num);
    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
    socketNumber = num;
  }
  break;
  case WStype_TEXT:
    Serial.printf("[%u] get Text: %s\r\n", num, payload);
    // send data to all connected clients
    // webSocket.broadcastTXT(payload, length);
    break;
  case WStype_BIN:
    Serial.printf("[%u] get binary length: %u\r\n", num, length);
    hexdump(payload, length);

    // echo data back to browser
    webSocket.sendBIN(num, payload, length);
    break;
  default:
    Serial.printf("Invalid WStype [%d]\r\n", type);
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

bool flashSTM32()
{
  webSocket.broadcastTXT("Multimodule Firmware Flash Requested!");
  stm32flasher_hardware_init();
  webSocket.broadcastTXT("Going to flash the firmware file: " + uploadedfilename);
  char filename[31];
  uploadedfilename.toCharArray(filename, sizeof(uploadedfilename) + 3);
  bool result = esp8266_spifs_write_file("/firmware.bin");
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
      SPIFFS.remove("/firmware.bin");
      String output;
      output += "Filesystem: used: ";
      output += String(fs_info.usedBytes);
      output += " / free: ";
      output += String(fs_info.totalBytes);
      webSocket.broadcastTXT(output);

      if (fs_info.usedBytes > 0)
      {
        webSocket.broadcastTXT("formatting filesystem");
        SPIFFS.format();
      }
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
    fsUploadFile = SPIFFS.open("/firmware.bin", "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
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
      if (flashSTM32())
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
  IPAddress my_ip;

#ifdef INVERTED_SERIAL
  Serial.begin(115200, SERIAL_8N1, SERIAL_FULL, 1, true); // inverted serial
#else
  Serial.begin(115200); // non-inverted serial
#endif

  SPIFFS.begin();

  wifi_station_set_hostname("elrs_tx");

#ifdef USE_WIFI_MANAGER
  WiFiManager wifiManager;
  Serial.println("Starting ESP WiFiManager captive portal...");
  wifiManager.autoConnect("ESP WiFiManager");
  my_ip = WiFi.localIP();
#else
  Serial.println("Starting ESP softAP...");
  WiFi.softAP(ssid, password);
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started");

  my_ip = WiFi.softAPIP();
#endif

  if (mdns.begin("elrs_tx", my_ip))
  {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else
  {
    Serial.println("MDNS.begin failed");
  }
  Serial.print("Connect to http://elrs_tx.local or http://");
  Serial.println(my_ip);
#ifdef USE_WIFI_MANAGER
  Serial.println(WiFi.localIP());
#else
  Serial.println(WiFi.softAPIP());
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
