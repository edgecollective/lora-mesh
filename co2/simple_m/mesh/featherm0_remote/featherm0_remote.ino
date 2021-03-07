
#include <RHSoftwareSPI.h> 
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#define RH_HAVE_SERIAL
#include <SPI.h>
#include <U8x8lib.h>
#include <Arduino.h>
#include <Bounce2.h> // https://github.com/thomasfredericks/Bounce2
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"  //  https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library
#include "config.h"

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
#define gatewayNode 1

SCD30 airSensor;

//U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

// Class to manage message delivery and receipt, using the driver declared above
RHMesh *manager;

typedef struct {
  int co2;
  float temperature;
  float humidity;
  float light;
  char loranet_pubkey[13]; //pubkey for this lora network (same as bayou pubkey of node #1 / the gateway node)
  char pubkey[13]; // bayou pubkey for this node
  char privkey[13]; // bayou privkey for this node
  int node_id; 
  int next_hop;
  int next_rssi;
  int logcode;
} Payload;

Payload theData;

// Radio pins for feather M0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

#define LED 13

RH_RF95 rf95(RFM95_CS, RFM95_INT);

long lastMeasureTime = 0;  // the last time the output pin was toggled
long measureDelay = interval_sec*1000;

void setup() {

  pinMode(LED, OUTPUT);

  u8x8.begin();
  
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
   u8x8.clear();
   u8x8.setCursor(0,0); 
   u8x8.print("Starting...");
   delay(1000);
   
  Serial.begin(115200);

  Wire.begin();

  if (airSensor.begin() == false)
  {
     u8x8.setCursor(0,2); 
   u8x8.print("SCD30 missing?");
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }
  u8x8.setCursor(0,2); 
   u8x8.print("SCD30 detected!");
   delay(1000);
   
  manager = new RHMesh(rf95, this_node_id);

  if (!manager->init()) {
    Serial.println(F("mesh init failed"));
    u8x8.setCursor(0,4); 
    u8x8.print("LoRa fail!");
  } else {
    u8x8.setCursor(0,4); 
    u8x8.print("LoRa working!");
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

    // warm up CO2 sensor by waiting 2 sec
    //co2 still warming up
    u8x8.setCursor(0,6); 
    u8x8.print("Warming up...");
    delay(3000);
    u8x8.clear();
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


int firstLoop = 1;
int logcode = 1;

int co2;
float temperature;
float humidity;
float light;

// create a first random interval
int epsilon_interval = random(0,random_interval_sec*1000);


void loop() {

// on the first loop, or at a regular (but slightly randomized) interval, make a measurement
// if we're the gateway, post it to Bayou; if we're a remote node, send it to the lora network

if (  ( (millis() - lastMeasureTime) > (measureDelay)) || firstLoop) {

if (airSensor.dataAvailable()) {

  co2 = airSensor.getCO2();

  if (co2 > 0) {

    // send to serial
    Serial.print("co2:");
    Serial.println(co2);

    // send to display
    u8x8.clear();

    u8x8.setFont(u8x8_font_inb21_2x4_n);
    u8x8.setCursor(0,2);
    u8x8.print("    "); 
    u8x8.setCursor(0,2);
    u8x8.print(co2);

    //display our node id
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.setCursor(9,2);
    u8x8.print("node:");
    u8x8.print(this_node_id);

     //display our pubkey
    //u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.setCursor(0,0);
    u8x8.print("K:");
    u8x8.print(this_node_pubkey);

    //display our loranet_pubkey
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.setCursor(0,7);
    u8x8.print("L:");
    u8x8.print(loranet_pubkey);
    
    
  // make rest of measurement
  temperature = roundf(airSensor.getTemperature()* 100) / 100;
  humidity = roundf(airSensor.getHumidity()* 100) / 100;
  light = 0; // if we add a light sensor, can make this real

    sendToMesh();
 
  
  firstLoop = 0; // we did our thing successfully for the first loop, so we're out of that mode
  lastMeasureTime = millis(); // reset the interval timer 
  epsilon_interval = random(0,random_interval_sec*1000); //create a new randomized interval
    
} // co2 > 0
} // airSensor avail
} // measureTime
else {
  // we're here because we received a packet to relay
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(10);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(10);         
}

int listenTime = measureDelay+epsilon_interval;
relayFromMesh(listenTime);

} // loop


void relayFromMesh(int waitTime) {

// run for waitTime millis, listening for and relaying mesh messages
// if the intended recipient is us, we're the gateway, and we should post the incoming packet data to Bayou

    uint8_t buf[10];
    uint8_t len = sizeof(buf);
    bool relay_status = manager->recvfromAckTimeout((uint8_t *)buf, &len, waitTime);

}


int sendToMesh() {
  // we are a remote node, and: it's time to measure, or we're in the first loop

    Serial.println(co2);

    theData.co2 = co2;
    theData.temperature = temperature;
    theData.humidity = humidity;
    theData.light = light;
    memcpy(theData.loranet_pubkey,loranet_pubkey,13);
    memcpy(theData.pubkey,this_node_pubkey,13);
    memcpy(theData.privkey,this_node_privkey,13);
    theData.node_id = this_node_id;
    theData.next_rssi = rf95.lastRssi();
    theData.logcode = logcode;
    RHRouter::RoutingTableEntry *route = manager->getRouteTo(gatewayNode);
    if (route != NULL) {
    theData.next_hop = route->next_hop;
    }
    else
    {
    theData.next_hop = 0; // consider this an 'error' message of sorts
    }
    
    
    // send an acknowledged message to the target node
    uint8_t error = manager->sendtoWait((uint8_t *)&theData, sizeof(theData), gatewayNode);
    if (error != RH_ROUTER_ERROR_NONE) {
      Serial.println();
      Serial.print(F(" ! "));
      Serial.println(getErrorString(error));

          u8x8.setFont(u8x8_font_chroma48medium8_r);
          //u8x8.setCursor(9,1);
          //u8x8.print("        ");
          u8x8.setCursor(9,3);
            u8x8.print("      ");
            u8x8.setCursor(9,4);
            u8x8.print("ROUTE?");
            u8x8.setCursor(9,5);
            u8x8.print("      ");
          
          
    } else {

          u8x8.setFont(u8x8_font_chroma48medium8_r);
          //u8x8.setCursor(9,1);
          //u8x8.print("        ");
          if (theData.next_hop==0) {
            u8x8.setCursor(9,3);
            u8x8.print("     ");
            u8x8.setCursor(9,4);
            u8x8.print("routing");
            u8x8.setCursor(9,5);
            u8x8.print("      ");
          }
          else {
            u8x8.setCursor(9,3);
            u8x8.print("hop:");
            u8x8.print(theData.next_hop);
            u8x8.setCursor(9,4);
            u8x8.print("rssi:");
            u8x8.setCursor(9,5);
            u8x8.print("  ");
            u8x8.print(theData.next_rssi);
            Serial.println(F("OK"));
          }
          
    }
    
}
