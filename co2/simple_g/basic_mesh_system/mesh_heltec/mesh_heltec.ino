
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
#include "config.h"

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

String post_url="http://"+String(bayou_base_url)+"/"+String(feed_pubkey);

String short_feed_pubkey = String(feed_pubkey).substring(0,3);

void setup() {

  Serial.begin(115200);

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

  if(nodeId == targetNode) // we're the gateway, so try to connect to wifi
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

void loop() {


if (  (nodeId != targetNode) && (( (millis() - lastMeasureTime) > measureDelay) || firstLoop)) {
  // we are a remote node, and: it's time to measure, or we're in the first loop

    firstLoop = 0;
    
    int co2 = 400 + 1-2*(random(0,99)/100); 

    Serial.println(co2);

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
      Serial.println(F("OK"));
    }
    
    lastMeasureTime = millis(); //set the current tim
}


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

 
}
