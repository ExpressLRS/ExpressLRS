# 20x20 RX PCB

<img src="img/bare_pcb.jpg" width="25%"> <img src="img/populated_side_1.jpg" width="25%"> <img src="img/populated_side_2.jpg" width="25%">

## Features

 * 20x20 footprint
 * 0805 size SMD components (for easy soldering) - NOTE: LED is still 0603 as this size is more common in LEDs
 * Components are placed for ease of soldering (easy iron access)
 * u.fl antenna connector footprint for 915MHz antenna
 * PCB trace antenna for WiFi
 * 5V input (when using the reg), or 3.3V input (bypasses the reg pads) if your FC supplies a decent 3.3V output

## Editing

 * The PCB has been developed in Eagle, and the custom libraries that were used are attached in the /lib directory. Copy the project files into you Eagle project dir, and the libs into your lib dir, then fire up Eagle and edit.

 ## Ordering

 * I've had good results using JLCPCB (https://jlcpcb.com/). Upload the .zip file in this repo and adjust any PCB parameters that you want (defaults work fine).
 * A super-lightweight build can be achieved by going to a 0.6mm PCB thickness (without an increase to cost)