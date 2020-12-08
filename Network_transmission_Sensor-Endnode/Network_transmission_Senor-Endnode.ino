#include <RF24.h>
#include <RF24Network.h>
#include <SPI.h>
#include <dht.h>
#include "LowPower.h"

#define DHT11_PIN 4

dht DHT;
//7,8
RF24 radio(7, 8);
RF24Network network(radio);
const uint16_t master_node = 00;
const uint16_t node_01 = 01;
const uint16_t endpoint = 0111;
const uint16_t node_02 = 02; 
bool successful = 0;
long random_number;

static int network_status = 0;
float return_temperature;
float return_humidity;
unsigned long current_time;
unsigned long start_time;

static char message_from_node_01[100];

const char sleep_message[] = "The end point is now going to sleep for 4 seconds.";
const char success_message[] = "This is a confirmation response from node_01, thank you for the data!";

/*
 * This network_ready() works the exact same way as network_ready() in the master node. Please look
 * at the Network_transmission_master.ino file for a description on how this function works in the 
 * exact same manner
 */
void network_ready()
{
  Serial.println("Initiatializing Network...");
  bool success_01;
  bool success_02;
  
  RF24NetworkHeader start_network_node01(node_01,'X');
  RF24NetworkHeader start_network_node02(node_02,'X');
  do
  {
    Serial.println("Attempting initial sends");
    network.update();
    success_01 = network.write(start_network_node01, &network_status, sizeof(network_status));
    success_02 = network.write(start_network_node02, &network_status, sizeof(network_status));
     
     if(!success_01)
    {
      Serial.println("Unsuccessful 01 send");
    }
    if(!success_02)
    {
      Serial.println("Unsuccessful 02 send");
    }

    if(success_01 && success_02)
    {
      Serial.print("Initial messages to coordinators successfull...");
      break;
    }
    delay(500);
  }while(1);

 RF24NetworkHeader begin_network_node01(node_01,'V');
 RF24NetworkHeader being_network_node02(node_02,'V');

 success_01 = 0;
 success_02 = 0;
 do
 {
    network.update();
    success_01 = network.write(start_network_node01, &network_status, sizeof(network_status));
    success_02 = network.write(start_network_node02, &network_status, sizeof(network_status));
  
    RF24NetworkHeader node_01;
    RF24NetworkHeader node_02;
    
    network.peek(node_01);
    if(node_01.type == 'F'){success_01 = 1;}
    network.peek(node_02);
    if(node_02.type == 'F'){success_02 = 1;} 
  
    if(success_01 && success_02)
    {
      Serial.println("Coordinators are ready! now starting system");
      break;
    }
 }while(1);

  
}

/*
 *  When sleep_mode is called, relay messages are sent to both coordinators as 
 *  a warning message. Once those messages have been sent and the coordinators 
 *  do their thing. the end node goes into low power mode for 4 seconds
 */
void sleep_mode()
{
   Serial.println("Now Entering sleep mode!");
   Serial.println("Sending relay messages to both master and end node....");
   RF24NetworkHeader node_01send(node_01, 'S');
   RF24NetworkHeader node_02send(node_02, 'S');
   network.write(node_01send, &sleep_message, sizeof(sleep_message));
   network.write(node_02send, &sleep_message, sizeof(sleep_message));
   Serial.println("Good night! See you in 4 seconds");
   
   LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
                  
   Serial.println("I'm awake! back to work!");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing Node 011");
  start_time = millis();
  SPI.begin();
  radio.begin();
  network.begin(90, endpoint); //Channel and end_point node
  network_ready();
}

/*
 * The whole system from the end node step - by - step:
 * - Initiate the network and await for any messages sent from the coordinators from the
 *   master node
 * - While waiting, keep track of the current time and compare it to the previous tracked 
 *   time, if the difference of the two equals to a value above 8000 (8 seconds), initiate
 *   sleep mode to conserve some battery
 * - Additionally while waiting for any new packets, probe the DHT11 for proper temperature
 *   and humidity values 
 * - When a packet finally arrives that's from the master asking for sensor values, send the
 *   data of those sensors back into 2 header. (However I found out you can actually send
 *   a chunk of data via a payload, didn't have time to implement that since I found out 
 *   about it last minute before the assignment deadline).
 */
void loop(){


  /* If the time between now and most recent recorded time is past 8 seconds,
   * send the end point node into 
   */
  current_time = millis();
  if(current_time - start_time > 8000)
  {
    sleep_mode();
    start_time = current_time;
    return; 
  }
  
  network.update();
  int temp_value = DHT.read11(DHT11_PIN);

  /*
   * Note: the DHT11 modules scan in intervals and is not always scanning
   * for temperature and humidity values. If the temperature and humidity 
   * values are pulled while it is "scanning", it will return the value 
   * -999.0 for both. To prevent this, a delay is set for for 2 seconds 
   * so that the sensor has a window to produce and return proper values
   */

  while(1)
  {
     Serial.println("grabbing temp and hum values");
     return_temperature = DHT.temperature;
     return_humidity = DHT.humidity;
     Serial.println(DHT.temperature);
     if(return_temperature > 0 && return_humidity > 0)
     {
      if(return_temperature < 100 && return_humidity)
      {
        break;
      }
     }
     delay(2000);
  }


  while(network.available()){
    RF24NetworkHeader header_receive;
    network.read(header_receive, &message_from_node_01, sizeof(message_from_node_01));
    Serial.println(message_from_node_01);
    
    Serial.println("Messaged Receieved! Now sending back temperature and humidity values");

    
    Serial.print("Humidity, and temp to be sent back: ");
    Serial.print(return_temperature);
    Serial.print(" and ");
    Serial.println(return_humidity);
    
    //Create the header frames and send them back to node 01
    RF24NetworkHeader header_send_temperature(master_node, 'V');
    RF24NetworkHeader header_send_humidity(master_node, 'B');
    network.write(header_send_temperature, &return_temperature, sizeof(random_number));
    network.write(header_send_humidity, &return_humidity, sizeof(return_humidity)); 
  }

  //Always reset the value of both variables at the end of the loop so no remaining values from previous loops are sent
  return_temperature = 0;
  return_humidity = 0;

  delay(2000);
}
