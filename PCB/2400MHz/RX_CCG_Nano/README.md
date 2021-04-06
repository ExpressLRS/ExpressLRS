v0.5 PCB
- Initial version


# CCG Nano 2.4 GHz Rx

The ExLRS CCG Nano Rx is 15mm*9.5mm and uses the same pinout as the CRSF Nano.

This PCB uses 0402 or 0603 SMD components and will require a hot air rework station, plus a microscope to make life easier.

Here is the EasyEDA project containing schematics, PCB design and BOM.
https://easyeda.com/Juhno/elrssmall2-4rx

Big thanks to @juhtipuhti for this great design!


### PCB manufacturing

Upload the Gerber file to https://jlcpcb.com/RAT and select 4-layers and one design (already panelized).
Its recommended to use 1.6mm board thickness so the mousebites wont break in production.

### BOM

- STM32L432KBU6 https://lcsc.com/product-detail/ST-Microelectronics_STMicroelectronics-STM32L432KBU6_C94784.html
- SX1280 https://lcsc.com/product-detail/RF-Transceiver-ICs_SEMTECH-SX1280IMLTRT_C125969.html 
- 0603 Led https://lcsc.com/product-detail/Light-Emitting-Diodes-LED_Everlight-Elec-19-217-BHC-ZL1M2RY-3T_C72041.html
- SOT23 3.3v Reg https://lcsc.com/product-detail/Low-Dropout-Regulators-LDO_MICRONE-Nanjing-Micro-One-Elec-ME6216A33M3G_C126466.html
- Ipex connector https://lcsc.com/product-detail/RF-Connectors-Coaxial-Connectors_Shenzhen-Kinghelm-Elec-KH-IPEX-K501-29_C411563.html
- Button https://lcsc.com/product-detail/Tactile-Switches_ALPS-Electric-SKTAAAE010_C358616.html
- RF output filter https://au.mouser.com/ProductDetail/johanson/2450fm07d0034t/?qs=%252bEew9%252b0nqrBEY7VUloPs4Q%3D%3D
- Crystal https://au.mouser.com/ProductDetail/Diodes-Incorporated/FW520WFMT1?qs=%2Fha2pyFadugPDf0nhgewaMwLjNbxaY4Xr5XmslnYeZgOQCJm3ZvVpQ%3D%3D or https://www.digikey.com/product-detail/en/diodes-incorporated/FW520WFMT1/FW520WFMT1CT-ND/6173706
- 0603 Capacitor 22uF https://lcsc.com/product-detail/Multilayer-Ceramic-Capacitors-MLCC-SMD-SMT_FH-Guangdong-Fenghua-Advanced-Tech-0603X226M6R3NT_C94018.html
- 0603 Capacitor 10uF https://lcsc.com/product-detail/Multilayer-Ceramic-Capacitors-MLCC-SMD-SMT_SANYEAR-C0603X5R106K6R3NT_C475250.html
- 0603 Capacitor 15pF https://lcsc.com/product-detail/Multilayer-Ceramic-Capacitors-MLCC-SMD-SMT_Walsin-Tech-Corp-0603N150F500CT_C458893.html
- 0603 Resistor 10K https://lcsc.com/product-detail/Chip-Resistor-Surface-Mount_KOA-Speer-Elec-RK73B1JTTD103J_C159891.html
- 0603 Resistor 470ohm(Not needed, pullup for TX pad) https://lcsc.com/product-detail/Chip-Resistor-Surface-Mount_FH-Guangdong-Fenghua-Advanced-Tech-RS-03K471JT_C140089.html
