# 2.4 GHz Tx



### PCB manufacturing

Upload the Gerber file to https://jlcpcb.com/

### BOM

Below are links to the more uncommon components.

- E28-2G4M27S SX1280 Wireless module 2.4G 27dBm https://www.aliexpress.com/item/33004335921.html
- ESP32 Development Board https://www.aliexpress.com/item/33057018346.html
- 3.3V DC-DC Step Down Power Supply https://www.aliexpress.com/item/32880983608.html
- 10k 0805 resistor https://www.aliexpress.com/item/4000049692396.html
- SMA or RPSMA connector https://www.aliexpress.com/item/4000848776660.html https://www.aliexpress.com/item/4000848776660.html
- KK 254 PC Board Connector https://www.molex.com/molex/products/part-detail/pcb_receptacles/0022142044

### Build order

- Solder the 4 pin molex connector.  Cut the pins flush with the PCB before soldering.
- Apply tape to the base of the esp32 board to insulate it from potentially shorting with the molex pins.  Remove the black plastic standoffs.  Place on the PCB and cut the pins flush then solder.
- Set the regulator to 3.3V and cut the PCB trace on the regulator for ADJ (red circle on image 4). Remove the black standoff, insulate the base with tape, then solder in place.
- Solder the 10k resistor.
- Solder the e28 module.  Dont forget to change the zero ohm resistor near the ufl.  Default is to use the PCB antenna, it must be repositioned to use the ufl.
