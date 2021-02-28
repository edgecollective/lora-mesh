#include <RHSoftwareSPI.h> 
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#define RH_HAVE_SERIAL
#include <SPI.h>
#include "config.h"


// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
#define targetNode 1
//#define RH_TEST_NETWORK 3

// Class to manage message delivery and receipt, using the driver declared above
RHMesh *manager;

typedef struct {
  float co2;
  int node_id; 
  int next_hop;
  int next_hop_rssi;
} Payload;
Payload theData;


#if defined(__AVR)
//mothbot
#define RFM95_CS 8
#define RFM95_RST 7
#define RFM95_INT 2
#define LED 4

#else

// heltec wifi lora 32 v2
#define RFM95_CS 18
#define RFM95_RST 14
#define RFM95_INT 26
#define LED 25

#endif

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {


  //pinMode(LED, OUTPUT);
  Serial.begin(115200);
  //while (!Serial) ; // Wait for serial port to be available

  Serial.print("RFM95_CS:");
  Serial.println(RFM95_CS);
  
  Serial.print(F("initializing node "));
  
  manager = new RHMesh(rf95, nodeId);

  if (!manager->init()) {
    Serial.println(F("init failed"));
  } else {
    Serial.println("done");
  }
  rf95.setTxPower(23, false);
  rf95.setFrequency(915.0);
  rf95.setCADTimeout(500);

  // Possible configurations:
  // Bw125Cr45Sf128 (the chip default)
  // Bw500Cr45Sf128
  // Bw31_25Cr48Sf512
  // Bw125Cr48Sf4096

  // long range configuration requires for on-air time
  boolean longRange = false;
  if (longRange) {
    RH_RF95::ModemConfig modem_config = {
      0x78, // Reg 0x1D: BW=125kHz, Coding=4/8, Header=explicit
      0xC4, // Reg 0x1E: Spread=4096chips/symbol, CRC=enable
      0x08  // Reg 0x26: LowDataRate=On, Agc=Off.  0x0C is LowDataRate=ON, ACG=ON
    };
    rf95.setModemRegisters(&modem_config);
    if (!rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096)) {
      Serial.println(F("set config failed"));
    }
  }

  Serial.println("RF95 ready");

}

const __FlashStringHelper* getErrorString(uint8_t error) {
  switch(error) {
    case 1: return F("invalid length");
    break;
    case 2: return F("no route");
    break;
    case 3: return F("timeout");
    break;
    case 4: return F("no reply");
    break;
    case 5: return F("unable to deliver");
    break;
  }
  return F("unknown");
}

void loop() {


    if (nodeId != targetNode) {

    //RHRouter::RoutingTableEntry *route = manager->getRouteTo(targetNode);
    theData.co2 = 400;
    theData.node_id = nodeId;
    //theData.next_hop = route->next_hop;
    theData.next_hop = 1;
    theData.next_hop_rssi = rf95.lastRssi();
    
    // send an acknowledged message to the target node
    uint8_t error = manager->sendtoWait((uint8_t *)&theData, sizeof(theData), targetNode);
    if (error != RH_ROUTER_ERROR_NONE) {
      Serial.println();
      Serial.print(F(" ! "));
      Serial.println(getErrorString(error));
    } else {
      Serial.println(F(" OK"));
      /*
      // we received an acknowledgement from the next hop for the node we tried to send to.
      RHRouter::RoutingTableEntry *route = manager->getRouteTo(targetNode);
      if (route->next_hop != 0) {
        //rssi[route->next_hop-1] = rf95.lastRssi();
        Serial.print("next_hop=");
        Serial.println(route->next_hop);
      }
      */
    }
    }

    // listen for incoming messages. Wait a random amount of time before we transmit
    // again to the next node
    unsigned long nextTransmit = millis() + random(3000, 5000);
    while (nextTransmit > millis()) {
      int waitTime = nextTransmit - millis();
      //uint8_t len = sizeof(buf);
      uint8_t buf[sizeof(Payload)];
      uint8_t len = sizeof(buf);
      uint8_t from;
      if (manager->recvfromAckTimeout((uint8_t *)buf, &len, waitTime, &from)) {
        theData = *(Payload*)buf;
        //buf[len] = '\0'; // null terminate string
        Serial.print(from);
        Serial.print(F("->"));
        Serial.print(F(" :"));
        Serial.print("node_id = ");
        Serial.print(theData.node_id);
        Serial.print("; next_hop = ");
        Serial.println(theData.next_hop);
        //Serial.println(3);
      }
    }

  

}
