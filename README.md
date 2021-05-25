![Banner](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/banner.png)

## Need help? Confused? Join the Community!
 * [<img src="https://camo.githubusercontent.com/79fcdc7c43f1a1d7c175827976ffee8177814a016fb1b9578ff70f1aef759578/68747470733a2f2f6564656e742e6769746875622e696f2f537570657254696e7949636f6e732f696d616765732f7376672f646973636f72642e737667" width="15" height="15"> Community Discord](https://discord.gg/dS6ReFY)
 * [<img src="https://camo.githubusercontent.com/8f245234577766478eaf3ee72b0615e99bb9ef3eaa56e1c37f75692811181d5c/68747470733a2f2f6564656e742e6769746875622e696f2f537570657254696e7949636f6e732f696d616765732f7376672f66616365626f6f6b2e737667" width="15" height="15"> Facebook Group](https://www.facebook.com/groups/636441730280366)
 * [<img src="https://camo.githubusercontent.com/b079fe922f00c4b86f1b724fbc2e8141c468794ce8adbc9b7456e5e1ad09c622/68747470733a2f2f6564656e742e6769746875622e696f2f537570657254696e7949636f6e732f696d616765732f7376672f6769746875622e737667" width="15" height="15"> Wiki](https://github.com/ExpressLRS/ExpressLRS/wiki)

## Support ExpressLRS
If you would like to support the development of ExpressLRS please feel free to make a small donation. This helps us buy hardware for porting, development and prototyping. Show your support for which new features you want added by leaving a message when you donate<br/><br/>
[![Donate](https://img.shields.io/badge/Donate-PayPal-253B80.svg)](https://www.paypal.com/donate?hosted_button_id=FLHGG9DAFYQZU)

## Quick Start Guide
If you have hardware that you want to flash, please refer to our guides on the [wiki](https://github.com/ExpressLRS/ExpressLRS/wiki/), and our [FAQ](https://github.com/ExpressLRS/ExpressLRS/wiki/FAQ)

## High Performance LoRa Radio Link

![Build Status](https://github.com/ExpressLRS/ExpressLRS/workflows/Build%20ExpressLRS/badge.svg)
![Release](https://img.shields.io/github/v/release/ExpressLRS/ExpressLRS?include_prereleases)
![License](https://img.shields.io/github/license/ExpressLRS/ExpressLRS)
![Stars](https://img.shields.io/github/stars/ExpressLRS/ExpressLRS)
![Chat](https://img.shields.io/discord/596350022191415318)

ExpressLRS is an open source RC link for RC applications. It is based on the fantastic semtech **SX127x**/**SX1280** hardware combined with an **ESP8285**, **ESP32** or **STM32**. ExpressLRS supports a wide range of hardware platforms as well as both `900 MHz` and `2.4 GHz` frequency options. ExpressLRS uses **LoRa** modulation as well as reduced packet size to achieve **best in class range and latency** compared to current commercial offerings.

ExpressLRS can run at various packet rates, up to `500hz` or down to `25hz` depending on your preference of range or low latency. At `900 MHz` a maximum of `200 Hz` packet rate is supported. At `2.4 GHz` a blistering `500 Hz` is currently supported with a custom openTX binary with future plans to extend this to `1000 Hz`.

ExpressLRS can be flashed into existing **Frsky R9M hardware (RX and TX)**, **Jumper R900 RXs**, **SiYi FM30 Hardware (Rx and TX)**, **GHOST hardware (RX and TX)** or **Custom PCBs** can be made if you enjoy tinkering. Happy Model released official ExpressLRS hardware (RX and TX) and several other manufacturers are preparing to offer offical ELRS hardware soon so stay tuned.

![LatencyChart](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/Average%20Total%20Latency.png)

ExpressLRS aims to achieve the best possible link preformance for both latency and range. This is achieved with an optimised over the air packet structure.  However, only basic telemetry is currently provided (**VBAT**, downlink/uplink **LQ** and downlink/uplink **RSSI**), work is underway for full telemetry support. This compromise allows ExpressLRS to achieve simultaneous **better latency AND range** compared to other options in the market. For example, **ExpressLRS 2.4GHz 150Hz** mode offers the same range as **GHST Normal** while delivering near **triple** the packet update rate. Similarly, **ExpressLRS 900MHz 200Hz** will dramatically out-range **Crossfire 150Hz** and **ExpressLRS 50Hz** will out-range **Crossfire 50Hz** watt per watt.

**2.4GHz Comparison**
![RangeVsPacketRate](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/pktrate_vs_sens.png)

More information can be found in the [wiki](https://github.com/ExpressLRS/ExpressLRS/wiki).

## Starting Out

After taking a look at the [supported Hardware](https://github.com/ExpressLRS/ExpressLRS/wiki/Supported-Hardware) and making sure you have the required hardware, the [Quick Start Guide](https://github.com/ExpressLRS/ExpressLRS/wiki/Toolchain-and-Git-Setup) is written to walk through the process of flashing ELRS for the first time


## Supported Hardware

### 900 MHz Hardware:

<img src="https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/900Mhardware.jpg" width = "80%">

- **TX**
    - [FrSky R9M (2018)](https://www.frsky-rc.com/product/r9m/) (Full Support, requires resistor mod)
    - [FrSky R9M (2019)](https://www.frsky-rc.com/product/r9m-2019/) (Full Support, no mod required)
    - [FrSky R9M Lite](https://www.frsky-rc.com/product/r9m-lite/) (Full Support, power limited)
    - [TTGO LoRa V1/V2](http://www.lilygo.cn/products.aspx?TypeId=50003&fid=t3:50003:3) (Full Support, V2 recommended w/50 mW power limit)
    - [Namimno Voyager 900 TX](http://www.namimno.com/product.html) (Full Support off the shelf)
    - [HappyModel ES915TX](http://www.happymodel.cn/index.php/2021/02/19/expresslrs-module-es915tx-long-range-915mhz-transmitter-and-es915rx-receiver/) (Full Support off the shelf)
    - DIY Module (Full Support, 50mW limit, limited documentation)
- **RX**
    - [FrSky R9mm](https://www.frsky-rc.com/product/r9-mm-ota/) (Full Support, OTA version can be used)
    - [FrSky R9 Mini](https://www.frsky-rc.com/product/r9-mini-ota/) (Full Support, OTA version can be used)
    - [FrSky R9mx](https://www.frsky-rc.com/product/r9-mx/) (Full Support)
    - [FrSky R9 Slim+](https://www.frsky-rc.com/product/r9-slim-ota/) (Full Support, OTA version can be used)
    - [Jumper R900 mini](https://www.jumper-b2b.com/jumper-r900-mini-receiver-900mhz-long-range-rx-p0083.html) (Full Support, only flashable via STLink, Bad Stock antenna)
    - [DIY mini RX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/900MHz/RX_Mini_v1.1) (Full Support, supports WiFi Updates)
    - [DIY 20x20 RX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/900MHz/RX_20x20_0805_SMD) (Full Support, supports WiFi Updates)
     - [HappyModel ES915RX](http://www.happymodel.cn/index.php/2021/02/19/expresslrs-module-es915tx-long-range-915mhz-transmitter-and-es915rx-receiver/) (Full Support off the shelf)
    - [Namimno Voyager 900 RX](http://www.namimno.com/product.html) (Full Support off the shelf)

### 2.4 GHz Hardware:

<img src="https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/24Ghardware.jpg" width = "80%">

- **TX**
    - [DIY JR Bay](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/2400MHz/TX_SX1280) (Full Support, 27dBm, supports WiFi Updates)
    - [DIY Slim TX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/2400MHz/TX_SX1280_Slim) (Full Support, 27dBm, supports Wifi Updates, fits Slim Bay)
    - [DIY Slimmer TX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/2400MHz/TX_SX1280_Slimmer) (Full Support, 27dBm, supports Wifi Updates, fits Slim Bay)
    - [GHOST TX](https://www.immersionrc.com/fpv-products/ghost/) (Full Support, 250 mW output power, OLED support in ELRS v1.1)
    - [GHOST TX Lite](https://www.immersionrc.com/fpv-products/ghost/) (Full Support, 250 mW output power, OLED support in ELRS v1.1)
    - [HappyModel TX](http://www.happymodel.cn/index.php/2021/04/12/happymodel-2-4g-expresslrs-elrs-micro-tx-module-es24tx/) (Full Support, 250 mW output power)
- **RX**
    - [GHOST Atto](https://www.immersionrc.com/fpv-products/ghost/) (Full Support, Initial flashing with STLINK then both STLINK and BF passthrough)
    - [GHOST Zepto](https://www.immersionrc.com/fpv-products/ghost/) (Full Support, Initial flashing with STLINK then both STLINK and BF passthrough)
    - [DIY 20x20 RX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/2400MHz/RX_20x20) (Full Support, easy to build. WiFi Updating)
    - [DIY Nano RX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/2400MHz/RX_Nano) (Full Support, CRSF Nano Footprint, WiFi Updating)
    - [DIY Nano CCG RX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/2400MHz/RX_CCG_Nano) (Full Support, CRSF Nano Pinout, STM32 Based)
    - [DIY Nano Ceramic RX](https://github.com/ExpressLRS/ExpressLRS-Hardware/tree/master/PCB/2400MHz/RX_Nano_Ceramic) (Full Support, CRSF Nano Footprint, WiFi Updating, Built in antenna)
    - [HappyModel PP RX](http://www.happymodel.cn/index.php/2021/04/10/happymodel-2-4g-expresslrs-elrs-nano-series-receiver-module-pp-rx-ep1-rx-ep2-rx/) (Full Support, CRSF Nano Pinout, STM32 Based)
    - [HappyModel EP1/EP2 RX](http://www.happymodel.cn/index.php/2021/04/10/happymodel-2-4g-expresslrs-elrs-nano-series-receiver-module-pp-rx-ep1-rx-ep2-rx/) (Full Support, CRSF Nano Pinout, ESP8285 Based, WiFi Updating)

**For a more exhaustive list refer to the [Supported Hardware](https://github.com/ExpressLRS/ExpressLRS/wiki/Supported-Hardware) page on the wiki**

## Long Range Competition
One of the most frequently asked questions that gets asked by people who are interested in, but haven't yet tried ELRS is "How far does it go, and at what packet rate?"

The following table is a leaderboard of the current record holder for each packet rate, and the longest distance from home. Note that not every flight resulted in a failsafe at max range, so the link may go (much) futher in some cases.

### Rules
Anyone can add an entry to the table, and entries should include the:
- Max distance from home
- RF freq (900 / 2.4)
- Packet rate
- Power level
- If the link failsafed at max range
- The pilot name
- A link to your DVR on youtube (DVR is essential to compete, sorry, no keyboard claims)

### Current Leaderboard
| Max Dist. | Freq | Pkt Rate | TX Power | Failsafe at Max Range? | Pilot Handle | Link to DVR |
| ---- | -------- | -------- | --------- | ---------------------- | ------------ | ----------- |
| 40Km | 900M | 50HZ | 10mW | No | Snipes | https://www.youtube.com/watch?v=0QWN9qWoSYY |
| 33Km | 2.4G | 250HZ | 100mW | No | Snipes | https://www.youtube.com/watch?v=GkOCT17a-DE |
| 30Km | 900M | 50HZ | 1W | No | Snipes | https://www.youtube.com/watch?v=SbWvFIpVkto |
| 10Km | 2.4G | 250HZ | 100mW | No | Snipes | https://youtu.be/dJYfWLtXVg8 |
| 6Km | 900M | 100HZ | 50mW | No | Snipes | https://youtu.be/kN89mINbmQc?t=58 |
| 6Km | 2.4G | 500HZ | 250mW | No | Spec | https://www.youtube.com/watch?v=bVJaiqJq8gY |
| 4.77Km | 900M | 200HZ | 250mW | No | DaBit | https://www.youtube.com/watch?v=k0lY0XwB6Ko |
| 3Km | 2.4G (ceramic chip antenna RX) | 500HZ | 100mW | No | Spec | https://www.youtube.com/watch?v=kfa6ugX46n8 |
| 2.28Km | 900M | 50HZ | 10mW | No | Mike Malagoli | https://www.youtube.com/watch?v=qi4OygUAZxA&t=75s |

Check the [wiki page](https://github.com/ExpressLRS/ExpressLRS/wiki/Range-Competition) for previous leaders!


## Legal Stuff
The use and operation of this type of device may require a license and some countries may forbid its use. It is entirely up to the end user to ensure compliance with local regulations. This is experimental software/hardware and there is no guarantee of stability or reliability. **USE AT YOUR OWN RISK**

[![Banner](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/footer.png)](https://github.com/ExpressLRS/ExpressLRS/wiki#community)
