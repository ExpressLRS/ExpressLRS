![Banner](img/banner.png)

## Need help? Confused? Join the Community!
 * [RCGroups Discussion](https://www.rcgroups.com/forums/showthread.php?3437865-ExpressLRS-DIY-LoRa-based-race-optimized-RC-link-system)
 * [Discord Chat](https://discord.gg/dS6ReFY)
 * [Facebook Group](https://www.facebook.com/groups/636441730280366)

## Suport ExpressLRS
If you would like to support the development of ExpressLRS please feel free to make a small donation. This helps us buy hardware for porting, development and prototyping. Show your support for which new features you want added by leaving a message when you donate<br/><br/>
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/donate?hosted_button_id=FLHGG9DAFYQZU)

## High Preformance LoRa Radio Link

![Build Status](https://github.com/AlessandroAU/ExpressLRS/workflows/Build%20ExpressLRS/badge.svg)

ExpressLRS is an open source RC link for RC applications. It is based on the fantastic semtech **SX127x**/**SX1280** hardware combined with an **ESP8285**, **ESP32** or **STM32**. ExpressLRS supports a wide range of hardware platforms as well as both `900 MHz` and `2.4 GHz` frequency options. ExpressLRS uses **LoRa** modulation as well as reduced packet size to achieve **best in class range and latency** compared to current commercial offerings. 

ExpressLRS can run at various packet rates, up to `500hz` or down to `25hz` depending on your preference of range or low latency. At `900 MHz` a maximum of `200 Hz` packet rate is supported. At `2.4 GHz` a blistering `500 Hz` is currently supported with a custom openTX binary with future plans to extend this to `1000 Hz`.

ExpressLRS can be flashed into existing **Frsky R9M hardware (RX and TX)**, **Jumper R900 RXs**, **GHOST ATTO/ZEPTO Receivers** or **Custom PCBs** can be made if you enjoy tinkering. Several manufacturers are preparing to offer offical ELRS hardware soon so stay tuned. 

![LatencyChart](img/Average%20Total%20Latency.png)

ExpressLRS aims too achieve the best possible link preformance for both latency and range. This is achieved with an optimised over the air packet structure.  However, only basic telemetry is currently provided (**VBAT**, downlink/uplink **LQ** and downlink/uplink **RSSI**), full telemetry support is supported in our development branch. This comprimise allows ExpressLRS to achieve simultaneous **better latency AND range** compared to other options in the market. For example, **ExpressLRS 2.4GHz 150Hz** mode offers the same range as **GHST Normal** while delivering near **triple** the packet update rate. Similarly, **ExpressLRS 900MHz 200Hz** will dramatically out-range **Crossfire 150Hz** and **ExpressLRS 50Hz** will out-range **Crossfire 50Hz** watt per watt.   

## Should I use 2.4ghz or 915/868mhz?

Unfortunately, there is a misconception about the 2.4ghz range thanks to other radio protocols on the market. ELRS uses LoRa, which has had some amazing results on 2.4. The current max range of ELRS on 2.4ghz (250hz locked) is 33km with only 100mW of power. This moves 2.4ghz to a long-range solution for pilots. If you think that 33km is not enough range, then bump up that 100mW power to 250mW. If you think that 33km+ range just will not cut it, look at 868/915 MHz solutions. 


#### RF noise
We have all heard the stories of racers powering up his TBS crossfire full module at 2W and causing people to fail-safe during a race. This happens because the 868/915mhz band has limited bandwidth. The solution for this is to use a low power mode during races, so you do not blast anyone out of the sky. 2.4ghz does not have this issue. Flite Test has a world record of having [179 RC airplanes](https://www.guinnessworldrecords.com/world-records/most-rc-model-aircraft-airborne-simultaneously#:~:text=The%20most%20RC%20model%20aircraft,USA%2C%20on%2016%20July%202016) in the sky using 2.4 GHz.

2.4 GHz LoRa can also handle WiFi noise very well. [Studies](https://link.springer.com/article/10.1007/s11235-020-00658-w) have been conducted with the coexistence of WiFi and LoRa bands. 

868/915 does not have to worry about WiFi signal but it does have to worry about cell towers and other RF noise. You are fighting against thermostats, fire systems, burglar systems and any other device running on that band. 

#### Where can I get 2.4GHz if I cannot solder? 
The number of 2.4GHz TX's and RX's that ELRS supports is on the rise. You can currently flash a Ghost TX from ImmersionRC or FM30 2.4ghz TX from SiYi with ELRS. Siyi is only available on Aliexpress and could take up to a month to get it in the mail. If you want something fast but do not want to pay the high price for a Ghost TX full/lite then you check out our buy and sell channel on the discord—lots of great people making great hardware. 

**2.4GHz Comparison**

![RangeVsPacketRate](img/pktrate_vs_sens.png)

More information can be found in the [wiki](https://github.com/AlessandroAU/ExpressLRS/wiki). 

## Supported Hardware

Development is ongoing but the following hardware is currently compatible

**Frsky Hardware**
| **RX/TX** | **Hardware**    | **Status**          | **Notes**                                    |
| --------- | --------------- | ------------------- | -------------------------------------------- |
| TX        | 2018 R9M        | Fully Supported     | Requires resistor mod for lowest latency     |
| TX        | 2019 R9M        | Fully Supported     | Resistor mod not required                    |
| TX        | R9M Lite        | Fully Supported     | Limited to 50mW                              |
| TX        | R9M Lite Pro    | In Development      |                                              |
| RX        | R9MM            | Fully Supported     |                                              |
| RX        | R9MX            | Fully Supported     |                                              |
| RX        | R9mini          | Fully Supported     |                                              |
| RX        | R9slimplus      | Fully Supported     |                                              |
| RX        | R9slimplusOTA   | Fully Supported     |                                              |

**Jumper Hardware**
| **RX/TX** | **Hardware**    | **Status**          | **Notes**                                                         |
| --------- | --------------- | ------------------- | ----------------------------------------------------------------- |
| RX        | R900 mini       | Fully Supported     | Can only be flashed via stlink,  BAD included antenna             |

**ImmersionRC Ghost Hardware**
| **RX/TX** | **Hardware**    | **Status**          | **Notes**                                    |
| --------- | --------------- | ------------------- | -------------------------------------------- |
| RX        | Ghost Atto      | Fully Supported     | Can only be initially flashed via stlink then betaflight passthrough |
| RX        | Ghost Zepto     | Fully Supported     | Can only be initially flashed via stlink then betaflight passthrough |
| TX        | Ghost TX        | Fully Supported     | Can only be initialy flashed via stlink (Development Branch)| 

**Siyi Hardware**
| **RX/TX** | **Hardware**    | **Status**          | **Notes**                                    |
| --------- | --------------- | ------------------- | -------------------------------------------- |
| TX | FM30 | Fully Supported | Stlink initial flash, USB update |
|RX | FM30 Mini | Development | In STlink initial flash, betaflight passthrough update

**DIY 2.4GHz Hardware**
| **RX/TX** | **Hardware**                       | **Status**          | **Notes**                                    |
| --------- | ---------------                    | ------------------- | -------------------------------------------- |
| TX        | ESP32 Module (E28 SX1280)          | Fully Supported     | Flashable via USB, 250mW max                 |
| TX        | ESP32 Module (F27 SX1280)          | In Testing          | Flashable via USB, 250mW max                 |
| TX        | ESP32 Module (Bare SX1280)         | Fully Supported     | Flashable via USB, 20mW max                  |
| RX        | 20x20mm RX                         | Fully Supported     | Supports WIFI Updating                       |
| RX        | Nano RX                            | Fully Supported     | Supports WIFI Updating                       |
| RX        | CCG Nano RX                        | Fully Supported     | No WIFI, STM32 Based                         |

**DIY 900MHz Hardware**
| **RX/TX** | **Hardware**                           | **Status**          | **Notes**                                    |
| --------- | ---------------                        | ------------------- | -------------------------------------------- |
| TX        | DIY Module (RFM95 Module)              | Fully Supported     | Flashable via USB, 50mW max                  |
| TX        | TTGO V1 Dev Board                      | Fully Supported     | No longer recommended                        |
| TX        | TTGO V2 Dev Board                      | Fully Supported     | Supports WIFI Updating, 50mW max             |
| RX        | DIY mini RX                            | Fully Supported     | Supports WIFI Updating                       |
| RX        | DIY 20x20 RX                           | Fully Supported     | Supports WIFI Updating                       |

## Hardware Examples

### 2.4GHz DIY Receiver and Transmitter
![2.4GHz Hardware](img/24Ghardware.jpg)

Links:
- [Nano 2.4GHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/2400MHz/RX_Nano) Currently Smallest DIY 2.4Ghz RX
- [20x20 2.4GHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/2400MHz/RX_20x20) Convenient Stack Mounted DIY 2.4GHz RX

### 868/915MHz DIY Receiver and Transmitter
![868/915MHz Hardware](img/900Mhardware.jpg)

Links:
- [Mini 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master/PCB/900MHz/RX_Mini_v1.1) Currently Smallest DIY 868/915MHz RX
- [20x20 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/900MHz/RX_20x20_0603_SMD) Convenient Stack Mounted DIY 20x20mm 868/915MHz RX
- [20x20 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/900MHz/RX_20x20_0805_SMD) Convenient Stack Mounted DIY 20x20mm 868/915MHz RX

## Long Range Leaderboard
One of the most frequently asked questions that gets asked from people who are interested in, but haven't yet tried ELRS is "How far does it go, and at what packet rate?"
The following table is a leaderboard of the current record holder for each packet rate, and the longest distance from home. Note that not every flight resulted in a failsafe at max range, so the link may go (much) futher in some cases.

Anyone can add an entry to the table, and entries should include the:
- Max distance from home,
- RF freq (900 / 2.4),
- Packet rate,
- Power level,
- If the link failsafed at max range,
- The pilot name, 
- A link to your DVR on youtube (DVR is essential to compete, sorry, no keyboard claims)

| Max Dist. | Freq | Pkt Rate | TX Power | Failsafe at Max Range? | Pilot Handle | Link to DVR |
| ---- | -------- | -------- | --------- | ---------------------- | ------------ | ----------- |
| 33Km | 2.4 | 250HZ | 100mW | No | Wez | https://youtu.be/GkOCT17a-DE |
| 30Km | 900M | 50HZ | 1W | No | Snipes | https://www.youtube.com/watch?v=SbWvFIpVkto |
| 10Km | 2.4G | 250HZ | 100mW | No | Snipes | https://youtu.be/dJYfWLtXVg8 |
| 6Km | 900M | 100HZ | 50mW | No | Snipes | https://youtu.be/kN89mINbmQc?t=58 |
| 4.77Km | 900M | 200HZ | 250mW | No | DaBit | https://www.youtube.com/watch?v=k0lY0XwB6Ko |
| 2.28Km | 900M | 50HZ | 10mW | No | Mike Malagoli | https://www.youtube.com/watch?v=qi4OygUAZxA&t=75s |

### Legal Stuff
The use and operation of this type of device may require a license and some countries may forbid its use. It is entirely up to the end user to ensure compliance with local regulations. This is experimental software/hardware and there is no guarantee of stability or reliability. USE AT YOUR OWN RISK 

[![Banner](img/footer.png)](https://github.com/AlessandroAU/ExpressLRS/wiki#community)
