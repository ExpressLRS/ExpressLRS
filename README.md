![2.4GHz Hardware](img/banner.png)

## Need help? Confused? Join the Community!
 * [RCGroups Discussion](https://www.rcgroups.com/forums/showthread.php?3437865-ExpressLRS-DIY-LoRa-based-race-optimized-RC-link-system)
 * [Discord Chat](https://discord.gg/dS6ReFY)

## ESP32/ESP8285/STM32 based LoRa Radio Link

![Build Status](https://github.com/AlessandroAU/ExpressLRS/workflows/Build%20ExpressLRS/badge.svg)

ExpressLRS is an open source RC link for RC applications. It is based on **SX127x**/**SX1280** hardware combined with an **ESP8285**, **ESP32** or **STM32** for RX and TX respectively. ExpressLRS supports both `900 MHz` and `2.4 GHz` hardware options.

ExpressLRS can be flashed into existing **Frsky R9M hardware (RX and TX)** or **custom PCBs** can be make to suit. It can run at up to `250hz` or down to `50hz` depending on your preference of range or low latency.

At `900 MHz` a maximum of `200 Hz` packet rate is supported. This makes it the fastest long-range RC link currently on the market. Stick latency of down to `6.5ms` is seen on firmware with **crsfshot** (aka openTX 2.4 mixer scheduler) support. At `2.4 GHz` a blistering `250 Hz` is currently supported with plans to extend this to `500 Hz` once OpenTX support is added.  

ExpressLRS aims to achieve the best possible link preformance in both speed/latency and range. However, due to the optimized packet structure only basic telemetry is provided. If you want MavLink this project is not for you. ExpressLRS uses a compressed packet structure which priorities the first 4 control channels, this allows it to achieve better raw performance than other commercial solutions. ExpressLRS is also very affordable, a TX module can be built for $30 and receivers for $15-20. Likewise, new/second hand Frsky R9M gear is compatible and can be acquired inexpensively.

More information can be found in the [wiki](https://github.com/AlessandroAU/ExpressLRS/wiki). 

## Hardware Examples

### 2.4GHz DIY Receiver and Transmitter
![2.4GHz Hardware](img/24Ghardware.jpg)

Links:
- [Nano 2.4GHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/Nano_RX_SX1280_SMD) Currently Smallest DIY 2.4Ghz RX
- [20x20 2.4GHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/20x20_RX_SX1280) Convenient Stack Mounted DIY 2.4GHz RX

### 868/915MHz DIY Receiver and Transmitter
![2.4GHz Hardware](img/900Mhardware.jpg)

Links:
- [Mini 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/Mini_Rx_v0.1) Currently Smallest DIY 868/915MHz RX
- [20x20 900MHz RX](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/PCB/20x20_RX) Convenient Stack Mounted DIY 20x20mm 868/915MHz RX

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

 * [LILYGO TTGO LoRa](http://www.lilygo.cn/prod_view.aspx?TypeId=50003&Id=1134&FId=t3:50003:3)
 * [AliExpress TTGO LoRa (no OLED)](https://www.aliexpress.com/item/4000059700341.html)
 * [AliExpress TTGO LoRa (with OLED)](https://www.aliexpress.com/item/32840238513.html)

#### Enclosures

STLs for printing your own enclosure are available in the [STL folder](https://github.com/AlessandroAU/ExpressLRS/tree/master-dev/STL).

<img src="img/ttgo_lora_wiring_diagram.png" width="50%"><img src="img/TTGO_BOARD_2.png" width="50%">

### Legal Stuff
The use and operation of this type of device may require a license and some countries may forbid its use. It is entirely up to the end user to ensure compliance with local regulations. This is experimental software/hardware and there is no guarantee of stability or reliability. USE AT YOUR OWN RISK 
