#define nodeId 1

#define RH_TEST_NETWORK 3
// This network looks like 1-2-4
//                         |   |
//                         --3--

#define N_NODES  4
#define targetNode 1

#if defined(__AVR)
//mothbot
#define RFM95_CS 8
#define RFM95_RST 7
#define RFM95_INT 2
#define LED 4

RH_RF95 rf95(RFM95_CS, RFM95_INT);

#elif defined(HELTEC_WIFI_LORA_32_V2)

// heltec wifi lora 32 v2
#define RFM95_CS 18
#define RFM95_RST 14
#define RFM95_INT 26
#define LED 25

RH_RF95 rf95(RFM95_CS, RFM95_INT);


#elif defined(FEATHER_ESP32)

// feather esp32
#define RFM95_CS 14
#define RFM95_RST 27
#define RFM95_INT 15
#define LED 13

RHSoftwareSPI sx1278_spi;

RH_RF95 rf95(RFM95_CS, RFM95_INT, sx1278_spi);

#endif
