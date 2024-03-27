
<center>

# PrivacyLRS
PrivacyLRS is a privacy-protecting fork of the excellent [ExpressLRS (ELRS)](https://www.expresslrs.org/) long-range RC system.
PrivacyLRS is for those who want to use telemetry, but not broadcast that telemetry (including GPS location) for 
other people to read.

With *standard* ELRS, it is inconvenient for others to read your telemetry link - they would need to work for it a little bit.
PrivacyLRS uses strong encryption to make it *impossible* for other people to read your telemetry and get your location
that way, among other information.

RC commands are also encrypted.

## FAQ
### How do the performance and features compare?  
  Performance and features of ELRS are identical for the same version number, because PrivacyLRS is the exact same code as ELRS, just with the packets encrypted.
  The encryption is much, much faster than the radio link, so there is no measurable delay.

### How do I use PrivacyLRS?  
  Download the [zip file](https://github.com/sensei-hacker/PrivacyLRS/archive/refs/heads/secure_01.zip) of the secure branch.
  Unzip it, then flash using ELRS Configurator by choosing "Local" as shown in this screenshot:
  (https://raw.githubusercontent.com/sensei-hacker/PrivacyLRS/secure_01/privacylrs/screenshot_choose_local.png)

### Is this a fork because somebody got mad?  
  The maintainer of PrivacyLRS has nothing but love for ExpressLRS and the ELRS maintainers. I simply wanted a version that protects my privacy.

## Status of the project and testing
PrivacyLRS is currently in beta - testers needed. What needs testing is weird corner cases that could possibly break the
link, by making the encryption get out of sync.  Performance (range, refresh rate, etc) is exactly the same as standard ELRS,
so there is no point in testing that. Though you are welcome to if you wish.
\* If standard ELRS has latency of 6.522ms at a certain packet rate, PrivacyLRS will have latency of around 6.525ms - the
the difference probably being too small to measure.
If you choose to test PrivacyLRS, be sure to set up your failsafe carefully. Bugs could cause the link to fail.

## Differences between ELRS and PrivacyLRS
The ELRS documentation makes clear that the binding phrase is *not* a security feature in standard ELRS.
In standard ELRS, the binding phrase is to prevent *accidental* conflicts between two aircraft.
This is different in PrivacyLRS. In PrivacyLRS, the bind phrase is used as a small part of the security.
For this reason, it is recommended to make your bind phrase four words long - four words that other people are unlikely
to guess.
PrivacyLRS uses strong cryptographic keys which are radnomly generated, but the bind phrase also plays small part in
security.


## About ExpressLRS

ExpressLRS is an open source Radio Link for Radio Control applications. Designed to be the best FPV Racing link, it is based on the fantastic Semtech **SX127x**/**SX1280** LoRa hardware combined with an Espressif or STM32 Processor. Using LoRa modulation as well as reduced packet size it achieves best in class range and latency. It achieves this using a highly optimized over-the-air packet structure, giving simultaneous range and latency advantages. It supports both 900 MHz and 2.4 GHz links, each with their own benefits. 900 MHz supports a maximum of 200 Hz packet rate, with higher penetration. 2.4 GHz supports a blistering fast 1000 Hz on [EdgeTX](http://edgetx.org/). With over 60 different hardware targets and 13 hardware manufacturers, the choice of hardware is ever growing, with different hardware suited to different requirements.

## Configurator
To configure your ExpressLRS hardware, the ExpressLRS Configurator can be used, which is found here:

https://github.com/ExpressLRS/ExpressLRS-Configurator/releases/


