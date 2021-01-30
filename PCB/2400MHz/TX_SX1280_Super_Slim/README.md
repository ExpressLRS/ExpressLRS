# 2.4GHz Slim Tx

### PCB manufacturing

Upload the Gerber file to https://jlcpcb.com/.  Check the price for 5, 10, and 30 pieces.  It is sometimes cheaper to order 30 than 10 and only a minor increase in price compared to 5.

*PCB Thickness: 1mm*
*Remove Order Number: Specify a location*

### BOM

- E28-2G4M27S SX1280 Wireless module 2.4G 27dBm https://www.aliexpress.com/item/33004335921.html
- 3.3V DC-DC Step Down Power Supply https://www.aliexpress.com/item/32880983608.html
- 10k 0805 resistor https://www.aliexpress.com/item/4000049692396.html
- SMA or RPSMA pigtail https://www.aliexpress.com/item/4000848776660.html https://www.aliexpress.com/item/4000848776660.html
- WROOM32 module https://www.aliexpress.com/item/ESP32-ESP-32S-WIFI-Bluetooth-Module-240MHz-Dual-Core-CPU-MCU-Wireless-Network-Board-ESP-WROOM/4000230070560.html
- 10uF 3528 Cap https://www.aliexpress.com/item/32666405364.html?algo_pvid=365ae59d-9e6c-46b7-9792-2656b0961f70&algo_expid=365ae59d-9e6c-46b7-9792-2656b0961f70-6&btsid=0bb0623116027669252885518ea610&ws_ab_test=searchweb0_0,searchweb201602_,searchweb201603_
- 8 Way header https://au.rs-online.com/web/p/sil-sockets/7022852/
- 4 M3 screws 4-7mm (use leftover motor/frame screws)
- 2.4GHz antenna
- A few wires

### Build order

Click this image to watch video of build
[![Build Video](https://github.com/SpencerGraffunder/ExpressLRS/blob/super-slim-pcb/PCB/2400MHz/TX_SX1280_Super_Slim/img/thumbnail.png?raw=true)](https://youtu.be/sNQbWaVPUCc)

0. Print both STLs in the orientation shown in the image below, use light support (15%)
1. Move 0 ohm resistor on E28 to make it use the external antenna
2. Cut trace on voltage regulator and solder 3.3V bridge (see image)
3. Solder ESP32 and E28 to PCB
4. Solder capacitor with stripe away from ESP32
5. Solder two 10k resistors
6. Bridge the boot pads near the E28
7. Solder 4 pin header to ESP32 side of PCB
8. Put voltage regulator on the pin header UPSIDE DOWN and solder
9. Clip extra length of header pins
10. Solder serial to USB adapter wires to R, T, G, and either 3.3V or VBAT (My adapter can't supply enough power through 3.3V so I select 5V and solder VCC to VBAT)
11. Upload your build (if it doesn't work, check for shorts, add flux and heat to all solder joints)
12. Solder wires to G, +, S on E28 side of board (If they're on the ESP side they might get in the way of the board fitting all the way inside the enclosure)
13. Insert antenna pigtail
14. Insert header to back of enclosure (I don't have the correct header so I had to trim mine a bit)
15. Solder wires to the three pins on the right +, G, S from left to right
16. Insert PCB and attach antenna pigtail
17. Screw down cover
18. Make sure LUA script can communicate with module

### STLs

[Onshape link](https://cad.onshape.com/documents/2cffc645d8696d047935ac89/w/6acaaaa832f4b23c1c8ac47e/e/49ad20ba4b7d79ea1d683a18)

### Pics

<img src="https://github.com/SpencerGraffunder/ExpressLRS/blob/super-slim-pcb/PCB/2400MHz/TX_SX1280_Super_Slim/img/printlayout.png?raw=true" width="300">
<img src="https://github.com/SpencerGraffunder/ExpressLRS/blob/super-slim-pcb/PCB/2400MHz/TX_SX1280_Super_Slim/img/antennaswitch.png?raw=true" width="300">
<img src="https://github.com/SpencerGraffunder/ExpressLRS/blob/super-slim-pcb/PCB/2400MHz/TX_SX1280_Super_Slim/img/vreg.png?raw=true" width="300">
<img src="https://github.com/SpencerGraffunder/ExpressLRS/blob/super-slim-pcb/PCB/2400MHz/TX_SX1280_Super_Slim/img/final.png?raw=true" width="300">

### Schematic and PCB layout

<img src="https://github.com/SpencerGraffunder/ExpressLRS/blob/super-slim-pcb/PCB/2400MHz/TX_SX1280_Super_Slim/img/brd.png?raw=true" width="300">
<img src="https://github.com/SpencerGraffunder/ExpressLRS/blob/super-slim-pcb/PCB/2400MHz/TX_SX1280_Super_Slim/img/sch.png?raw=true" width="300">
