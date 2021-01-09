# 2.4 GHz Tx

There are 2 different PCB design based on the G-NiceRF_LoRa1280 (12.5dBm) and EBYTE_E28-2G4M27S (27dBm).

### PCB manufacturing

Upload the Gerber file to https://jlcpcb.com/.  Check the price for 5, 10, and 30 pieces.  It is sometimes cheaper to order 30 than 10 and only a minor increase in price compared to 5.

*PCB Thickness: 1mm*

*Remove Order Number: Specify a location*

### BOM E28-2G4M27S

- E28-2G4M27S SX1280 Wireless module 2.4G 27dBm https://www.aliexpress.com/item/33004335921.html
- ESP32 Development Board https://www.aliexpress.com/item/33057018346.html
- 3.3V DC-DC Step Down Power Supply https://www.aliexpress.com/item/32880983608.html
- 10k 0805 resistor https://www.aliexpress.com/item/4000049692396.html
- SMA or RPSMA connector https://www.aliexpress.com/item/4000848776660.html https://www.aliexpress.com/item/4000848776660.html
- KK 254 PC Board Connector https://www.molex.com/molex/products/part-detail/pcb_receptacles/0022142044 https://www.digikey.com/en/products/detail/molex/0022142044/26523

### Build order

- Solder the 4 pin molex connector.  Cut the pins flush with the PCB before soldering.
- Apply tape to the base of the esp32 board to insulate it from potentially shorting with the molex pins.  Remove the black plastic standoffs.  Place on the PCB and cut the pins flush then solder.
- Set the regulator voltage to 3.45V by rotating the potentiometer on the top fully clockwise, and jumper both the 5V and 12V pads on the back.  Why didn't we just jumper the 3.3V?  Because that also requires cutting the ADJ trace and potentially damaging the regulator.  The e28 will also run better on a voltage slightly above 3.3V.  Now remove the black standoff, insulate the base with tape, then solder in place.

<img src="img/regulator_setup.jpg" width="30%">

- Solder the 10k resistor.
- Solder the e28 module.  Dont forget to change the zero ohm resistor near the ufl.  Default is to use the PCB antenna, it must be repositioned to use the ufl.

<img src="img/1.jpg" width="30%"> <img src="img/2.jpg" width="30%"> <img src="img/3.jpg" width="30%"> <img src="img/5.jpg" width="30%"> <img src="img/6.jpg" width="30%">
<img src="img/7.jpg" width="30%"> <img src="img/8.jpg" width="30%">
