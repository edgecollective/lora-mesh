#define nodeId 1

#define RH_TEST_NETWORK 3
// This network looks like 1-2-4
//                         |   |
//                         --3--


#define N_NODES  4
#define targetNode 1
#define LED 25

#if defined(__AVR)
//mothbot
#define RFM95_CS 8
#define RFM95_RST 7
#define RFM95_INT 2

#else

// heltec wifi lora 32 v2
#define RFM95_CS 18
#define RFM95_RST 14
#define RFM95_INT 26

#endif
