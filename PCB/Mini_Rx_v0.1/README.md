# Mini Rx v0.1

https://easyeda.com/jyesmith/expresslrs-rx

<img src="img/pcb.jpg" width="25%"> <img src="img/antenna.jpg" width="25%"> <img src="img/front.jpg" width="25%"> <img src="img/back.jpg" width="25%">

### PCB manufacture

Upload the Mini_Rx_v0.1_Gerber.zip file to https://jlcpcb.com/
- PCB Thickness 0.6m. Thicker is ok but the Rx becomes bulky.  Going thinner to 0.4mm costs more!
- PCB Colour blue... Blue is best, but I suppose you could select something different.

### Component placement

Most of it should be self explanatory except for a couple of the resistors.  

Note - The WiFi antenna isn't required. A small 1/8th wavelength PCB trace was included and tested up to a distance of 5m.  It works well for flashing on the bench or in the field.

<img src="img/Mini_Rx_v0.1_pcb_image.png" width="25%"> <img src="img/Mini_Rx_v0.1_pcb_image_labelled.png" width="25%"> <img src="Mini_Rx_v0.1_Schematic.png" width="25%">

### Minimum flashing requirements

ESP-01F, button (hold while powering to put into boot mode) and 2 x 10k resistors is all that is need to flash the esp. **The red power wire is 3.3V**.

<img src="img/minimum_flash_components.jpg" width="25%">
