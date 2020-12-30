![Banner](img/banner.png)

## Need help? Confused? Join the Community!
 * [RCGroups Discussion](https://www.rcgroups.com/forums/showthread.php?3437865-ExpressLRS-DIY-LoRa-based-race-optimized-RC-link-system)
 * [Discord Chat](https://discord.gg/dS6ReFY)
 * [Facebook Group](https://www.facebook.com/groups/636441730280366)


## ESP32/ESP8285/STM32 based LoRa Radio Link

![Build Status](https://github.com/AlessandroAU/ExpressLRS/workflows/Build%20ExpressLRS/badge.svg)

ExpressLRS is an open source RC link for RC applications. It is based on **SX127x**/**SX1280** hardware combined with an **ESP8285**, **ESP32** or **STM32** for RX and TX respectively. ExpressLRS supports both `900 MHz` and `2.4 GHz` hardware options. ExpressLRS uses **LoRa** modulation at all packet rates to achieve the best possible range and consistency of packet delivery. 

ExpressLRS can be flashed into existing **Frsky R9M hardware (RX and TX)**, **GHOST ATTO/ZEPTO Receivers** or **custom PCBs** can be make to suit. It can run at up to `500hz` or down to `50hz` depending on your preference of range or low latency.

At `900 MHz` a maximum of `200 Hz` packet rate is supported. At `2.4 GHz` a blistering `500 Hz` is currently supported with a custom openTX binary with plans to extend this to `1000 Hz` once OpenTX support is added. This makes ExpressLRS one of the fastest RC links available while still offering long-range preformance. 

![LatencyChart](img/Average%20Total%20Latency.png)

ExpressLRS aims to achieve the best possible link preformance in both speed/latency and range. However, due to the optimized packet structure only basic telemetry is provided. If you want MavLink this project is not for you. ExpressLRS uses a compressed packet structure which priorities the first 4 control channels, this allows it to achieve better raw performance than other commercial solutions. ExpressLRS is also very affordable, a TX module can be built for $30 and receivers for $15-20. Likewise, new/second hand Frsky R9M gear is compatible and can be acquired inexpensively.

More information can be found in the [wiki](https://github.com/AlessandroAU/ExpressLRS/wiki). 

## Hardware Examples

### 2.4GHz DIY Receiver and Transmitter
![2.4GHz Hardware](img/24Ghardware.jpg)

Links:
- [Nano 2.4GHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/2400MHz/RX_Nano) Currently Smallest DIY 2.4Ghz RX
- [20x20 2.4GHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/2400MHz/RX_20x20) Convenient Stack Mounted DIY 2.4GHz RX

### 868/915MHz DIY Receiver and Transmitter
![868/915MHz Hardware](img/900Mhardware.jpg)

Links:
- [Mini 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/900MHz/RX_Mini) Currently Smallest DIY 868/915MHz RX
- [20x20 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/900MHz/RX_20x20_0603_SMD) Convenient Stack Mounted DIY 20x20mm 868/915MHz RX
- [20x20 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/900MHz/RX_20x20_0805_SMD) Convenient Stack Mounted DIY 20x20mm 868/915MHz RX

## Compatible "Off-The-Shelf" Hardware

Development is ongoing but the following hardware is currently compatible

| Brand | Type | Name      | Notes |
| ----- | ---- | --------- | ----- |
| FrSky | TX   | R9M       | 2018 and 2019 versions, up to 2W ⚠️(cooling mod needed) |
| FrSky | TX   | R9M Lite  | output power 50mw in accordance with chip specification |
| TTGO  | TX   | LoRa v1   | __not__ recommended due to bad RF design (~10mW) |
| TTGO  | TX   | LoRa v2   | power output 50mW |
| FrSky | RX   | R9mm      |       |
| FrSky | RX   | R9mini    |       |

#### Building a TX Module using a TTGO Board

For the build you will need a TTGO LoRa board, with or without an OLED. These boards are readily available from ebay, aliexpress, and banggood. The only others parts required are some wire, 5 pin female header, and your favourite 5V regulator that can take the transmitters battery voltage range.

Note - The board I bought came with a female SMA pigtail. Check if your antenna is suitable.

**V1 Hardware, (not recommended)**
 * [LILYGO TTGO LoRa](http://www.lilygo.cn/prod_view.aspx?TypeId=50003&Id=1134&FId=t3:50003:3)
 * [AliExpress TTGO LoRa (no OLED)](https://www.aliexpress.com/item/4000059700341.html)
 * [AliExpress TTGO LoRa (with OLED)](https://www.aliexpress.com/item/32840238513.html)
 
**V2 Hardware** 
* [TTGO LORA32 V2.0](https://www.aliexpress.com/item/32847443952.html)

#### Enclosures

STLs for printing your own enclosure are available in the [STL folder](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/STL).

<img src="img/ttgo_lora_wiring_diagram.png" width="50%"><img src="img/TTGO_BOARD_2.png" width="50%">

## Long Range Leaderboard

One of the most frequently asked questions that gets asked from people who are interested in, but haven't yet tried ELRS is "How far does it go, and at what packet rate?"
The following table is a leaderboard of the current record holder for each packet rate, and the longest distance from home. Note that not every flight resulted in a failsafe at max range, so the link may go (much) futher in some cases.

Anyone can add an entry to the table, and entries should include the:
- RF freq (900 / 2.4),
- Packet rate,
- Power level,
- Max distance from home,
- If the link failsafed at max range,
- The pilot name, 
- A link to your DVR on youtube (DVR is essential to compete, sorry, no keyboard claims)

| Freq | Pkt Rate | TX Power | Max Dist. | Failsafe at Max Range? | Pilot Handle | Link to DVR |
| ---- | -------- | -------- | --------- | ---------------------- | ------------ | ----------- |
| 900M | 50HZ | 1W | 30Km | No | Snipes | https://www.youtube.com/watch?v=SbWvFIpVkto |
| 900M | 100HZ | 50mW | 6Km | No | Snipes | https://youtu.be/kN89mINbmQc?t=58 |
| 900M | 50HZ | 10mW | 2.28Km | No | Mike Malagoli | https://www.youtube.com/watch?v=qi4OygUAZxA&t=75s |
| 900M | 200HZ | 250mW | 4.77Km | No | DaBit | https://www.youtube.com/watch?v=k0lY0XwB6Ko |
| 2.4G | 250HZ | 250mW | 9.8Km | Yes | Snipes | https://www.youtube.com/watch?v=D9SHpSguOMQ |

### Legal Stuff
The use and operation of this type of device may require a license and some countries may forbid its use. It is entirely up to the end user to ensure compliance with local regulations. This is experimental software/hardware and there is no guarantee of stability or reliability. USE AT YOUR OWN RISK 

[![Banner](img/footer.png)](https://github.com/AlessandroAU/ExpressLRS/wiki#community)
