# Mini Rx v1.1

This version contains minor changes to reduce the number of components, makes the PCB smaller, and uses the same voltage regulator as the 2.4GHz Nano Rx.

https://easyeda.com/jyesmith/expresslrs-rx


<img src="img/front.jpg" width="25%"> <img src="img/back.jpg" width="25%">


### PCB manufacturing

Upload the Mini_Rx_v1.1_Gerber.ara file to https://jlcpcb.com/


- PCB Thickness 0.6m. Thicker is ok but the Rx becomes bulky.  Going thinner to 0.4mm costs more!



### Component placement

Most of it should be self explanatory :)

R2 is not required unless you run into boot issues due to the flight controller pulling the pin low during powerup.

Boot jumper pads have been added next to the button as an alternative.  Typically it is easier to use the jumpers for the initial flash, and then wifi updating for future updates.

<img src="img/schematic.png" width="25%"> <img src="img/component_placement.PNG" width="25%">



### Flashing

- Solder the jumper pads or hold the button down while powering up.
- Connect an FTDI adapter.
- Upload target DIY_900_RX_ESP8285_SX127x_via_UART.
