#include "flag_png.h"
#include "main_css.h"

static const char PROGMEM INDEX_HTML[] = R"rawliteral(<!DOCTYPE HTML>
<html>
<head>
<title>Welcome to your ExpressLRS System</title>
<meta charset=utf-8 />
<meta name=viewport content="width=device-width, initial-scale=1" />
<link rel=stylesheet href=main.css />
</head>
<body>
<section id=header>
<div class=inner>
<img src=flag.png height=250 width=250 style=padding:20px></a>
<h1>Welcome to your <b>ExpressLRS</b><br/> update page<br/>
</h1>
<p>From here you can update the firmware on your <b>Reciever</b> module<br/>
<p>
<b>Firmware Rev. </b><var id=FirmVersion>1.0</var><p>
</div>
</section>
<br>
<section id=one class="main style1">
<div class=container>
<div align=left>
<legend><h2>Useful Links and Support:</h2></legend>
<h4><a href=https://github.com/ExpressLRS/ExpressLRS>GitHub Repository</a><h4>
<h4><a href=https://discord.gg/dS6ReFY>Discord Chat</a><h4>
</div>
<br>
<div align=left>
<fieldset>
Here you can update module firmware,
be careful to upload the correct file otherwise a bad flash may occur. If this happens you will need to reflash via USB/Serial.
<br><br>
<legend><h2>Firmware Update:</h2></legend>
<form method=POST action=/update enctype=multipart/form-data> <input type=file name=update> <input type=submit value=Update></form>
</form>
<br><br>
</fieldset>
</div>
<div align=left>
<fieldset>
<legend><h2>Set 'Home' Network:</h2></legend>
<a href=/scanhome>Scan for networks</a>
<br><br>
</fieldset>
</div>
</div>
</section>
</body>
</html>)rawliteral";

static const char PROGMEM SCAN_HTML[] = R"rawliteral(<!DOCTYPE HTML>
<html>
<head>
<title>Welcome to your ExpressLRS System</title>
<meta charset=utf-8 />
<meta name=viewport content="width=device-width, initial-scale=1" />
<link rel=stylesheet href=main.css />
</head>
<body>
<section id=header>
<div class=inner>
<img src=flag.png height=250 width=250 style=padding:20px></a>
<h1>Welcome to your <b>ExpressLRS</b><br/> update page<br/>
</h1>
<p>From here you can set your 'home' network. This will allow you to connect directly to your Receiver directly via a web browser on http://elrs_rx.local<br/>
<p>
<b>Firmware Rev. </b><var id=FirmVersion>1.0</var><p>
</div>
</section>
<br>
<section id=one class="main style1">
<div class=container>
<form action=/sethome id=sethome method=post>
<select name=network id=network>
</select>
<input type=password id=password name=password>
<input type=submit value=Join>
</form>
<div id=failed style=display:none color=red>
Failed to connect to network, please try again.
</div>
</div>
</section>
</body>
<script>document.addEventListener("DOMContentLoaded",get_json_data,false);function get_json_data(){var a="networks.json";xmlhttp=new XMLHttpRequest();xmlhttp.onreadystatechange=function(){if(this.readyState==4&&this.status==200){var b=JSON.parse(this.responseText);append_json(b)}};xmlhttp.open("POST",a,true);xmlhttp.setRequestHeader("Content-type","application/x-www-form-urlencoded");xmlhttp.send()}function append_json(a){var b=document.getElementById("network");b.length=0;a.forEach(function(c){option=document.createElement("option");option.text=c;option.value=c;b.add(option)})}function hasErrorParameter(){var b=[],a=false;location.search.substr(1).split("&").forEach(function(c){b=c.split("=");if(b[0]==="error"){a=true}});return a}function show(c,a){c=c.length?c:[c];for(var b=0;b<c.length;b++){c[b].style.display=a||"block"}}var elements=document.querySelectorAll("#failed");if(hasErrorParameter()){show(elements)};</script>
</html>)rawliteral";
