![Banner](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/banner.png?raw=true)

<center>

[![Release](https://img.shields.io/github/v/release/ExpressLRS/ExpressLRS?style=flat-square)](https://github.com/ExpressLRS/ExpressLRS/releases)
[![Build Status](https://img.shields.io/github/workflow/status/ExpressLRS/ExpressLRS/Build%20ExpressLRS?logo=github&style=flat-square)](https://github.com/ExpressLRS/ExpressLRS/actions)
[![License](https://img.shields.io/github/license/ExpressLRS/ExpressLRS?style=flat-square)](https://github.com/ExpressLRS/ExpressLRS/blob/master/LICENSE)
[![Stars](https://img.shields.io/github/stars/ExpressLRS/ExpressLRS?style=flat-square)](https://github.com/ExpressLRS/ExpressLRS/stargazers)
[![Chat](https://img.shields.io/discord/596350022191415318?color=%235865F2&logo=discord&logoColor=%23FFFFFF&style=flat-square)](https://discord.gg/dS6ReFY)

</center>

## Support ExpressLRS
Supporting ExpressLRS is as easy as contributing a feature, either code or just a fleshed out idea. Coding not your thing? Testing a Pull Request using the convenient Configurator tab and providing feedback is essential as well. We're all working together.

If you don't have the time to contribute in that way, consider making a small donation. Donations are used to buy test equipment, software licenses, and certificates needed to further the project and make it securely accessible. ExpressLRS accepts donations through [Open Collective](https://opencollective.com/), which provides recognition of donors and transparency on how that support is utilized.

[![Open Collective backers](https://img.shields.io/opencollective/backers/expresslrs?label=Open%20Collective%20backers&style=flat-square)](https://opencollective.com/expresslrs)

## Website
For general information on the project please refer to our guides on the [website](https://www.expresslrs.org/), and our [FAQ](https://www.expresslrs.org/faq/)

## About

ExpressLRS is an open source Radio Link for Radio Control applications. Designed to be the best FPV Racing link, it is based on the fantastic Semtech **SX127x**/**SX1280** LoRa hardware combined with an Espressif or STM32 Processor. Using LoRa modulation as well as reduced packet size it achieves best in class range and latency. It achieves this using a highly optimized over-the-air packet structure, giving simultaneous range and latency advantages (see below). It supports both 900 MHz and 2.4 GHz links, each with their own benefits. 900 MHz supports a maximum of 200 Hz packet rate, with higher penetration. 2.4 GHz supports a blistering fast 500 Hz on [EdgeTX](http://edgetx.org/). With over 40 different hardware targets and 13 hardware manufacuturers, the choice of hardware is ever growing, with different hardware suited to different requirements

![LatencyChart](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/Average%20Total%20Latency.png?raw=true)
![RangeVsPacketRate](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/pktrate_vs_sens.png?raw=true)

## Configurator
To configure your ExpressLRS hardware, the ExpressLRS Configurator can be used, which is found here:

https://github.com/ExpressLRS/ExpressLRS-Configurator/releases/

## Community
We have both a [Discord Server](https://discord.gg/dS6ReFY) and [Facebook Group](https://www.facebook.com/groups/636441730280366), which have great support for new users and constant ongoing development discussion

## Features

ExpressLRS has the following features:

- 500 Hz Packet Rate 
- Telemetry (Betaflight Lua Compatibility)
- Wifi Updates
- Bluetooth Sim Joystick
- Oled & TFT Displays
- 2.4 GHz or 900 MHz RC Link
- Ceramic Antenna - allows for easier installation into micros
- VTX and VRX Frequency adjustments from the LUA
- Bind Phrases - no need for button binding

with many more features on the way!

## Supported Hardware

ExpressLRS currently supports hardware from the following manufacturers. Any hardware with a check next to it has been sent to and tested by developers:

- AxisFlying :white_check_mark:
- BETAFPV
- Flywoo
- FrSky
- HappyModel :white_check_mark:
- HiYounger
- HGLRC
- ImmersionRC
- iFlight
- JHEMCU
- Jumper
- Matek :white_check_mark:
- NamimnoRC :white_check_mark:
- QuadKopters
- SiYi

For an exhaustive list of hardware targets and their user guides, check out the [Supported Hardware](https://www.expresslrs.org/2.0/hardware/supported-hardware/) and [Reciever Selection](https://www.expresslrs.org/2.0/hardware/receiver-selection/) pages on the website. We do not manufacture any of our hardware, so we can only provide limited support on defective hardware.

## Developers

If you are a developer and would like to contribute to the project, feel free to join the [discord](https://discord.gg/dS6ReFY) and chat about bugs and issues. You can also look for issues at the [GitHub Issue Tracker](https://github.com/ExpressLRS/ExpressLRS/issues). The best thing to do is to a submit a Pull Request to the GitHub Repository. 

![](https://github.com/ExpressLRS/ExpressLRS-Hardware/blob/master/img/community.png?raw=true)