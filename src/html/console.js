document.addEventListener("DOMContentLoaded", ws_connect, false);
var ws, console = document.getElementById("console");
function ws_connect() {
    if ("WebSocket" in window) {
        var l = window.location; ws = new WebSocket("ws://" + self.location.hostname + "/ws");
        console.value = "connecting...";
        ws.onopen = function (e) { console.value += "connected\n"; }
        ws.onclose = function (e) { console.value += "disconnected\n"; }
        ws.onmessage = function (e) { console.value += e.data; console.scrollTop = console.scrollHeight; }
        ws.onerror = function (e) { console.value += e.data; }
    } else alert("WebSockets not supported on your browser.");
}