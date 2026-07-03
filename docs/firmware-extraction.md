# Extracting the firmware from an AT-4N-868 keyfob

> **Scope:** this documents dumping the firmware from **your own** remote's MCU so
> you can study the rolling-code implementation. It does **not** cover, and this
> project will not help with, obtaining the shared per-manufacturer "rainbow
> table" secret that would let you clone arbitrary AT-4N remotes.

The transmitter is built around a **PUYA PY32F002A** — an Arm Cortex-M0+ MCU
(marked `Puya` on U1). The firmware lives in on-chip flash at `0x08000000`
(20 KB on this part). You read it out over the **SWD** debug port.

## 1. Hardware you need

| Item | Notes |
| --- | --- |
| SWD debug probe | **USBDM** (in ARM SWD mode), ST-Link V2, or a CMSIS-DAP probe all work. |
| Jumper wires / test hooks | The JP1 pads are small; hook clips or a pogo jig help. |
| Multimeter | To sanity-check pin identity before powering anything. |
| Host software | OpenOCD or `pyocd` (PY32 has community support), or the USBDM GDB server. |

The PY32F002A is a 3.3 V part. Power it from the coin cell already in the fob, or
from the probe's 3V3 — **never both at once**.

## 2. The JP1 debug header

The board carries a labelled 5-pad header, **JP1**. Reading the silk top-to-bottom:

| JP1 label | Signal | PY32 pin |
| --- | --- | --- |
| `V`  | VCC / 3V3 | VDD |
| `D`  | SWDIO | PA13 |
| `G`  | GND | VSS |
| `CL` | SWCLK | PA14 |
| `NR` | NRST | NRST |

### Confirm with a multimeter before wiring

Do this with the battery **in**, probe **disconnected**:

- `G` → continuity to the battery negative terminal / ground plane.
- `V` → reads the battery voltage (~3 V) against `G`.

If `G` and `V` are swapped from what you expect, stop and re-check the silk
orientation — powering the part backwards will kill it.

## 3. Wiring to the probe

Minimum connection is four wires:

```
JP1            probe
 V  (VDD)  --- 3V3   (only if NOT using the coin cell)
 D  (SWDIO)--- SWDIO
 CL (SWCLK)--- SWCLK
 G  (GND)  --- GND
 NR (NRST) --- RESET (optional but recommended; lets the probe
                      connect-under-reset if the firmware reconfigures
                      the SWD pins as GPIO)
```

Keep the leads short — SWD is happier at the lower adapter clock speeds if the
wiring is long.

## 4. Read the protection state first

Before trying to dump anything, check the **option bytes / RDP (read-out
protection)** level. If the vendor set RDP, the debug port will refuse to read
flash, and on many parts an attempt plus a level change will **mass-erase** the
chip — so look, don't poke.

With OpenOCD (adjust the target/config for your probe):

```bash
openocd -f interface/cmsis-dap.cfg -f target/py32f0x.cfg \
    -c "init; halt; flash probe 0; mdw 0x1FFF0000 4; shutdown"
```

- If `halt` succeeds and memory reads back, RDP is **disabled** — proceed.
- If the connect fails or every read returns `0x00000000` / errors, RDP is likely
  **enabled** and a non-invasive dump is not possible (see §6).

## 5. Dump the flash

Once you can halt the core, read the 20 KB flash region:

```bash
openocd -f interface/cmsis-dap.cfg -f target/py32f0x.cfg \
    -c "init; halt; dump_image at4n_flash.bin 0x08000000 0x5000; shutdown"
```

`0x5000` = 20480 bytes. Adjust if your part reports a different flash size.

With `pyocd` instead:

```bash
pyocd cmd -t py32f002ax5 -c "halt" -c "savemem 0x08000000 0x5000 at4n_flash.bin"
```

Verify you got real code, not a blank/`0xFF` image:

```bash
xxd at4n_flash.bin | head          # expect a vector table at the top
```

The first word is the initial stack pointer (`0x2000xxxx` for RAM), the second is
the reset-handler address (`0x0800xxxx`) — a good quick sanity check.

## 6. If RDP is enabled

If the part is read-protected, the SWD route is closed without invasive attacks
(voltage glitching, decapping) which are out of scope here. In that case your
options for studying the protocol are:

- Capture and analyse the **RF** side, which is what the rest of this project
  does — you do not need the firmware to validate the frame, CRC, or observe the
  rolling-code behaviour.
- Recover the encryption constants only from a source you are entitled to.

## 7. What to do with the dump

Once you have `at4n_flash.bin`, disassemble it (Ghidra, `arm-none-eabi-objdump
-D -b binary -m armv6-m`, or Cutter) and look for:

- the CRC-8 routine (poly `0x31`) applied per-frame,
- the XTEA-style Feistel round that mixes the payload with six 32-bit constants,
- those six constants themselves, which the firmware must hold to encrypt.

That closes the loop with the [rtl_433 decoder](../src/devices/alutech_at_4n.c),
whose optional decryption path takes exactly those six values as
`-R 321:<m0>,<m1>,<m2>,<m3>,<m4>,<m5>`.
