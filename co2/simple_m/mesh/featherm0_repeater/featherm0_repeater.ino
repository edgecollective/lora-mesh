
#include <RHSoftwareSPI.h> 
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#define RH_HAVE_SERIAL
#include <SPI.h>
#include <Arduino.h>
#include "config.h"

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
//#define gatewayNode 1

// Class to manage message delivery and receipt, using the driver declared above
RHMesh *manager;

// Radio pins for feather M0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

#define LED 13

RH_RF95 rf95(RFM95_CS, RFM95_INT);

long lastMeasureTime = 0;  // the last time the output pin was toggled
long measureDelay = 5000;

void setup() {
   
  Serial.begin(115200);

  manager = new RHMesh(rf95, this_node_id);

  if (!manager->init()) {
    Serial.println(F("mesh init failed"));
   
  } else {
   
    delay(1000);
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

  pinMode(LED, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {

uint8_t buf[10];
uint8_t len = sizeof(buf);
lastMeasureTime = millis();

bool relay_status = manager->recvfromAckTimeout((uint8_t *)buf, &len, measureDelay);

if (  (millis() - lastMeasureTime) < measureDelay) {

  // then we relayed something via the mesh
digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
delay(10);                       // wait for a second
digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
delay(10);                       // wait for a second
}
} 
