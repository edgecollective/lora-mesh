
#include <RHSoftwareSPI.h> 
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#define RH_HAVE_SERIAL
#include <SPI.h>
#include "config.h"
#include <U8x8lib.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"  //  https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library
#include <ArduinoJson.h> //https://arduinojson.org/v6/doc/installation/
#include <BMP388_DEV.h> // https://github.com/MartinL1/BMP388_DEV
#include <Bounce2.h> // https://github.com/thomasfredericks/Bounce2

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
#define targetNode 1


// Class to manage message delivery and receipt, using the driver declared above
RHMesh *manager;

typedef struct {
  float co2;
  int node_id; 
  int next_hop;
  int next_hop_rssi;
} Payload;
Payload theData;


#define LORA_IRQ 15
#define LORA_CS 14
#define LORA_SCK 26 //A0
#define LORA_MOSI 21
#define LORA_MISO 25 //A1
#define LORA_RST 27

//#define LED 13

RHSoftwareSPI sx1278_spi;

RH_RF95 rf95(LORA_CS, LORA_IRQ, sx1278_spi);

int NAcounts=0;
int NAthreshold = 10;
boolean connectioWasAlive = true;

WiFiMulti wifiMulti;

SCD30 airSensor;


U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 17, /* data=*/ 16, /* reset=*/ 39);

long lastMeasureTime = 0;  // the last time the output pin was toggled

long measureDelay = interval_sec*1000;

String post_url="http://"+String(bayou_base_url)+"/"+String(feed_pubkey);

String short_feed_pubkey = String(feed_pubkey).substring(0,3);

void setup() {

  u8x8.begin();
  
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
   u8x8.clear();
   u8x8.setCursor(0,0); 
   u8x8.print("Starting...");
   delay(1000);
   
  //pinMode(LED, OUTPUT);
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
  
  Serial.print(F("initializing node "));

  sx1278_spi.setPins(LORA_MISO, LORA_MOSI, LORA_SCK);

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

     //co2 still warming up
     delay(1000);
     //u8x8.clear();
    //u8x8.setFont(u8x8_font_7x14B_1x2_f);
    //u8x8.setFont(u8x8_font_inr33_3x6_f);
    //u8x8.setFont(u8x8_font_7x14B_1x2_f);
    u8x8.setCursor(0,4); 
    u8x8.print("warming up...");
    delay(3000);
    
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

if (airSensor.dataAvailable())
  {

  int co2 = airSensor.getCO2();
  float temp = roundf(airSensor.getTemperature()* 100) / 100;
  float humid = roundf(airSensor.getHumidity()* 100) / 100;

  if (co2 > 0) {
    
    Serial.println(co2);

    //display CO2 reading
    u8x8.clear();
    //u8x8.setFont(u8x8_font_7x14B_1x2_f);
    //u8x8.setFont(u8x8_font_inr33_3x6_f);
    u8x8.setFont(u8x8_font_inb21_2x4_n);
    u8x8.setCursor(0,0); 
    u8x8.print(co2);

    // send data to targetNode via lora mesh
    if (nodeId != targetNode) {

    theData.co2 = co2;
    theData.node_id = nodeId;
    theData.next_hop_rssi = rf95.lastRssi();
    RHRouter::RoutingTableEntry *route = manager->getRouteTo(targetNode);
    if (route != NULL) {
    theData.next_hop = route->next_hop;
    }
    else
    {
    theData.next_hop = 0; // consider this an 'error' message of sorts
    }
    
    // send an acknowledged message to the target node
    uint8_t error = manager->sendtoWait((uint8_t *)&theData, sizeof(theData), targetNode);
    if (error != RH_ROUTER_ERROR_NONE) {
      Serial.println();
      Serial.print(F(" ! "));
      Serial.println(getErrorString(error));
    } else {
      Serial.println(F(" OK"));
    }
    }

  }
  else {

    //co2 still warming up
     u8x8.clear();
    //u8x8.setFont(u8x8_font_7x14B_1x2_f);
    //u8x8.setFont(u8x8_font_inr33_3x6_f);
    u8x8.setFont(u8x8_font_7x14B_1x2_f);
    u8x8.setCursor(0,3); 
    u8x8.print("(warming up)");
    delay(2000);
  }  
  }

    // listen for incoming LoRa messages to relay in between readings
    int waitTime = measureDelay + random(3000, 5000);
    uint8_t buf[sizeof(Payload)];
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, waitTime, &from)) {
      theData = *(Payload*)buf;
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
