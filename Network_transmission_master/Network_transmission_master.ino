#include <RF24.h>
#include <RF24Network.h>
#include <SPI.h>


RF24 radio(7, 8);
RF24Network network(radio);
const uint16_t master_node = 00;
const uint16_t node_01 = 01;
const uint16_t node_02 = 02;
const uint16_t endpoint = 0111;
bool successful = 0;
long random_number;
bool ready_for_next_run = 1;
static float node_02_data;

static int network_status = 0;
static bool main_ready = 1;
int current_node = 1;

static char coordinator_sleep_message[100];
const char success_message[] = "Message Successfully received";

/*
 * network_ready() is setup function used to ensure that that every
 * node network is ready in a synchronous manner. If we were to say, power on
 * a coordinator node a second or two too late, there might be some timing issues
 * when it comes to the coordinators and their sleep modes which would cause even more
 * network issues with the master and end_point trying to find a working coordinator
 * 
 * Network_ready() step - by - step (the same applies to the end point node as well)"
 * 
 * 1 - send an initial message to both coordinators, do go to the next step until both
 *     success conditions have been met
 * 2 - Send a "ready" header tranmission (label with type 'C') to both coordinators to
 *     notify that master is ready for the network to start
 * 3 - initiate loop and network
 * 
 */
void network_ready()
{
  Serial.println("Initiatializing Network...");
  bool success_01;
  bool success_02;
  
  RF24NetworkHeader start_network_node01(node_01,'Z');
  RF24NetworkHeader start_network_node02(node_02,'Z');

  //Send initiating messages to both coordinators until both are successful
  do
  {
    network.update();
    success_01 = network.write(start_network_node01, &network_status, sizeof(network_status));
    success_02 = network.write(start_network_node02, &network_status, sizeof(network_status));
    
    if(success_01 && success_02)
    {
      Serial.print("Initial messages to coordinators successful...");
      break;
    }
    else
    {
      Serial.println("Sends not completed, retrying");
    }
    delay(500);
  }while(1);

 RF24NetworkHeader begin_network_node01(node_01,'C');
 RF24NetworkHeader being_network_node02(node_02,'C');

 success_01 = 0;
 success_02 = 0;

 //Let both coordinators know master is ready, start the system when both acknowledge
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

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing Master/Parent Node 00");

  SPI.begin();
  radio.begin();
  network.begin(90, master_node); //Channel and master node address
//  network_ready();
}
/*
 * The whole system from the masters point of view to end-point goes as follows per loop:
 * 
 * 1 - Keep updating for anything on the network.
 * 2 - Generate a header for the frame and send an initial message out to which ever coordinator is currently available.
 * 3 - That header is labeled as a 'T' type and will be interpereted as an initial message sent out by the master from 
 *   the coordinator nodes.
 * 4 - If the message is successful, print out to the serial monitor that the transmission was a success.
 * 5 - At the same time, if we have any tranmissions waiting for us in our network buffer. Create a header to receive
 *   that data and print any of the information that came with it to the command line (the information that will received
 *   by either of the nodes will always be the temperature and humidity values from the DHT11 end point).
 * 6 - Afterwards, delay the system 2 seconds before we run the loop again to provide leverage for the other nodes on the 
 *   network to accomplish their task.
 *   
 * Battery Optimization:
 *  - The master has no form of battery optimization as it is the head and constantly needs to be on to send messages
 *    and receive any data or messages from whichever coordinator the master is currently using.
 *  - If the master receives a sleep initiated message from its' current coordinator, the master will then begin to send
 *    a message to the other coordinator to check that it's up and running and will continue to send and recieve any messages
 *    and/or data from that receiver
 *  - Any packets dropped during coordinator switching will be relayed to both master and end point in which those packets will 
 *    resent from both sides.
 */

static float temperature = 0.0;
static float humidity = 0.0;

void loop(){
  network.update();
//  if(ready_for_next_run)
//  {
//    Serial.println("Attempting to send out initial message");
//    const char initial[] = "Initial message from the master Node";
//    
//    RF24NetworkHeader header(node_01, 'T');
//    successful = network.write(header, &initial, sizeof(initial));
//
//    if(successful)
//    {
//      ready_for_next_run = 0;
//      Serial.println("Message successfully sent");
//    }
//    else
//    {
//      Serial.println("Message sent unsuccessfully, attempting a resend");
//    }
//  }
//  else
//  {
//    Serial.println("Currently Waiting for the data from node 02");
//  }
    const char initial[] = "Initial message from the master Node";
    
    RF24NetworkHeader header(node_02, 'T');
    successful = network.write(header, &initial, sizeof(initial));

    if(successful)
    {
      ready_for_next_run = 0;
      Serial.println("Message successfully sent");
    }
    else
    {
      Serial.println("Message sent unsuccessfully, attempting a resend");
    }
  

  while(network.available()){
    RF24NetworkHeader header_receive;
    network.peek(header_receive);

    /*
     * S - A message from the currently being used that they are going to sleep, switch to the other node
     * V - a message from the DH11 with the temperature value, print to the serial monitor
     * B - a message from the DHT11 with the humidity value, print to the serial monitor
     */
    switch(header_receive.type)
    {
      case 'S':
        static char sleep_message[100];
        network.read(header_receive, &sleep_message, sizeof(sleep_message));
        Serial.println(sleep_message);
        Serial.println("Delaying master from sending any messages for 4 seconds...");
        delay(4000);
        Serial.println("Now restarting transmissions...");
        return;
      case 'V':
        network.read(header_receive, &temperature, sizeof(temperature));
        Serial.print("Temperature value from the end point: ");
        Serial.println(temperature);
        break;
      case 'B':
        network.read(header_receive, &humidity, sizeof(humidity));
        Serial.print("Humidity value from the end point: ");
        Serial.println(humidity);
        break;
      default:
        Serial.println("header with no specified type");
        break;
    }
  }

  //Always reset the temperature and humidity variables back to 0 to prevent possible data roll-over.
  temperature = 0.0;
  humidity = 0.0;
  delay(2000);
}
