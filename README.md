# ALUTECH AT-4N-868
Reversere engineer an garage door remote key fob 

![AT-4N-868](AT_4N_868.png) 


## Hypotheis
* Can this radio protocoll be decoded in rtl_433 and is there any vulnerabilities 
* Create and Flex decoder alutech.conf file based on examples from [rtl_433 conf](https://github.com/merbanan/rtl_433/tree/master/conf)
* Create and C version and possible merge to [rtl_433 devices](https://github.com/merbanan/rtl_433/tree/master/src/devices)
* Is it possibe to open grage door with an flipper

## Datasheets 
| No       | Description | IC           |
| ---      | ---         |---           |
|        | Probably         | [Microchip HCS301](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/21143C.pdf)  |

## Reverse engineering
Universal radio Hacker (URH) recored and try to decode the data from one [keypress](urh_alutech.complex16s) 
* It's 868,35MHz ASK alias OOK in URH 
* It's a rolling message, so a simple clone is not enough 
* There is an preaamble of 6 a in hex and the message is repeated 3 times for each keypres
* An Pause threshold of 10 under modulation is enough to get preamble and message to be in one message
* ~~Converting from URH to RTL Flex decoder using this as~~ [inspiration](https://github.com/klohner/klohner.github.io/blob/master/SDR/Decoding/Example_2019-01-24/README.md)

Aftear looking at datasheet and the recorded data
* There is ~9350 us with 12 1 bit ( ~389 us)
* A break for ~4150 us
* A 1 is coded as ~370 us short signal (1x)
* A 0 is coded as ~760 us long signal (2x)
* And it's whole signal is ~ 1123 us  (3x)

![AT-4N-868](urh_AT_4N_868.png)
