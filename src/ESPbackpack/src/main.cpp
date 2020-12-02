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
#include "stk500.h"

// reference for spiffs upload https://taillieu.info/index.php/internet-of-things/esp8266/335-esp8266-uploading-files-to-the-server

//#define INVERTED_SERIAL                                  // Comment this out for non-inverted serial
#define USE_WIFI_MANAGER                                 // Comment this out to host an access point rather than use the WiFiManager

const char *ssid = "ExpressLRS Tx";                        // The name of the Wi-Fi network that will be created
const char *password = "expresslrs";                       // The password required to connect to it, leave blank for an open network

MDNSResponder mdns;

ESP8266WebServer server(80);

WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdater;

File fsUploadFile;                                         // a File object to temporarily store the received file
String uploadedfilename;                                   // filename of uploaded file

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

static const char PROGMEM CSS[] = R"rawliteral(a,abbr,acronym,address,applet,article,aside,audio,b,big,blockquote,body,canvas,caption,center,cite,code,dd,del,details,dfn,div,dl,dt,em,embed,fieldset,figcaption,figure,footer,form,h1,h2,h3,h4,h5,h6,header,hgroup,html,i,iframe,img,ins,kbd,label,legend,li,mark,menu,nav,object,ol,output,p,pre,q,ruby,s,samp,section,small,span,strike,strong,sub,summary,sup,table,tbody,td,tfoot,th,thead,time,tr,tt,u,ul,var,video{margin:0;padding:0;border:0;font-size:100%;font:inherit;vertical-align:baseline}article,aside,details,figcaption,figure,footer,header,hgroup,menu,nav,section{display:block}body{line-height:1}ol,ul{list-style:none}blockquote,q{quotes:none}blockquote:after,blockquote:before,q:after,q:before{content:'';content:none}table{border-collapse:collapse;border-spacing:0}body{-webkit-text-size-adjust:none}*,:after,:before{-moz-box-sizing:border-box;-webkit-box-sizing:border-box;box-sizing:border-box}.container{margin-left:auto;margin-right:auto}.container.\31 25\25{width:100%;max-width:75em;min-width:60em}.container.\37 5\25{width:45em}.container.\35 0\25{width:30em}.container.\32 5\25{width:15em}.container{width:60em}@media screen and (max-width:1680px){.container.\31 25\25{width:100%;max-width:75em;min-width:60em}.container.\37 5\25{width:45em}.container.\35 0\25{width:30em}.container.\32 5\25{width:15em}.container{width:60em}}@media screen and (max-width:1140px){.container.\31 25\25{width:100%;max-width:112.5%;min-width:90%}.container.\37 5\25{width:67.5%}.container.\35 0\25{width:45%}.container.\32 5\25{width:22.5%}.container{width:90%}}@media screen and (max-width:980px){.container.\31 25\25{width:100%;max-width:125%;min-width:100%}.container.\37 5\25{width:75%}.container.\35 0\25{width:50%}.container.\32 5\25{width:25%}.container{width:100%!important}}@media screen and (max-width:736px){.container.\31 25\25{width:100%;max-width:125%;min-width:100%}.container.\37 5\25{width:75%}.container.\35 0\25{width:50%}.container.\32 5\25{width:25%}.container{width:100%!important}}@media screen and (max-width:480px){.container.\31 25\25{width:100%;max-width:125%;min-width:100%}.container.\37 5\25{width:75%}.container.\35 0\25{width:50%}.container.\32 5\25{width:25%}.container{width:100%!important}}@media screen and (max-width:320px){.container.\31 25\25{width:100%;max-width:125%;min-width:100%}.container.\37 5\25{width:75%}.container.\35 0\25{width:50%}.container.\32 5\25{width:25%}.container{width:100%!important}}th{text-align:left}body{background:#fff}body.is-loading *,body.is-loading :after,body.is-loading :before{-moz-animation:none!important;-webkit-animation:none!important;-ms-animation:none!important;animation:none!important;-moz-transition:none!important;-webkit-transition:none!important;-ms-transition:none!important;transition:none!important}body,input,select,textarea{color:#666;font-family:"Source Sans Pro",Helvetica,sans-serif;font-size:16pt;font-weight:300;line-height:1.65em}a{-moz-transition:color .2s ease-in-out,border-color .2s ease-in-out;-webkit-transition:color .2s ease-in-out,border-color .2s ease-in-out;-ms-transition:color .2s ease-in-out,border-color .2s ease-in-out;transition:color .2s ease-in-out,border-color .2s ease-in-out;border-bottom:dotted 1px #666;color:inherit;text-decoration:none}a:hover{border-bottom-color:transparent!important;color:#6bd4c8}b,strong{color:#555;font-weight:400}em,i{font-style:italic}p{margin:0 0 2em 0}h1,h2,h3,h4,h5,h6{color:#555;line-height:1em;margin:0 0 1em 0}h1 a,h2 a,h3 a,h4 a,h5 a,h6 a{color:inherit;text-decoration:none}h1{font-size:2.25em;line-height:1.35em}h2{font-size:2em;line-height:1.35em}h3{font-size:1.35em;line-height:1.5em}h4{font-size:1.25em;line-height:1.5em}h5{font-size:.9em;line-height:1.5em}h6{font-size:.7em;line-height:1.5em}sub{font-size:.8em;position:relative;top:.5em}sup{font-size:.8em;position:relative;top:-.5em}hr{border:0;border-bottom:solid 1px rgba(144,144,144,.5);margin:2em 0}hr.major{margin:3em 0}blockquote{border-left:solid 4px rgba(144,144,144,.5);font-style:italic;margin:0 0 2em 0;padding:.5em 0 .5em 2em}code{background:rgba(144,144,144,.075);border-radius:4px;border:solid 1px rgba(144,144,144,.5);font-family:"Courier New",monospace;font-size:.9em;margin:0 .25em;padding:.25em .65em}pre{-webkit-overflow-scrolling:touch;font-family:"Courier New",monospace;font-size:.9em;margin:0 0 2em 0}pre code{display:block;line-height:1.75em;padding:1em 1.5em;overflow-x:auto}.align-left{text-align:left}.align-center{text-align:center}.align-right{text-align:right}#header{padding:9em 0 9em 0;background-color:#4686a0;color:rgba(255,255,255,.75);background-attachment:fixed,fixed,fixed;background-image:linear-gradient(45deg,#9dc66b 5%,#4fa49a 30%,#4361c2);background-position:top left,center center,center center;background-size:auto,cover,cover;overflow:hidden;position:relative;text-align:center}#header b,#header h1,#header h2,#header h3,#header h4,#header h5,#header h6,#header strong{color:#fff}@media screen and (max-width:1680px){body,input,select,textarea{font-size:14pt}#header{padding:6em 0 6em 0}}@media screen and (max-width:1140px){body,input,select,textarea{font-size:13pt}h1 br,h2 br,h3 br,h4 br,h5 br,h6 br{display:none}ul.major-icons li{padding:2em}ul.major-icons li .icon{height:8em;line-height:8em;width:8em}.main{padding:4em 0 2em 0}.main.style2{background-attachment:scroll}#header{padding:5em 0 5em 0;background-attachment:scroll}#header br{display:inline}#footer{padding:4em 0 4em 0;background-attachment:scroll}}@media screen and (max-width:980px){ul.major-icons li{padding:2em}ul.major-icons li .icon{height:7em;line-height:7em;width:7em}.main{padding:5em 3em 3em 3em}#header{padding:8em 3em 8em 3em}#footer{padding:5em 3em 5em 3em}#one{text-align:center}#two{text-align:center}}@media screen and (max-width:736px){body,input,select,textarea{font-size:12pt}h1{font-size:1.75em}h2{font-size:1.5em}h3{font-size:1.1em}h4{font-size:1em}ul.major-icons li{padding:1.5em}ul.major-icons li .icon{height:5em;line-height:5em;width:5em}ul.major-icons li .icon:before{font-size:42px}.icon.major{margin:0 0 1em 0}.main{padding:3em 1.5em 1em 1.5em}#header{padding:4em 3em 4em 3em}#header .actions{margin:2em 0 0 0}#footer{padding:3em 1.5em 3em 1.5em}}@media screen and (max-width:480px){ul.actions{margin:0 0 2em 0}ul.actions li{display:block;padding:1em 0 0 0;text-align:center;width:100%}ul.actions li:first-child{padding-top:0}ul.actions li>*{margin:0!important;width:100%}ul.actions li>.icon:before{margin-left:-2em}ul.actions.small li{padding:.5em 0 0 0}ul.actions.small li:first-child{padding-top:0}.main{padding:2em 1.5em .1em 1.5em}#header{padding:4em 2em 4em 2em}#header br{display:none}#footer{padding:2em 1.5em 2em 1.5em}#footer .copyright{margin:1.5em 0 0 0}#footer .copyright li{border:0;display:block;margin:1em 0 0 0;padding:0}#footer .copyright li:first-child{margin-top:0}}@media screen and (max-width:320px){body,html{min-width:320px}.main{padding:2em 1em .1em 1em}#header{padding:3em 1em 3em 1em}#footer{padding:2em 1em 2em 1em}})rawliteral";

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
<title>Welcome to your ExpressLRS System</title>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<link rel="stylesheet" href="main.css" />
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
var d = new Date();
var n = d.toISOString();
document.getElementById("logField").value += n + ' ' + evt.data + '\n';
document.getElementById("logField").scrollTop = document.getElementById("logField").scrollHeight;
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
downloadLink.href = window.webkitURL.createObjectURL(textFileAsBlob);
} else {
downloadLink.href = window.URL.createObjectURL(textFileAsBlob);
downloadLink.onclick = destroyClickedElement;
downloadLink.style.display = "none";
document.body.appendChild(downloadLink);
}
downloadLink.click();
}
function destroyClickedElement(event) {
document.body.removeChild(event.target);
}
</script>
</head>
<body onload="javascript:start();">
<section id="header">
<div class="inner">
<img src="data:image/svg+xml;base64,PHN2ZyBpZD0iRWJlbmVfMSIgZGF0YS1uYW1lPSJFYmVuZSAxIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxNjguOTltbSIgaGVpZ2h0PSIxNzUuNzJtbSIgdmlld0JveD0iMCAwIDQ3OS4wMiA0OTguMDkiPjxkZWZzPjxzdHlsZT4uY2xzLTF7ZmlsbDojZmZmO308L3N0eWxlPjwvZGVmcz48ZyBpZD0iXzAwMDAwMGZmIiBkYXRhLW5hbWU9IiMwMDAwMDBmZiI+PHBhdGggY2xhc3M9ImNscy0xIiBkPSJNMjM3LjU3LDE0Ljg1YzQ3LjIyLTUsOTYuMjcsMTAuMzQsMTMxLjksNDEuNzksNS4yOCw0LjkyLDEyLjQ4LDkuNDIsMTMuNCwxNy4yMSwxLjM0LDguODItNi44NCwxNy41NC0xNS43OSwxNi40LTgtLjM4LTEyLjMtOC4wNi0xOC4xMS0xMi40MkExNDQsMTQ0LDAsMCwwLDI3NC40LDQ0Ljc0Yy00Mi44MS02LTg4LjEsOC4zLTExOS4xOSwzOC40Ni0zLjQ5LDMuNjctNy44NCw3LjMxLTEzLjIzLDcuMDktNy40Mi40MS0xNC4yNC01LjgzLTE0Ljg4LTEzLjE1YTE1LDE1LDAsMCwxLDQuMTQtMTEuOUMxNTkuNDQsMzYuODQsMTk3LjY3LDE4LjQ5LDIzNy41NywxNC44NVoiIHRyYW5zZm9ybT0idHJhbnNsYXRlKC0xNi40OSAtMTMuOTEpIi8+PHBhdGggY2xhc3M9ImNscy0xIiBkPSJNMjM3LjYyLDYzLjgxYzM5LTUuNjYsODAuNDksOC4yMSwxMDcuNjUsMzYuOTMsNC45MSw1LjY3LDQuMjYsMTUuMTUtMS40NCwyMC4wNWExNC41NiwxNC41NiwwLDAsMS0xOC4xLDEuNDhjLTIuODUtMi4xMi01LjMtNC43LTgtN2E5Ni43LDk2LjcsMCwwLDAtMTI2LjYsMWMtMy41OCwzLjA5LTYuNzgsNy4yMi0xMS43LDguMTUtNy43MywyLjEyLTE2LjQtMy41OC0xNy43Ni0xMS40M2ExNC4zOSwxNC4zOSwwLDAsMSw0LTEzLjI2QTEyNC42NCwxMjQuNjQsMCwwLDEsMjM3LjYyLDYzLjgxWiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0yNDUuNTYsMTExLjc3YTc2Ljg5LDc2Ljg5LDAsMCwxLDY1LjI1LDIzLjE0YzQuODYsNS41Miw0LjU0LDE0LjkxLTEsMTkuODktNS4xMSw1LjUxLTE0LjcxLDUuNzEtMjAuMDguNDdhNDkuNTgsNDkuNTgsMCwwLDAtMjIuNjQtMTMuMDksNDguNTgsNDguNTgsMCwwLDAtNDUuNzYsMTIuMSwxNC43MSwxNC43MSwwLDAsMS0xNSw0LjIxLDE1LjEzLDE1LjEzLDAsMCwxLTEwLTkuOTRjLTEuMjgtNC44Ni0uMjktMTAuNDEsMy4yOC0xNC4wOUE3Ni40NCw3Ni40NCwwLDAsMSwyNDUuNTYsMTExLjc3WiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0yNDIuODYsMTcyLjhhMjQuNzMsMjQuNzMsMCwwLDEsMzcuNzEsMTgsMjUuMDcsMjUuMDcsMCwwLDEtNy43MSwyMWMxLjQzLDkuMzcsMi44OSwxOC43NCw0LjMyLDI4LjExLTUuNTgsMC0xMS4xNSwwLTE2LjczLDAtMS4wNi03LjE5LTIuMi0xNC4zNi0zLjI5LTIxLjU0bC0yLjI4LjEzYy0xLjI0LDcuMTEtMi4yMiwxNC4yNy0zLjM0LDIxLjQxLTUuNTcsMC0xMS4xNSwwLTE2LjcyLDAsMS40My05LjM4LDIuODktMTguNzUsNC4zMi0yOC4xMmEyNS4zMSwyNS4zMSwwLDAsMS03Ljg2LTE2LjgzLDI0Ljc1LDI0Ljc1LDAsMCwxLDExLjU4LTIyLjI1bTkuNDUsMTMuNmMtNS41NSwyLjQ4LTYuMDgsMTEuMDktLjg3LDE0LjIzLDUsMy43LDEyLjk1LS40NSwxMi43OS02LjY5QzI2NC42MSwxODgsMjU3LjUzLDE4My40OSwyNTIuMzEsMTg2LjRaIiB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtMTYuNDkgLTEzLjkxKSIvPjxwYXRoIGNsYXNzPSJjbHMtMSIgZD0iTTg2LDIzMmEzMS42MSwzMS42MSwwLDAsMSw1MC4zMiwzNy4zOGMtMi41LDQuNjEtNi40NSw4LjE2LTEwLjExLDExLjg0UTExNC40OSwyOTMsMTAyLjc0LDMwNC43OEE0MS4yNyw0MS4yNywwLDAsMCw5MC44NSwzMzRjMCwxNC4zLS4wOCwyOC42MS4wNSw0Mi45MS4wNywxMyw4LDIzLjg0LDEzLjIxLDM1LjIyLDEwLjg3LTEwLjgxLDIxLjY0LTIxLjcxLDMyLjU0LTMyLjQ4LDEwLTkuNzksMjYuNjMtMTEuNCwzOC4zNS0zLjg0LDEyLDcuMjMsMTcuOSwyMywxMy40NiwzNi4zMi0xLjc2LDYtNS42NCwxMS0xMC4xMiwxNS4yMnEtMjMuMjgsMjMuMjQtNDYuNTEsNDYuNWMtNS4yOSw1LjM3LTExLDEwLjQ1LTE1LjA4LDE2Ljg1QTYwLjI4LDYwLjI4LDAsMCwwLDEwOC4yLDUxMkg5MS41YTc0LjM2LDc0LjM2LDAsMCwxLDIwLjY4LTQxLjgzcTI3LjE5LTI3LjI3LDU0LjQ1LTU0LjQ2YzMuMDYtMi44OCw2LTYuMzEsNi41NC0xMC42NGExNS4wNywxNS4wNywwLDAsMC0yNS44NC0xMi43NmMtOCw4LTE2LDE2LTI0LDI0LTYuNDUsNi4zMi0xMi40NiwxMy41NC0yMSwxNy4xNy0xMS4zNSw1LjM2LTI0LjE5LDMuMTMtMzYuMjksMy41OXEwLTguMjQsMC0xNi40NmM3Ljg2LS4xNiwxNS43NS4zNywyMy41OS0uMzctMy4xNi02LjQ3LTYuNDYtMTIuODgtOS42Mi0xOS4zNUE1OC4xNyw1OC4xNywwLDAsMSw3NC4zMywzNzVjMC0xNCwwLTI4LDAtNDJBNTcuNjIsNTcuNjIsMCwwLDEsOTAuNiwyOTMuNTljOS41Ny05Ljc2LDE5LjM3LTE5LjMsMjguOTQtMjkuMDZhMTUuMDYsMTUuMDYsMCwwLDAtMTIuNjQtMjUuNDcsMTUuOTIsMTUuOTIsMCwwLDAtOS42OSw1LjE0Yy0xMy4xMywxMy4xOC0yNi4zMiwyNi4yOS0zOS40NCwzOS40OUE1Ny41NSw1Ny41NSwwLDAsMCw0MiwzMTQuOTNjLTIuNTIsMTUuNjYtNC45NSwzMS4zMy03LjQ0LDQ3QTExMC43MSwxMTAuNzEsMCwwLDAsMzMsMzgwcTAsNTUuNDksMCwxMTFhOTEuNjEsOTEuNjEsMCwwLDAsMi4xOCwyMUgxOC4zNmExMDguMzIsMTA4LjMyLDAsMCwxLTEuODUtMjFxMC01NC40NiwwLTEwOC45M2MtLjM3LTE2LjE2LDMuNTQtMzEuOTUsNS43Mi00Ny44NiwyLjM4LTEzLDMtMjYuNTIsOC4zLTM4LjgxQTc1LDc1LDAsMCwxLDQ3LDI3MS4wN1E2Ni41MSwyNTEuNTMsODYsMjMyWiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0zNzIuNDYsMjQ2LjgyYTMxLjYxLDMxLjYxLDAsMCwxLDUzLTE1LjI3UTQ0NS43NCwyNTEuNzcsNDY2LDI3MkE3NC45Miw3NC45MiwwLDAsMSw0ODYuNiwzMTQuMWMyLjU3LDE2LjM3LDUuMTksMzIuNzIsNy43Myw0OS4wOSwxLjUsOS41NSwxLjExLDE5LjIzLDEuMTYsMjguODV2ODRjLS4wOSwxMiwuNTYsMjQuMTMtMS44NiwzNkg0NzYuNzljMi43OS0xMS40NiwyLjEtMjMuMzEsMi4xOC0zNVYzOTNjLS4wNS05LjYuMzUtMTkuMjctMS4yLTI4Ljc5LTIuNjUtMTYuNzctNS4yMy0zMy41NS04LTUwLjNhNTguODIsNTguODIsMCwwLDAtMTgtMzIuN2MtMTIuNy0xMi42NC0yNS4yOS0yNS4zNy0zOC0zOC01LjQ3LTUuNTQtMTUuMjctNS43NS0yMC44MS0uMjEtNS45NCw1LjIxLTYuMzksMTUuMTUtMSwyMC45MkMzOTguMzMsMjcwLjYsNDA1LDI3Nyw0MTEuNSwyODMuNmM1Ljk0LDYuMSwxMi40OSwxMS43NSwxNy4wNSwxOUE1OC4zMiw1OC4zMiwwLDAsMSw0MzcuNjcsMzM1Yy0uMDcsMTQuODUuMTgsMjkuNzItLjEzLDQ0LjU3LS41MiwxNC45My05LjA1LDI3LjYtMTUuMiw0MC42OSw3Ljg0Ljc0LDE1LjczLjIxLDIzLjU5LjM3djE2LjQ2Yy0xMC0uNC0yMC4yLDEuMDgtMzAtMS4zOGE0MS43Niw0MS43NiwwLDAsMS0xOC43OC0xMC45M2MtMTEtMTAuOTUtMjItMjItMzMtMzIuOTUtNS40NS01LjQ3LTE1LjE1LTUuNjctMjAuNjctLjE5LTYuMTksNS4zNS02LjQzLDE1LjczLS42NCwyMS40NiwxOC45NCwxOS4wOSwzOC4wNiwzOCw1Nyw1Ny4wOUE3NC40OSw3NC40OSwwLDAsMSw0MjAuNDksNTEySDQwMy44YTU5LjIyLDU5LjIyLDAsMCwwLTE1LjY2LTMwLjEzYy0xOS4wOC0xOS4yNS0zOC4zNi0zOC4yOS01Ny40Mi01Ny41NC0xMC0xMC4xNS0xMS40Ni0yNy4yNS0zLjQ2LTM5LDcuNTgtMTIsMjMuNjYtMTcuNDEsMzYuOTQtMTIuNDksOC4yMSwyLjYyLDEzLjc1LDkuNDksMTkuNjgsMTUuM3ExMiwxMiwyNCwyNGM1LjE4LTExLjM2LDEzLjEtMjIuMTksMTMuMi0zNS4xNy4xNC0xNC4zMSwwLTI4LjYyLDAtNDIuOTNhNDEuNDEsNDEuNDEsMCwwLDAtMTEuODgtMjkuMjNjLTkuNjItOS43NC0xOS40LTE5LjMzLTI5LTI5LjA4QTMxLjU0LDMxLjU0LDAsMCwxLDM3Mi40NiwyNDYuODJaIiB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtMTYuNDkgLTEzLjkxKSIvPjxwYXRoIGNsYXNzPSJjbHMtMSIgZD0iTTE1Ni45MSwyNDcuMTZIMzU1LjA5cTAsOC4yNSwwLDE2LjUyLTk5LDAtMTk4LjEsMEMxNTYuOCwyNTguMTksMTU2Ljk0LDI1Mi42NywxNTYuOTEsMjQ3LjE2WiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0xNTEuNTEsMjgwLjc5YzExLjUxLTIuODEsMjQuMjcsNCwyOC40MiwxNS4wOWEyNC43NSwyNC43NSwwLDEsMS0yOC40Mi0xNS4wOW0xLjg4LDE2LjcyYy01LjY4LDIuMzctNi4zMywxMS4xLTEuMDgsMTQuMjksNC43NCwzLjUxLDEyLjI1LjA3LDEyLjc2LTUuNzlDMTY2LjE1LDI5OS44NiwxNTguOTMsMjk0LjU3LDE1My4zOSwyOTcuNTFaIiB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtMTYuNDkgLTEzLjkxKSIvPjxwYXRoIGNsYXNzPSJjbHMtMSIgZD0iTTIwNi40NSwyODAuMnE0OS41NSwwLDk5LjEsMCwwLDMzLDAsNjYuMDZIMjA2LjQ2cTAtMzMsMC02Ni4wNk0yMjMsMjk2LjcycTAsMTYuNSwwLDMzaDY2cTAtMTYuNTEsMC0zM1EyNTYsMjk2LjcsMjIzLDI5Ni43MloiIHRyYW5zZm9ybT0idHJhbnNsYXRlKC0xNi40OSAtMTMuOTEpIi8+PHBhdGggY2xhc3M9ImNscy0xIiBkPSJNMzUwLjQ2LDI4MC42NEEyNC43NSwyNC43NSwwLDEsMSwzMzIsMjk2YTI0LjgsMjQuOCwwLDAsMSwxOC40NC0xNS4zN20xLjY3LDE2LjY0Yy01LjcyLDEuOTEtNy4xLDEwLjMzLTIuMzMsMTQsNC4zLDQsMTIuMDcsMS40MywxMy4yOC00LjI3QzM2NSwzMDAuODUsMzU4LDI5NC42NSwzNTIuMTMsMjk3LjI4WiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0xMzYuOSwzNDUuNTVhNTcsNTcsMCwwLDEsMzgtNi42Nyw1Ny43Nyw1Ny43NywwLDAsMSw0Ni43NCw2OC42OGMtNC42NCwyMy0yNC41NCw0Mi00Ny44NCw0NS4zNHEtMS4yMy04LjE2LTIuNDctMTYuMzFhNDEuNjcsNDEuNjcsMCwwLDAsMjcuOC0xNy42LDQxLDQxLDAsMCwwLDYtMzMuMTIsNDEuNDksNDEuNDksMCwwLDAtMjIuNjItMjcuNDYsNDEsNDEsMCwwLDAtMzcsMS4xNSw0MS43Myw0MS43MywwLDAsMC0yMS4yMSwzMGMtNS40My0uOS0xMC44OC0xLjY2LTE2LjMxLTIuNDhBNTguMjMsNTguMjMsMCwwLDEsMTM2LjksMzQ1LjU1WiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0zMTIuNDIsMzQ5LjQ0YTU3LjU3LDU3LjU3LDAsMCwxLDQ4LTkuNzZjMjIuMjYsNS4yNSw0MC4yMywyNC43OCw0My41MSw0Ny40My01LjQ0Ljg0LTEwLjg3LDEuNjktMTYuMzEsMi40N2E0MS43NSw0MS43NSwwLDAsMC0xOS45MS0yOS4yOCw0MSw0MSwwLDAsMC0zNC42My0zLjQsNDEuNDUsNDEuNDUsMCwwLDAtMjQsMjIuMjUsNDEsNDEsMCwwLDAsMS4xNiwzNS41OCw0MS44MSw0MS44MSwwLDAsMCwzMC4zNiwyMS44NWMtLjc5LDUuNDEtMS42OSwxMC44MS0yLjQxLDE2LjI0LTE3LTIuMzktMzIuNTItMTMuMTYtNDEuMTctMjhhNTcuMTIsNTcuMTIsMCwwLDEtNy40MS0zN0E1Ny43NCw1Ny43NCwwLDAsMSwzMTIuNDIsMzQ5LjQ0WiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0yNDcuNzUsMzk1LjgxaDE2LjVxMCw4LjI1LDAsMTYuNTFoLTE2LjVRMjQ3Ljc0LDQwNC4wNywyNDcuNzUsMzk1LjgxWiIgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LjQ5IC0xMy45MSkiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0xOTguMTksNDYxLjg4SDMxMy44MXYzM3EzMywwLDY2LjA2LDBjMCw1LjQxLDAsMTAuODIsMCwxNi4yNC00NC45NS40Ni04OS45MiwwLTEzNC44OC4yMS0yOC42Ny0uMS01Ny4zNCwwLTg2LS4wNS04Ljk1LS4yLTE3LjkyLjQ0LTI2Ljg1LS4yNCwwLTUuMzksMC0xMC43OCwwLTE2LjE2cTMzLDAsNjYuMDYsMHYtMzNtMTYuNTIsMTYuNTJjMCw1LjUsMCwxMSwwLDE2LjVoODIuNTZjMC01LjUsMC0xMSwwLTE2LjVRMjU2LDQ3OC4zNywyMTQuNzEsNDc4LjRaIiB0cmFuc2Zvcm09InRyYW5zbGF0ZSgtMTYuNDkgLTEzLjkxKSIvPjwvZz48L3N2Zz4=" height="250" width="250" style="padding:20px;"></a>
<h1>Welcome to your <b>ExpressLRS</b><br/> update page</h1>
<p>From here you can update the firmwares on your R9M Tx module and WiFi Backpack module</p>
<p><b>Firmware Rev.  </b><var id="FirmVersion">1.0</var></p>
</div>
</section>
<br>
<section id="one" class="main style1">
<div class="container">
<div align="left">
<fieldset>
<legend>
<h2>Firmware Update Status:</h2>
</legend>
<div>Use the following command to connect the websocket using 'curl', which is a lot faster over the terminal than browser:</div>
<textarea id="curlCmd" rows="24" cols="80" style="margin: 0px; height: 270px; width: 350px;background-color: #252525;Color: #C5C5C5;border-radius: 5px;border: none;font-size: 11pt;">
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
<div>Alternatively, you can use the log area below to view messages:</div>
<textarea id="logField" rows="24" cols="80" style="font-size: 11pt;">BEGIN LOG</textarea><br/>
<button type="button" onclick="saveTextAsFile()" value="save" id="save">Save log to file</button>
</fieldset>
</div>
<hr>
<div align="left">
<fieldset>
<legend>
<h2>R9M Tx Firmware Update:</h2>
</legend>
<form method='POST' action='/upload' enctype='multipart/form-data'>
<input type='file' accept='.bin,.elrs' name='filesystem'>
<input type='submit' value='Upload and Flash R9M Tx'>
</form>
<div style="color:red;">CAUTION! Be careful to upload the correct firmware file, otherwise a bad flash may occur! If this happens you will need to re-flash the module's firmware via USB/Serial.</div>
</fieldset>
</div>
<hr>
<div align="left">
<fieldset>
<legend>
<h2>WiFi Backpack Firmware Update:</h2>
</legend>
<form method='POST' action='/update' enctype='multipart/form-data'>
<input type='file' accept='.bin' name='firmware'>
<input type='submit' value='Flash WiFi Backpack'>
</form>
<div style="color:red;">CAUTION! Be careful to upload the correct firmware file, otherwise a bad flash may occur! If this happens you will need to re-flash the module's firmware via USB/Serial.</div>
</fieldset>
</div>
<hr>
<div align="left">
<legend>
<h2>Useful Links and Support:</h2>
</legend>
<h4><a href="https://github.com/AlessandroAU/ExpressLRS">GitHub Repository</a><h4>
<h4><a href="https://discord.gg/dS6ReFY">Discord Chat</a><h4>
</div>
</div>
</section>
</body>
</html>
)rawliteral";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);

  switch (type)
  {
    case WStype_DISCONNECTED:
    {
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    }

    case WStype_CONNECTED:
    {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      socketNumber = num;
      break;
    }

    case WStype_TEXT:
    {
      Serial.printf("[%u] get text: %s\r\n", num, payload);
      // send data to all connected clients
      // webSocket.broadcastTXT(payload, length);
      break;
    }

    case WStype_BIN:
    {
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    }

    default:
    {
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
    }
  }
}

