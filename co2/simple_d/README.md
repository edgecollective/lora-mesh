
note: mesh seems to work on the feather esp32 setup here, but not the 'test network'? (i.e. the packets aren't limited by the 'block' in RH_TEST_NETWORK for the feather esp32 node). big difference is that feather esp32 uses software defined SPI bus for the lora radio. going to hack on this in 'simple_e' version.

aside: working version was with:

heltec as node 1
heltec as node 2
feather m0 as node 3
feather m0 as node 4



