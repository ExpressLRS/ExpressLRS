# 2.4GHz Slim Tx

### PCB manufacturing

Upload the Gerber file to https://jlcpcb.com/.  Check the price for 5, 10, and 30 pieces.  It is sometimes cheaper to order 30 than 10 and only a minor increase in price compared to 5.

*PCB Thickness: 1mm*

### BOM

- E28-2G4M27S SX1280 Wireless module 2.4G 27dBm https://www.aliexpress.com/item/33004335921.html
- 3.3V DC-DC Step Down Power Supply https://www.aliexpress.com/item/32880983608.html
- 10k 0805 resistor https://www.aliexpress.com/item/4000049692396.html
- SMA or RPSMA connector https://www.aliexpress.com/item/4000848776660.html https://www.aliexpress.com/item/4000848776660.html
- WROOM32 module https://www.aliexpress.com/item/ESP32-ESP-32S-WIFI-Bluetooth-Module-240MHz-Dual-Core-CPU-MCU-Wireless-Network-Board-ESP-WROOM/4000230070560.html
- 10uF 3528 Cap https://www.aliexpress.com/item/32666405364.html?algo_pvid=365ae59d-9e6c-46b7-9792-2656b0961f70&algo_expid=365ae59d-9e6c-46b7-9792-2656b0961f70-6&btsid=0bb0623116027669252885518ea610&ws_ab_test=searchweb0_0,searchweb201602_,searchweb201603_
- 8 Way header https://au.rs-online.com/web/p/sil-sockets/7022852/

### Build order

- Solder the e28 module.  Dont forget to change the zero ohm resistor near the ufl.  Default is to use the PCB antenna, it must be repositioned to use the ufl.
- Solder the WROOM32 module
- Solder the 2x 10k resistors
- Solder the capacitor
- Set the regulator voltage to 3.45V by rotating the potentiometer on the top fully clockwise, and jumper both the 5V and 12V pads on the back.  Why didn't we just jumper the 3.3V?  Because that also requires cutting the ADJ trace and potentially damaging the regulator.  The e28 will also run better on a voltage slightly above 3.3V.

<img src="img/regulator_setup.jpg" width="30%">

- Apply tape to the base of the regulator pcb to insulate it from potentially shorting with the vias on the main pcb. Solder on a 4 pin straight header, then remove the plastic bridge on the header so that the reg will stil flush with the main pcb on the bottom. NOTE: pictures show the v1 pcb with reg on the top, which is incorrect. The v1.1 PCB moved the reg to the underside of the main PCB, so it doesn't intrude on the antenna SMA.
- Solder 3x silicon wires to the 3 pin header pads (G, V, S), and attach to the 8 way header using the pinout below, so that G goes to GND, V goes to 6V, and S goes to S.Port.

### STLs

- Print STLs standing on the end. For best results use 0.12 layer thickness, and 40% infill for the base, and 0.12 layer thickness, and 100% infill for the top.
- NOTE: The X-Lite STL is pretty crude. I just extended the mount part of the print out by 10mm so it clears the bay. It could really do with someone designing a proper xlite STL. It's also untested :|

### Build Pics

<img src="img/1.jpg" width="30%"> <img src="img/2.jpg" width="30%"> <img src="img/3.jpg" width="30%">
<img src="img/4.jpg" width="30%"> <img src="img/5.jpg" width="30%"> <img src="img/6.jpg" width="30%">
<img src="img/7.jpg" width="30%"> <img src="img/8.jpg" width="30%"> <img src="img/9.jpg" width="30%">

### Schematic and PCB layout

<img src="img/schm.png" width="30%">
<img src="img/pcb.png" width="30%">

### Flashing 

- Connect an FTDI to the GND, 3v3, TX and RX pins on the left header, then hold the boot pin to ground while powering up, and flash the DIY_2400_TX_ESP32_SX1280_E28_via_UART build in pio

<img src="img/ftdi-wiring" width="30%">
