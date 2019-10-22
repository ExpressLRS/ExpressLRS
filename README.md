# ExpressLRS
ExpressLRS is an open source RC link for RC aircraft. It is based on SX127x hardware combined with an ESP8285 and ESP32 for RX and TX respectively. 

It can be built with various hardware or customized to suit. The standard build fits in a JR module and the standard RX can be mounted in a 20x20mm stack. 

It can run at 200 Hz, 100 Hz or 50 Hz depending on if you prefer range or low latency. At 200 Hz it is the fastest 900/433 MHz RC link on the market with a Stick -> OpenTX > RF -> RX -> Serial Packet Latency of ~10ms. 

Due to the optimized packet structure only basic telemetry that gives uplink/downlink information is currently supported.

TX and RX modules communicate via the standard CRSF serial protocol for easy use with betaflight and openTX. 


![Hardware Image](img/R9M_and_ExpressLRS_modules.jpg)

![Hardware Image](img/module_inhousing.jpg)

![Hardware Image 1](img/IMG_20181025_210516.jpg)

![Hardware Image 1](img/IMG_20181025_210535.jpg)

