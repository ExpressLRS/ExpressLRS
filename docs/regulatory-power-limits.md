# Regulatory domain power limits

The maximum power you can select is constrained by the hardware and the
regulatory domains included in the firmware. The actual output may be lower
when the active RF band has a tighter limit.

## Domain-scoped CE limits

| Firmware and active mode | Maximum selectable power | Output limit |
| --- | --- | --- |
| CE_LBT on a single-band 2.4 GHz radio | 100 mW | 100 mW |
| EU868/EU433 + CE_LBT on LR1121/LR2021, 2.4 GHz only | 100 mW | 100 mW |
| EU868/EU433 + CE_LBT on LR1121/LR2021, sub-GHz only | 100 mW | 25 mW |
| EU868/EU433 + CE_LBT on LR1121/LR2021, simultaneous dual-band | 100 mW | 25 mW on both bands |
| BR915 + CE_LBT, sub-GHz only | 250 mW | 250 mW |
| BR915 + CE_LBT, 2.4 GHz only | 250 mW | 100 mW |
| BR915 + CE_LBT, simultaneous dual-band | 250 mW | 100 mW on both bands |

The `Regulatory_Domain_EU_CE_2400` flag scopes its power policy to EU domains:
CE_LBT is capped at 100 mW, and EU868/EU433 are capped at 25 mW on dual-band
radios in CE builds. FCC915, AU915, TH920 and other non-EU sub-GHz domains keep
their own limits. In particular, BR915 retains 250 mW. This intentionally
corrects the previous blanket 25 mW restriction on non-EU sub-GHz domains.

Lua power choices show the shared requested level. The CE information field
reports the selected domains' limits, rather than assuming every sub-GHz domain
is capped at 25 mW.

Hardware with a lower maximum remains limited to that maximum. The shared
output setting in simultaneous dual-band mode uses the tighter of the two
domain limits; the two bands do not have independently selectable power.

Non-CE domains retain their previous hardware-defined maximum. Their software
ceiling of 2000 mW preserves the existing power-level range; it is not a statement
that every region permits that output.

## Static power, dynamic power, and receiver telemetry

Selecting or loading a power setting above a domain's output limit cannot raise
the output above that limit on hardware that supports the required setting.
The same output restriction applies to dynamic-power adjustments and receiver
telemetry, including MatchTX. Default power is constrained by the selectable
maximum, then by the active band's output limit.

The most recent power request is retained separately from the applied output.
When a dual-band radio changes its active bands, it re-evaluates that request
before configuring the radio. For example, a 100 mW request with EU868/EU433 and CE_LBT
produces 25 mW in sub-GHz mode and can produce 100 mW again in 2.4 GHz mode.
For dynamic power, this restores the most recent request, not necessarily the
configured Max Power ceiling. Band changes no longer require a subsequent
power adjustment to apply the appropriate limit.

## Hardware minimum limitation

The existing fallback is retained when a domain's output limit is below the
hardware's minimum calibrated power: the firmware uses the hardware minimum.
It does not index the calibration table below that minimum or disable the radio.
Such hardware cannot satisfy the domain cap through this setting and needs a
compatible calibration/hardware configuration before use in that domain.

## Adding a domain

Each FHSS domain definition carries a `max_power` value from the existing power
level enumeration. Frequency selection remains in FHSS; power management
combines the domain limit with hardware limits and the active band selection.

The [Brazil BR915 domain](brazil-regulatory-domain.md) uses a 250 mW sub-GHz
ceiling with channel exclusions. Pairing BR915 with CE_LBT leaves sub-GHz at
250 mW and limits simultaneous dual-band output to the tighter 100 mW cap.
Existing domain IDs and over-the-air packet formats retain their meanings.

For Lua power controls, see the
[ExpressLRS Lua guide](https://www.expresslrs.org/quick-start/transmitters/lua-howto/).
