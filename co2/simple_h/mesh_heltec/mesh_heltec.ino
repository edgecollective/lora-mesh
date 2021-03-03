
#include <RHSoftwareSPI.h> 
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#define RH_HAVE_SERIAL
#include <SPI.h>
#include <U8x8lib.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> //https://arduinojson.org/v6/doc/installation/
#include <Bounce2.h> // https://github.com/thomasfredericks/Bounce2
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"  //  https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library
#include "config.h"

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
#define gatewayNode 1




SCD30 airSensor;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 17, /* data=*/ 16, /* reset=*/ 39);


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
  int log_code;
} Payload;
Payload theData;

// heltec wifi lora 32 v2
#define RFM95_CS 18
#define RFM95_RST 14
#define RFM95_INT 26
//#define LED 25

RH_RF95 rf95(RFM95_CS, RFM95_INT);
int NAcounts=0;
int NAthreshold = 10;
boolean connectioWasAlive = true;

WiFiMulti wifiMulti;

long lastMeasureTime = 0;  // the last time the output pin was toggled

long measureDelay = interval_sec*1000;

void setup() {

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

  if(this_node_id == gatewayNode) // we're the gateway, so try to connect to wifi
  {
    wifiMulti.addAP(SSID,WiFiPassword);
  }
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

  
void loop() {


if ( (millis() - lastMeasureTime) > (measureDelay+random(3000,5000)) || firstLoop) { // time to make a measurement and send


if (airSensor.dataAvailable())
  {

    co2 = airSensor.getCO2();
    temperature = roundf(airSensor.getTemperature()* 100) / 100;
    humidity = roundf(airSensor.getHumidity()* 100) / 100;
    light = 0;
    
  if (co2 > 0) {
    
  firstLoop = 0; // we'll do something this loop
  
  if (this_node_id != gatewayNode) { // then broadcast over LoRa 

  sendToMesh();
}
else { // we're the gateway and should post over HTTP

 postToBayou(feed_pubkey,feed_privkey);
        
}

lastMeasureTime = millis(); //set the current time

  }
  else {
   //co2 still warming up
     u8x8.clear();
    u8x8.setFont(u8x8_font_7x14B_1x2_f);
    u8x8.setCursor(0,3); 
    u8x8.print("(warming up)");
    delay(2000);
  }
}
}

/*

    int waitTime = measureDelay + random(3000, 5000);
    uint8_t buf[sizeof(Payload)];
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, waitTime, &from)) {
    // this service returns true only if *we* were the intended recipient. otherwise it is still important for relaying mesh messages.

     // the rest of this code only runs if we were the intended recipient; which means we're the gateway
      theData = *(Payload*)buf;
      Serial.print(from);
      Serial.print(F("->"));
      Serial.print(F(" :"));
      Serial.print("node_id = ");
      Serial.print(theData.node_id);
      Serial.print("; next_hop = ");
      Serial.println(theData.next_hop);

    
      //try to post online
      // first make sure wifi is connected!
      Serial.print("Connecting to wifi ...");
      while (wifiMulti.run() != WL_CONNECTED && NAcounts < NAthreshold )
      {
//      u8x8.setFont(u8x8_font_chroma48medium8_r);
//      u8x8.setCursor(10,2);
//      u8x8.print("wifi?");
//      u8x8.setCursor(10,3);
//      u8x8.print(NAcounts);
      Serial.print(".");
      NAcounts ++;
      delay(2000);
      }
      
      if (wifiMulti.run() != WL_CONNECTED) {
//      u8x8.setCursor(8,3);
//      u8x8.print("RESET");
      Serial.println("no wifi ... resetting!");
      delay(500);
      ESP.restart();
      }

      //Form the JSON:
        DynamicJsonDocument doc(1024);
        
        Serial.println(post_url);
        
        doc["private_key"] = feed_privkey;
        doc["co2"] =  theData.co2;
        //doc["tempC"]=temp;
        //doc["humidity"]=humid;
        //doc["mic"]=0.;
        //doc["auxPressure"]=aux_press;
        //doc["auxTempC"]=aux_temp;
        //doc["aux001"]=0.;
        //doc["aux002"]=0.;
        //doc["aux003"]=0.;
        //doc["log"]=0.;
         
        String json;
        serializeJson(doc, json);
        serializeJson(doc, Serial);
        Serial.println("\n");
        
        // Post online

        HTTPClient http;
        int httpCode;
        
        http.begin(post_url);
        http.addHeader("Content-Type", "application/json");
        Serial.print("[HTTP] Connecting ...\n");
      
        httpCode = http.POST(json);        

        if(httpCode== HTTP_CODE_OK) {
          
            Serial.printf("[HTTP code]: %d\n", httpCode);

            //u8x8.setFont(u8x8_font_7x14B_1x2_f);
//            u8x8.setFont(u8x8_font_chroma48medium8_r);
//            u8x8.setCursor(10,2);
//            u8x8.print(httpCode);
            
        } else {
            Serial.println(httpCode);
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            //u8x8.setFont(u8x8_font_7x14B_1x2_f);
//            u8x8.setFont(u8x8_font_chroma48medium8_r);
//              u8x8.setCursor(10,2);
//              u8x8.print(httpCode);
//              //u8x8.print(http.errorToString(httpCode).c_str());
//              u8x8.setCursor(8,3);
//              u8x8.print("RESET");
            Serial.print("resetting!");
            delay(5000);
            ESP.restart();
       }

        http.end();
      
    }
*/
 
}

void sendToMesh() {
  // we are a remote node, and: it's time to measure, or we're in the first loop

    Serial.println(co2);

    theData.co2 = co2;
    theData.temperature = temperature;
    theData.humidity = humidity;
    theData.light = light;
    memcpy(theData.loranet_pubkey,feed_pubkey,13);
    memcpy(theData.pubkey,feed_pubkey,13);
    memcpy(theData.privkey,feed_privkey,13);
    theData.node_id = this_node_id;
    theData.next_rssi = rf95.lastRssi();
    theData.log_code = logcode;
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
    } else {
      Serial.println(F("OK"));
    }
    
}
void postToBayou(const char * post_pubkey, const char * post_privkey) {

// Handle wifi ...
  Serial.print("Connecting to wifi ...");
      while (wifiMulti.run() != WL_CONNECTED && NAcounts < NAthreshold )
      {
//      u8x8.setFont(u8x8_font_chroma48medium8_r);
//      u8x8.setCursor(10,2);
//      u8x8.print("wifi?");
//      u8x8.setCursor(10,3);
//      u8x8.print(NAcounts);
      Serial.print(".");
      NAcounts ++;
      delay(2000);
      }
      
      if (wifiMulti.run() != WL_CONNECTED) {
//      u8x8.setCursor(8,3);
//      u8x8.print("RESET");
      Serial.println("no wifi ... resetting!");
      delay(500);
      ESP.restart();
      }

      // form the URL:
     
      char post_url [60];
      strcpy(post_url, "http://");
      strcat(post_url,bayou_base_url);
      strcat(post_url,"/");
      strcat(post_url,post_pubkey);
      
      //Form the JSON:
        DynamicJsonDocument doc(1024);
        
        Serial.println(post_url);
        
        doc["private_key"] = post_privkey;
        doc["co2"] =  theData.co2;
        doc["tempC"]=temperature;
        doc["humidity"]=humidity;
        doc["node_id"]=this_node_id;
        doc["next_hop"]=this_node_id;
        doc["rssi"]=0;
        doc["log"]=logcode;
         
        String json;
        serializeJson(doc, json);
        serializeJson(doc, Serial);
        Serial.println("\n");
        
        // Post online

        HTTPClient http;
        int httpCode;
        
        http.begin(post_url);
        http.addHeader("Content-Type", "application/json");
        Serial.print("[HTTP] Connecting ...\n");
      
        httpCode = http.POST(json);        

        if(httpCode== HTTP_CODE_OK) {
          
            Serial.printf("[HTTP code]: %d\n", httpCode);

            //u8x8.setFont(u8x8_font_7x14B_1x2_f);
//            u8x8.setFont(u8x8_font_chroma48medium8_r);
//            u8x8.setCursor(10,2);
//            u8x8.print(httpCode);
            
        } else {
            Serial.println(httpCode);
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            //u8x8.setFont(u8x8_font_7x14B_1x2_f);
//            u8x8.setFont(u8x8_font_chroma48medium8_r);
//              u8x8.setCursor(10,2);
//              u8x8.print(httpCode);
//              //u8x8.print(http.errorToString(httpCode).c_str());
//              u8x8.setCursor(8,3);
//              u8x8.print("RESET");
            Serial.print("resetting!");
            delay(5000);
            ESP.restart();
       }

        http.end();
        
}
