# ALUTECH AT-4N-868

Reverse engineering a garage door remote key fob.

![AT-4N-868](AT_4N_868.png)

## Hypothesis

* Can this radio protocol be decoded in rtl_433, and are there any vulnerabilities?
* Create a flex decoder `alutech.conf` file based on examples from [rtl_433 conf](https://github.com/merbanan/rtl_433/tree/master/conf).
* Create a C version and possibly merge it into [rtl_433 devices](https://github.com/merbanan/rtl_433/tree/master/src/devices).
* Is it possible to open the garage door with a Flipper? -> Probably not as this is quite advanced rotating algorithm
* I assume this is a Microchip HCS301, based on looking arround but hard to say as the chip have no markings
  * Bought a new one from Aliexpress and here the U1 is Arm Cortex-M0+ and that means it's contains some own software 

  
## Steps
[ ] Decode Sampling rates with [I/Q Spectrogram & Pulsedata](https://triq.org/pdv3/)
[ ] Add samples to tests/Microchip merge [rtl_433 tests](https://github.com/merbanan/rtl_433_tests)
[ ] Create a C version and possibly merge it into [rtl_433 devices](https://github.com/merbanan/rtl_433/tree/master/src/devices)

## Datasheets

| No  | Description    | IC                                                                                                                            |
| --- | ---            | ---                                                                                                                           |
|     | Probably       | [Microchip HCS301](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/21143C.pdf)   |
| U1  | Arm Cortex-M0+ | [PUYA F002AW15 SH6HN1B](https://www.puyasemi.com/download_path/%E6%95%B0%E6%8D%AE%E6%89%8B%E5%86%8C/MCU%20%E5%BE%AE%E5%A4%84%E7%90%86%E5%99%A8/PY32F002A_Datasheet_V0.2.pdf)|    

## Reverse engineering

Universal Radio Hacker (URH) recorded and tried to decode the data from one [keypress](urh_alutech.complex16s).

* It is 868.35 MHz ASK (a.k.a. OOK in URH).
* It uses a rolling code, so a simple clone is not enough.
* There is a preamble of 6 `a` in hex, and the message is repeated 3 times per keypress.
* A pause threshold of 10 under modulation is enough to capture the preamble and message in a single message.
* ~~Converting from URH to an RTL flex decoder using this as~~ [inspiration](https://github.com/klohner/klohner.github.io/blob/master/SDR/Decoding/Example_2019-01-24/README.md).

After looking at the datasheet and the recorded data:

* There is a ~9350 µs window with 12 one-bits (~389 µs each).
* Followed by a ~4150 µs break.
* A `1` is encoded as a ~370 µs short signal (1x).
* A `0` is encoded as a ~760 µs long signal (2x).
* The full symbol period is ~1123 µs (3x).
* A previous version of the chip, [HCS200](https://github.com/merbanan/rtl_433/blob/master/src/devices/hcs200.c), has a decoder in rtl_433, but it fails here because at 868.35 MHz it picks up 72 bits instead of 66?
* The new Arm Cortex-M0+ repeats the message 6 times instead of 3 

## rtl_433 decoder

A C decoder now lives at [`src/devices/alutech_at_4n.c`](src/devices/alutech_at_4n.c)
(submitted upstream to [rtl_433](https://github.com/merbanan/rtl_433)). It registers
as protocol **321**.

### Frame format

Each keypress sends a 12-bit preamble (`0xfff`), a ~4150 µs gap, then a 72-bit
payload repeated several times. The 72 bits are 9 bytes:

| Bits   | Field                       |
| ---    | ---                         |
| 0..63  | encrypted payload (8 bytes) |
| 64..71 | CRC-8                       |

* Each on-air byte is sent **LSB first**, so every byte is bit-reversed
  (`reverse8`) before processing.
* Integrity is a **CRC-8, poly `0x31`, init `0xff`, reflected I/O** computed over
  the eight payload bytes and compared against the (reversed) CRC byte.

With no secret at all the decoder validates the CRC and emits the raw encrypted
64-bit `code`. That code is part of the rolling sequence, so it changes on every
press — enough for presence/keypress detection but not for cloning.

```
rtl_433 -f 868.35M -s 1024k -R 321
```

```
model : Alutech-AT4N   Encrypted : 7EA354AE42FC6594   Integrity : CRC
```

### Optional decryption (serial / button / counter)

The 64-bit payload is encrypted with an **XTEA-style Feistel cipher** keyed by
**six 32-bit per-manufacturer constants** (a "rainbow table" shared across the
whole AT-4N product line). These constants are **not public** — they ship only
AES-encrypted inside a Flipper Zero's keystore and are decrypted on-device by its
secure-enclave key. They are never transmitted during pairing and cannot be
brute-forced (~128-bit keyspace). For that reason **no key is embedded in
rtl_433**; you supply it at runtime:

```
rtl_433 -f 868.35M -s 1024k -R 321:<m0>,<m1>,<m2>,<m3>,<m4>,<m5>
```

* The six values are 32-bit hex in keystore order, separated by any non-hex
  character (comma/colon/space).
* The decrypted block carries its own check byte, so the decoder only trusts the
  fields when that check verifies. A **wrong or partial key silently falls back
  to encrypted-only output** rather than printing garbage — so if you pass dummy
  values you will still only see the `Encrypted` field.
* When a correct key is given the decoder additionally emits **`id`** (serial),
  **`button`** (1–5), and **`counter`** (rolling code value).

Decrypted field layout (after running the cipher over the bit-reversed payload):

| Byte    | Field                                  |
| ---     | ---                                    |
| dec[0]  | button (`0xff`→1, `0x11`→2, `0x22`→3, `0x33`→4, `0x44`→5) |
| dec[1..2] | rolling counter (little-endian)      |
| dec[3..6] | serial (little-endian)               |
| dec[7]  | internal check byte                    |

The cipher and field layout were ported from the Flipper Zero project
(`lib/subghz/protocols/alutech_at_4n.c`).

## Images
![front](front.jpg)
![back](back.jpg)
![back_new](back_new.jpg)
