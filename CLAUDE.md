# CLAUDE.md

Guidance for Claude Code in this repo.

## Project

Hardware RE of the **ALUTECH AT-4N-868** garage door remote (4-button,
868.35 MHz rolling code transmitter). Goal: decode the protocol with
rtl_433, understand the rolling code, check Flipper Zero compatibility.

U1 = PUYA PY32F002A, Arm Cortex-M0+, custom firmware. Proprietary protocol,
matches Flipper's `alutech_at_4n` decoder: 72-bit frame, CRC-8 poly `0x31`,
XTEA-style Feistel cipher keyed by six per-manufacturer constants.

Full protocol write-up, frame layout, and usage: [README.md](README.md).

## Key Files

- **C rtl_433 decoder NOT vendored here.** Lives in a fork
  (`andy778/rtl_433`, `src/devices/alutech_at_4n.c`, commit `5f0a1d6`),
  headed upstream. Do not re-add a copy — link to the fork.
- **`docs/firmware-extraction.md`** — dumping PY32 firmware over SWD (JP1),
  own hardware only.
- **`research/flipper/`** — public AES-encrypted Flipper keystore + decrypt
  helper (needs the AES key, not included).
- **`alutech.conf`** — early flex-decoder experiment, superseded by the C
  decoder, may not match verified parameters.
- **`urh_alutech.complex16s`**, **`g012_868M_1000k.cu8`**,
  **`g025_868M_1000k.cu8`** — raw captures, not tracked in git. Regenerate
  with `rtl_433 -r <file> ...` or re-capture. **Only capture/commit from a
  keyfob not paired to a real garage door** — a decrypted capture reveals
  the fob's serial number to anyone who later obtains the rainbow table.

## Working with the Decoder

Build from the `andy778/rtl_433` fork (device already declared in
`include/rtl_433_devices.h`, `src/CMakeLists.txt`/`Makefile.am`). Registers
as protocol **321**.

```bash
rtl_433 -f 868.35M -s 1024k -R 321                          # encrypted only
rtl_433 -f 868.35M -s 1024k -R 321:<m0>,...,<m5>             # + serial/button/counter
```

Flex-test without building — keep `gap_limit` below the ~4150 µs gap:

```bash
rtl_433 -r g012_868M_1000k.cu8 -R 0 \
    -X 'n=alutech,m=OOK_PWM,s=400,l=800,r=4400,g=1500,t=160'
```

URH: `urh urh_alutech.complex16s`

## Security Boundary

Help with: decoding/validating frames, dumping the **user's own** PY32
firmware, generic crypto tooling, behavioral rolling-code analysis.

Do **not** help extract Flipper's secure-enclave AES key or otherwise source
the shared manufacturer magic constants — lets anyone clone arbitrary AT-4N
remotes. User supplies constants they're entitled to (own hardware).

## Open TODOs

- Submit fork's decoder upstream to [rtl_433 devices](https://github.com/merbanan/rtl_433/tree/master/src/devices).
- Behavioral replay / resync analysis with Flipper Zero.