void sendReturn()
{
  server.send_P(200, "text/html", GO_BACK);
}

void sendMainCss()
{
  server.send_P(200, "text/css", CSS);
}

void handleRoot()
{
  server.send_P(200, "text/html", INDEX_HTML);
}

bool flashSTM32()
{
  bool result = 0;
  webSocket.broadcastTXT("Firmware Flash Requested!");
  webSocket.broadcastTXT("  the firmware file: '" + uploadedfilename + "'");
  if (uploadedfilename.endsWith(".elrs"))
  {
    result = stk500_write_file(uploadedfilename.c_str());
  }
  else if (uploadedfilename.endsWith(".bin"))
  {
    stm32flasher_hardware_init();
    result = esp8266_spifs_write_file(uploadedfilename.c_str());
  }
  Serial.begin(460800);
  return result;
}
void handleFileUpload()
{ // upload a new file to the SPIFFS
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    if (uploadedfilename.length() && SPIFFS.exists(uploadedfilename))
    {
      SPIFFS.remove(uploadedfilename);
    }

    FSInfo fs_info;
    if (SPIFFS.info(fs_info))
    {
      String output = "Filesystem: used: ";
      output += fs_info.usedBytes;
      output += " / free: ";
      output += fs_info.totalBytes;
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
    fsUploadFile = SPIFFS.open(uploadedfilename, "w");      // Open the file for writing in SPIFFS (create if it doesn't exist)
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (fsUploadFile)
    {
      fsUploadFile.write(upload.buf, upload.currentSize);  // Write the received bytes to the file
      String output = "Uploaded: ";
      output += fsUploadFile.position();
      output += " bytes";
      webSocket.broadcastTXT(output);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)                                      // If the file was successfully created
    {
      size_t file_pos = fsUploadFile.position();
      String totsize = "Total uploaded size ";
      totsize += file_pos;
      totsize += " of ";
      totsize += upload.totalSize;
      webSocket.broadcastTXT(totsize);
      server.send(100);

      fsUploadFile.close(); // Close the file again

      bool success = false;

      /* Make sure the all data is received */
      if (file_pos == upload.totalSize)
        success = flashSTM32();

      server.sendHeader("Location", "/return");          // Redirect the client to the success page
      server.send(303);
      webSocket.broadcastTXT(
        (success) ? "Update Successful!": "Update Failure!");

      SPIFFS.remove(uploadedfilename);
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
  Serial.begin(460800, SERIAL_8N1, SERIAL_FULL, 1, true);  // inverted serial
#else
  Serial.begin(460800);                                    // non-inverted serial
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
  server.on("/main.css", sendMainCss);

  server.on(
      "/upload", HTTP_POST,                                // if the client posts to the upload page
      []() { server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
      handleFileUpload                                     // Receive and save the file
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
