# ALUTECH AT-4N-868
Reversere engineer an garage door remote key fob 

![AT-4N-868](AT_4N_868.png) 


## Hypotheis
* Can this radio protocoll be decoded i rtl_433 and is there any vulnerabilities 
* Create and Flex decoder alutech.conf file based on examples from [rtl_433 project](https://github.com/merbanan/rtl_433/tree/master/conf)
* Create and C version
* Is it possibe to open grage door with an flipper

## Reverse engineering
Universal radio Hacker (URH) recored and try to decode the data from one [keypress](urh_alutech.complex16s) 
* It's 868,35MHz ASK
* It's rotating the message, so a simple clone is not enough 
* There is an preaamble of 6 a in hex and the message is repeated 3 times for each keypres
* An Pause threshold of 10 under modulation is enough to get preable and message to be in one message 

![AT-4N-868](urh_AT_4N_868.png)
