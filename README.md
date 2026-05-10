# ALUTECH AT-4N-868

Reverse engineering a garage door remote key fob.

![AT-4N-868](AT_4N_868.png)

## Hypothesis

* Can this radio protocol be decoded in rtl_433, and are there any vulnerabilities?
* Create a flex decoder `alutech.conf` file based on examples from [rtl_433 conf](https://github.com/merbanan/rtl_433/tree/master/conf).
* Create a C version and possibly merge it into [rtl_433 devices](https://github.com/merbanan/rtl_433/tree/master/src/devices).
* Is it possible to open the garage door with a Flipper?

## Datasheets

| No  | Description | IC                                                                                                                            |
| --- | ---         | ---                                                                                                                           |
|     | Probably    | [Microchip HCS301](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/21143C.pdf)   |

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
* A previous version of the chip, [HCS200](https://github.com/merbanan/rtl_433/blob/master/src/devices/hcs200.c), has a decoder in rtl_433, but it fails here because at 868.35 MHz it picks up 72 bits instead of 66.

![AT-4N-868](urh_AT_4N_868.png)
