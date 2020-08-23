
// Mesh has much greater memory requirements, and you may need to limit the
// max message length to prevent wierd crashes
#define RH_MESH_MAX_MESSAGE_LEN 50
 
//#include <RHMesh.h>
//#include <RH_RF22.h>
//#include <SPI.h>

#include <RHMesh.h>
#include <RH_RF95.h>
 
// In this small artifical network of 4 nodes,
#define CLIENT_ADDRESS 1
#define SERVER1_ADDRESS 2
#define SERVER2_ADDRESS 3
#define SERVER3_ADDRESS 4

// for itsy bitsy m0  
#define RFM95_CS 7
#define RFM95_RST 9
#define RFM95_INT 10

RH_RF95 driver(RFM95_CS, RFM95_INT);
 
// Class to manage message delivery and receipt, using the driver declared above
RHMesh manager(driver, SERVER1_ADDRESS);
 
void setup() 
{
  Serial.begin(9600);
  if (!manager.init()) {
    Serial.println("RFM95 init failed");
  }
   driver.setTxPower(23, false);
  driver.setFrequency(915.0);
  driver.setCADTimeout(500);
}
 
uint8_t data[] = "And hello back to you from server1";
// Dont put this on the stack:
uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
void loop()
{
  //Serial.println("listening ...");
  
  uint8_t len = sizeof(buf);
  uint8_t from;
  if (manager.recvfromAck(buf, &len, &from))
  {
    Serial.print("got request from : 0x");
    Serial.print(from, HEX);
    Serial.print(": ");
    Serial.println((char*)buf);
 
    // Send a reply back to the originator client
    if (manager.sendtoWait(data, sizeof(data), from) != RH_ROUTER_ERROR_NONE)
      Serial.println("sendtoWait failed");
  }
}
