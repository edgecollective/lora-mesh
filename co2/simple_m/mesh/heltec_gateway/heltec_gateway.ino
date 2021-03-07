
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

#define display_push 1 //line to skip

SCD30 airSensor;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

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

long lastPingTime = 0;  // the last time the output pin was toggled
long pingDelay = interval_sec*1000;

void setup() {

  u8x8.begin();
  
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
   u8x8.clear();
   u8x8.setCursor(0,0); 
   u8x8.print("Starting...");
   delay(1000);
   
  Serial.begin(115200);
   
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

  if(this_node_id == gatewayNode) // we're the gateway, so try to connect to wifi
  {
    wifiMulti.addAP(SSID,WiFiPassword);
  }

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


void loop() {

// at a regular interval, ping the server to let it know that we're up and running
// if we're the gateway, post it to Bayou; if we're a remote node, send it to the lora network

if (  ( (millis() - lastPingTime) > (pingDelay)) || firstLoop) {

    // send to display
    u8x8.clear();

     //display our pubkey
    //u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.setCursor(0,0);
    //u8x8.print("G/1: ");
    u8x8.print(this_node_pubkey);
    //u8x8.print(" n:1");

    u8x8.setCursor(0,display_push);
    u8x8.print("n  hop rssi post");
    //u8x8.setFont(u8x8_font_chroma48medium8_r);
    //u8x8.setCursor(0,7);
    //u8x8.print(this_node_pubkey);

    //light = 0; // if we add a light sensor, can make this real
    gatewayPingBayou(this_node_pubkey,this_node_privkey,this_node_id); // we're assigning '0' to the next_rssi here as a convention
  
    firstLoop = 0; // we did our thing successfully for the first loop, so we're out of that mode
    lastPingTime = millis(); // reset the interval timer 

} // measureTime


//otherwise in the loop, we should be listening for and processing messages from the lora mesh
// this is blocking code; but basically run whenever we're *not* measuring
// trying to do this by coordinating the 'epsilon_interval' variable, not sure yet if it's working ...

relayFromMesh(pingDelay);

} // loop


void relayFromMesh(int waitTime) {

// run for waitTime millis, listening for and relaying mesh messages
// if the intended recipient is us, we're the gateway, and we should post the incoming packet data to Bayou

    
    uint8_t buf[sizeof(Payload)];
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, waitTime, &from)) {  // this runs until we receive some message
      // entering this block means the message is for us

     // the rest of this code only runs if we were the intended recipient; which means we're the gateway
      theData = *(Payload*)buf;

      
      Serial.print(from);
      Serial.print(F("->"));
      Serial.print(F(" :"));
      Serial.print("node_id = ");
      Serial.print(theData.node_id);
      Serial.print("; next_hop = ");
      Serial.println(theData.next_hop);
      Serial.print("loranet:");
      Serial.println(theData.loranet_pubkey);

      u8x8.setFont(u8x8_font_chroma48medium8_r);
        u8x8.setCursor(0,theData.node_id+display_push);
        u8x8.print(theData.node_id);
        u8x8.print("  ");
        u8x8.print(theData.next_hop);
        u8x8.print("    ");
        u8x8.print(theData.next_rssi);
        
        
      
     if(strcmp(theData.loranet_pubkey, loranet_pubkey) == 0) {
          Serial.println("loranet match!");
          u8x8.setFont(u8x8_font_chroma48medium8_r);
          //u8x8.setCursor(0,theData.node_id+display_push);
         // u8x8.print(theData.node_id);
          //u8x8.print(":");
          //u8x8.print("REC");
          //delay(100);
          // copy the incoming data to global variables for co2, temperature, humidity, etc

          co2 = theData.co2;
          temperature = theData.temperature;
          humidity = theData.humidity;
          light = theData.light;
          
          // post data to the appropriate Bayou feed, with the proper keys
          postToBayou(theData.pubkey,theData.privkey,theData.node_id,theData.next_hop,theData.next_rssi);
    } // end if loranet
    else{
       Serial.println("no loranet match");
       u8x8.print(" ");
       u8x8.print("KEY?");
    }
    
    }

}


void postToBayou(const char * post_pubkey, const char * post_privkey, int node_id, int next_hop, int next_rssi) {

// Handle wifi ...
  Serial.print("Connecting to wifi ...");
      while (wifiMulti.run() != WL_CONNECTED && NAcounts < NAthreshold )
      {
      //u8x8.setFont(u8x8_font_chroma48medium8_r);
      //u8x8.setCursor(10,2);
      //u8x8.print("wifi?");
      //u8x8.setCursor(10,3);
      //u8x8.print(NAcounts);
      Serial.print(".");
      NAcounts ++;
      delay(1000);
      }
      
      if (wifiMulti.run() != WL_CONNECTED) {

      u8x8.print(" ");
      u8x8.print("WIFI");
        
      //ESP.restart();
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
        doc["co2"] =  co2;
        doc["tempC"]=temperature;
        doc["humidity"]=humidity;
        doc["light"]=light;
        doc["node_id"]=node_id;
        doc["next_hop"]=next_hop;
        doc["next_rssi"]=next_rssi;
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

        u8x8.setFont(u8x8_font_chroma48medium8_r);
        u8x8.print(" ");
        u8x8.print(httpCode);

        http.end();
        
}


void gatewayPingBayou(const char * post_pubkey, const char * post_privkey, int node_id) {

// Handle wifi ...
  Serial.print("Connecting to wifi ...");

  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.setCursor(0,node_id+display_push);
  u8x8.print(node_id);
  u8x8.print("           ");
  
  
      while (wifiMulti.run() != WL_CONNECTED && NAcounts < NAthreshold )
      {
       u8x8.setCursor(0,node_id+display_push);
      u8x8.print(node_id);
      u8x8.print("  ");
      u8x8.print("wifi? #");
      //u8x8.setCursor(10,3);
      u8x8.print(NAcounts);
      Serial.print(".");
      NAcounts ++;
      delay(2000);
      }
      
      if (wifiMulti.run() != WL_CONNECTED) {
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.setCursor(0,node_id+display_push);
      u8x8.print(node_id);
      u8x8.print("BAD WIFI: RESET");
      delay(2000);
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
        doc["node_id"]=node_id;
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

            u8x8.setFont(u8x8_font_chroma48medium8_r);
            u8x8.setCursor(0,node_id+display_push);
            u8x8.print(node_id);
            u8x8.print("           ");
            u8x8.print(httpCode);
            
        } else {
            Serial.println(httpCode);
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
             u8x8.setFont(u8x8_font_chroma48medium8_r);
              u8x8.setCursor(0,node_id+display_push);
              u8x8.print(node_id);
              u8x8.print("   ");
              u8x8.print(httpCode);
              u8x8.print(" RESET");
            Serial.println("resetting!");
            delay(5000);
            ESP.restart();
       }

        http.end();
        
}
