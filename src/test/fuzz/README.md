# RX fuzzer

## Rationale

This fuzzer looks for receiver bugs without hardware: it runs the receiver
firmware on a PC, feeds it a radio link that drops and corrupts packets, and
checks every frame the receiver sends to the flight controller. Issue
[#3617](https://github.com/ExpressLRS/ExpressLRS/issues/3617) is an example of
the kind of bug it is for.

## What it does

The receiver is the real firmware, built unmodified: `rx_main.cpp`,
`common.cpp`, config, FHSS, OTA, CRC, the CRSF router, telemetry and the serial
drivers. Underneath it, `sim_hardware.cpp` provides a virtual clock, the
hardware timer, a radio that hands packets over, an EEPROM in RAM and empty
stubs for the peripherals.

In front of it is a simulated transmitter (`sim_tx.cpp`). It packs packets
with the real OTA code and follows the real rate table, hop sequence, sync
packets and telemetry slots. The transmitter has its own copy of the OTA code
(`sim_tx_ota.cpp`), so the harness only reads the receiver's state and never
writes it once the receiver has booted. Between the two is the channel. For every packet
the receiver is in a position to hear, the test case decides what happens to it:

- delivered, dropped, or dropped in a burst of 8 to 1024
- one byte bit-flipped
- truncated, the tail overwritten
- corrupted but with a valid CRC, as a false accept would be, at most once per
  test case

A test case can also power cycle the transmitter, make it lose the receiver's
telemetry, and stall the receiver's main loop. The transmitter's clock runs off
against the receiver's and packets arrive with jitter, within
`MAX_CLOCK_OFFSET_PPM` and `MAX_CLOCK_DRIFT_PPM_PER_S` in `sim_tx.h` and
`MAX_ARRIVAL_JITTER_US` in `fuzz_channel.h`.

Every CRSF, SBUS and SUMD frame the receiver emits is checked:

| Check | Fails when |
|---|---|
| `leak` | a channel the transmitter sends was output before it was ever received, without failsafe flagged |
| `wrong-value` | a received channel was output with a value other than what was sent |
| `unset-not-min` | a channel the transmitter never sends was output as something other than minimum |
| `blackout` | valid RC packets on a healthy link produce no output |
| `failsafe-not-held` | the RX is set to hold the last position on failsafe, but after losing the link it output a channel at some other value than the last one it had |
| `protocol-changed` | the configured serial protocol was changed by something received over the air |

The build has AddressSanitizer and UndefinedBehaviorSanitizer on, so memory
errors in the firmware show up as `crash`.

Each seed is one test case. `fuzz_main.cpp` turns the seed into input bytes
and runs it in a forked process, because the firmware keeps its state in globals
that cannot be reset.

## How to use it

Needs Linux, `clang++` and `make`. PlatformIO is not involved.

```
cd src/test/fuzz
make run                               # 1M seeds, random start, all cores
make run SEED=1 COUNT=50000            # a fixed range
make run FUZZ_SKIP=leak:SBUS,blackout  # ignore some checks
make repro SEED=17918                  # one seed, with a per-packet trace
```

The run is controlled by environment variables. They can be exported, put in
front of the command (`SEED=1 COUNT=50000 make run`), or given as arguments to
`make` as above.

| Variable | Default | |
|---|---|---|
| `SEED` | random | first seed |
| `COUNT` | 1000000 | number of seeds |
| `JOBS` | physical cores | parallel processes |
| `FUZZ_SKIP` | | checks to ignore |

`FUZZ_SKIP` takes check names, or `check:PROTOCOL` to skip a check for one
output protocol only. It is how to get past known findings to look for new ones.

A seed only reproduces at the commit it was found at. Changing the harness,
`inputFromSeed()` or the firmware can move it, so record the commit with the
seed.

## Example

The binary can also be run directly, with a first seed, a count and optionally
the number of processes to run at once. Without a first seed, or with `random`
in its place, it takes one from `/dev/urandom` and prints it:

```
$ build/rx_fuzzer 1 5
seed 4: VIOLATION unset-not-min on CRSF
  where     slot 19, 500Hz std, txMode=0 rxMode=0, connected
  what      CH13 emitted 2047, expected 0
  channels  300 380 460 540 1792 699 775 851 927 1029 1105 1182 - 1792 - -
seed 5: VIOLATION unset-not-min on CRSF
  where     slot 442, 1000Hz std, txMode=0 rxMode=0, connected
  what      CH13 emitted 2047, expected 0
  channels  300 380 460 540 1792 699 775 851 927 1029 1105 1182 - 1792 - -
seeds 1..5: 2 x unset-not-min:CRSF
```

Each failing seed gets a short report, up to three per kind. In the report for
seed 4:

- `unset-not-min on CRSF` is the check that failed and the output protocol.
- `where` is the transmitter's packet slot, the packet rate, standard or
  `fullres` packets, the switch mode the transmitter packs in and the receiver
  unpacks in, and the receiver's connection state.
- `what` says the receiver put 2047 on channel 13 of a CRSF frame, where 0 was
  expected because the transmitter never sends that channel.
- `channels` is the receiver's 16 channel values, `-` for one it has never
  received.

The last line counts the failing seeds of each kind and output protocol: 2 of
the 5 seeds hit `unset-not-min` on CRSF.

`make repro SEED=4` replays that seed and prints every packet.

## What it cannot model

The transmitter is a model, not `tx_main.cpp`. A finding that depends on what
the transmitter does, such as how many packets it sends between a config change
and the commit, needs checking against the transmitter's code before it is
believed.

Not covered at all:

- PWM and servo output
- MAVLink and HoTT output, whose libraries are not in the tree
- the DVDA rates (D500, D250), which send every packet more than once
- the 900 MHz, LR1121 and LR2021 radios, only the SX1280 rate table is used
- a second radio: diversity and Gemini
- packet rate and switch mode changes on the transmitter
- MSP and other data uplink packets, the transmitter sends only sync and RC
- model match, team race, binding
- a disarmed transmitter, and arming by a switch other than CH5
- the receiver being power cycled, it boots once per seed

Simplified:

- The radio delivers a packet or it does not. There is no RSSI or SNR variation,
  no frequency error, and the radio never reports a CRC error or timeout of its
  own.
- A power cycled transmitter comes back on the same timing grid and at the same
  rate.
- Lost telemetry is all or nothing, and a corrupted telemetry packet is the same
  as a lost one.
- The firmware's `loop()` runs once per simulated millisecond and after every
  interrupt, not continuously.
- Interrupts never preempt `loop()` part way through, so races between the two
  are not exercised.
