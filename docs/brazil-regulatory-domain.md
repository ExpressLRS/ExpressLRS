# Brazil BR915 domain

BR915 is a proposed sub-GHz domain for Brazil. It avoids channel centers in the
907.5–915 MHz gap and sets a maximum software output level of 250 mW. It is
available for SX127x, LR1121 and LR2021 hardware.

## Selecting BR915

- In a source build, enable `-DRegulatory_Domain_BR_915` in `src/user_defines.txt`.
- In the binary configurator, select `--domain br_915`.
- In the TX or RX web options, select `BR915` and save the settings.

Select BR915 on both the transmitter and receiver. The appended domain ID is
9; existing domain IDs 0–8 keep their meanings. BR915 is not offered by the
binary configurator as a 2.4 GHz regulatory domain.

## Frequency grid

The original grid has 42 channel centers from 902.35 to 926.95 MHz with 600 kHz
spacing. Inclusive indexes 9–21 are excluded:

| Retained indexes in the original grid | Channel centers | Count |
| --- | --- | --- |
| 0–8 | 902.35–907.15 MHz | 9 |
| 22–41 | 915.55–926.95 MHz | 20 |
| Total | Both slices | 29 |

The 29 retained channels yield a 232-entry hopping sequence, containing each
channel eight times. Sync and same-band Gemini use retained-channel indexes.
The radio calibration/CW test center is 922.15 MHz, inside the upper slice.

This implements the maintainer's 42-channel grid with indexes 9–21 removed;
that calculation leaves 29 channels, rather than the earlier 30-channel
estimate in [discussion #3541](https://github.com/ExpressLRS/ExpressLRS/discussions/3541).

The grid accounts for a nominal 500 kHz channel width plus a conservative
100 kHz frequency-correction allowance in either direction. At the lower slice
edges, this envelope reaches 902 and 907.5 MHz exactly on radios using Hz units.
SX127x register quantization is covered by native tests. There is no additional
edge margin beyond that envelope.

## Power and dual-band operation

The BR915 sub-GHz output setting is capped at 250 mW, or the hardware maximum
if lower. Static power, dynamic power and RX telemetry share that enforcement.
On CE LR1121/LR2021 builds, the existing 25 mW sub-GHz ceiling remains stricter
and takes precedence.

For simultaneous sub-GHz/2.4 GHz operation, the shared output setting uses the
tighter domain limit: BR915 normally limits it to 250 mW on both bands, or
25 mW in a CE build. In 2.4 GHz-only operation, the 2.4 GHz domain's limit
applies. A dual-band device may therefore offer selectable power above 250 mW;
the sub-GHz output remains capped when BR915 is active.

The existing hardware-minimum fallback still applies. Hardware whose minimum
calibrated output exceeds the domain limit cannot satisfy that cap through the
firmware setting; see [power-limit documentation](regulatory-power-limits.md).

## Validation before release

[ANATEL Act 14448, section 10](https://informacoes.anatel.gov.br/legislacao/atos-de-certificacao-de-produtos/2017/1139-ato-14448)
lists 902–907.5 and 915–928 MHz for the discussed use. Under the FHSS requirements
in section 10.2.5, fewer than 35 hopping channels have a 250 mW peak output
ceiling; bandwidth and occupancy requirements also apply. Section 10.4 treats
hybrid systems separately, so classification must be established for each mode.

Native tests verify frequency arithmetic, exclusions, sync, Gemini, sequence
balance and domain power metadata. They do not establish RF compliance or
product homologation. Before release, measure occupied bandwidth, band-edge
and spurious emissions, dwell time and calibrated output for each supported
modulation mode. Gemini's simultaneous radio outputs and their aggregate power
must also be evaluated. Wider edge margins or a lower Gemini power setting may
be needed after measurement and certification review.
