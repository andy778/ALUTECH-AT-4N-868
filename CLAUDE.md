# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a hardware reverse engineering project for the **ALUTECH AT-4N-868** garage
door remote (4-button, 868.35 MHz rolling code transmitter; some units are
433.92 MHz). The goal is to decode the radio protocol with rtl_433, understand the
rolling-code scheme, and explore what a Flipper Zero can and cannot do with it.

**This is not a Microchip HCS301 / KeeLoq device.** That was an early assumption
and has been ruled out:

- A new unit has **U1 = PUYA PY32F002A**, an Arm Cortex-M0+ MCU running custom
  firmware — not a fixed-function KeeLoq encoder.
- The frame is **72 bits**, not the 66 bits of an HCS200/HCS300 KeeLoq frame,
  which is why the existing rtl_433 `hcs200` decoder never matched.
- The protocol matches Flipper Zero's proprietary `alutech_at_4n` decoder
  (`lib/subghz/protocols/alutech_at_4n.c`): 72-bit frame, CRC-8 poly `0x31`, and
  an XTEA-style Feistel cipher keyed by six per-manufacturer "magic" constants.

## Key Files

- **`src/devices/alutech_at_4n.c`** — the C rtl_433 device decoder (primary
  technical artifact). Validates the CRC and emits the encrypted payload; has an
  optional decryption path that takes the six magic constants at runtime.
- **`README.md`** — full protocol write-up, frame layout, and usage.
- **`docs/firmware-extraction.md`** — how to dump the PY32 firmware over SWD (JP1
  header) from your own hardware.
- **`research/flipper/`** — the (public, AES-encrypted) Flipper keystore asset and
  a helper script that decrypts it *given* the AES key.
- **`alutech.conf`** — early rtl_433 flex-decoder experiment. Superseded by the C
  decoder; kept for history and may not reflect the verified parameters.
- **`urh_alutech.complex16s`** — raw IQ capture (URH, complex16s).
- **`g012_868M_1000k.cu8`** / **`g025_868M_1000k.cu8`** — 868 MHz captures used to
  verify the CRC. **`g001/g002-BTN1_433.92M_250k.cu8`** — 433.92 MHz captures.

## Working with the Decoder

The C decoder is built into rtl_433 (add to `src/devices/`, declare in
`include/rtl_433_devices.h`, list in `src/CMakeLists.txt` / `Makefile.am`). Once
built it registers as protocol **321**.

```bash
# Live capture (encrypted output only, no secret needed):
rtl_433 -f 868.35M -s 1024k -R 321

# With the six magic constants, also decode serial / button / counter:
rtl_433 -f 868.35M -s 1024k -R 321:<m0>,<m1>,<m2>,<m3>,<m4>,<m5>
```

Flex-test the raw parameters against a capture without building anything. The
~4150 µs gap splits each burst into a 12-bit preamble row and a 72-bit data row,
so keep `gap_limit` below it:

```bash
rtl_433 -r g012_868M_1000k.cu8 -R 0 \
    -X 'n=alutech,m=OOK_PWM,s=400,l=800,r=4400,g=1500,t=160'
```

Analyze a capture in Universal Radio Hacker (URH):

```bash
urh urh_alutech.complex16s
```

## Protocol Details

| Parameter | Value |
|-----------|-------|
| Modulation | OOK_PWM (ASK/OOK) |
| Frequency | 868.35 MHz (some units 433.92 MHz) |
| Total bits | 72 (= 8 payload bytes + 1 CRC byte) |
| Short pulse (1) | ~370–400 µs |
| Long pulse (0) | ~740–800 µs |
| Preamble | `0xfff` (12 × 1-bits) |
| Post-preamble gap | ~4150 µs |
| Repeats per keypress | 3 (older units) / 6 (new PY32 units) |
| Bit order | each on-air byte is LSB-first (`reverse8` before use) |
| Integrity | CRC-8, poly `0x31`, init `0xff`, reflected I/O, over the 8 payload bytes |

**Frame layout.** On air the 8 payload bytes are **encrypted**; a flex decoder or
the no-key C decoder only ever sees the encrypted `code` plus the CRC. The
plaintext fields below appear only *after* running the Feistel decrypt with the
six magic constants:

| Decrypted byte | Field |
|----------------|-------|
| dec[0] | button (`0xff`→1, `0x11`→2, `0x22`→3, `0x33`→4, `0x44`→5) |
| dec[1..2] | rolling counter (little-endian) |
| dec[3..6] | serial (little-endian) |
| dec[7] | internal check byte |

The six magic constants are a per-manufacturer secret shared across the AT-4N
product line. They are **not public** — Flipper ships them AES-encrypted and
decrypts them in its secure enclave — so no key is embedded in this repo. See the
security note below.

## Security Boundary

Help with: decoding/validating frames, dumping the **user's own** PY32 firmware,
generic crypto tooling, and behavioral rolling-code analysis. Do **not** help
extract Flipper's secure-enclave AES key or otherwise source the shared
manufacturer magic constants — those would let anyone clone arbitrary AT-4N
remotes. The user must supply constants they are entitled to (e.g. recovered from
their own hardware).

## Next Steps / Open TODOs

- Land the optional-decryption feature for `alutech_at_4n.c` on its branch (it is
  currently only in a git stash — rescue it).
- Submit the decoder upstream to
  [rtl_433 devices](https://github.com/merbanan/rtl_433/tree/master/src/devices).
- Decode the 433.92 MHz captures (`g001/g002`) and confirm the CRC there too.
- Behavioral replay / rolling-code resync analysis with the Flipper Zero.
