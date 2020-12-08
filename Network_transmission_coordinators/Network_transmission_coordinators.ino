#include <RF24.h>
#include <RF24Network.h>
#include <SPI.h>
#include <LowPower.h>
/*
 * #NOTE:THIS CODE IS USED FOR BOTH ARDUINO NANO COORDINATORS
 * 
 * This is the code for the Arduino nanos, also known as the coordinators for the system.
 * For battery optimization and simulations, one coordinator will always be on and one 
 * coordinator will always be on. 
 * 
 * Battery saving and optimizations:
 * In order to maximize the battery potential of the entire system. One coordinator will 
 * be on and the other coordinator will be off one at a time. After 9 seconds of one coordinator
 * being awake, the code will call the sleep_mode() function and will send out relay messages
 * to both the master and end point node that they will be going into sleep mode. Finally the 
 * coordinator will go into sleep mode turning off things like ADC port, timers, SPI port and
 * will stay in slumber for 8 seconds. The reason we initiate a sleep mode sometime after 9 seconds
 * is to give the other coordinator that's waking back some leverage in order to prevent the master
 * and end point nodes from falling into a loop of looking for a node during that split second if
 * both coordinators were to both be asleep at an inconvenient time.
 */

RF24 radio(10, 9);
RF24Network network(radio);
const uint16_t master_node = 00;
const uint16_t node_01 = 01;
const uint16_t node_02 = 02;
const uint16_t endpoint = 0111;
bool successful = 0;
long random_number;
static char master_message[100];
static float temperature_data = 0;
static float humiditiy_data = 0;
static int last_message = 0;
static int start = 1;
bool coordinator_in_use = 0; //<------ This variable is the key to starting a node either in sleep mode
                                      //or active mode, 1 for active move, 0 for currently in sleep mode

//This will be used for measuring the time
unsigned long current_time;
unsigned long start_time;

const char success_message[] = "This is a confirmation response from node_01, thank you for the data!";
const char sleep_message[] = "I'm going to sleep, please contact the other node.";
const char place_holder = "Placeholder message.";

/*
 * network_ready_coordinator() works similarly to network_ready() but is implemented 
 * slightly different since the coordinator deals with 2 different transmissions. The
 * network_ready_coordinator() protocol has 2 phases it goes through before initiating
 * the actual loop. The coordinator activately listens for the initial messages from  
 * both the master and end points. Once both initial messages have been made and acknoledgement
 * messages have been sent back to both master and end point, phase 1 is completed. Phase 2
 * is essentially waiting for the "ready" messages from both master and end point as those
 * messages are the indicators that they're ready to start the system. Once the both "ready"
 * messages have been received. start the system.
 */
void network_ready_coordinator()
{
  Serial.println("Establishing connection with nodes..."); 
  bool master_node_initial;
  bool endpoint_node_initial;

  bool master_ready;
  bool endpoint_ready;
  while(1)
  {
    network.update();
    if(network.available())
      {
      RF24NetworkHeader header;
      network.peek(header);
  
      /*
       * Z - master node initial message
       * X - End node initial message
       * C - master node is ready
       * V - End node is ready
       */
        switch(header.type)
        {
          case 'Z':
            Serial.println("startup message from master received");
            master_node_initial = 1;
            break;
          case 'X':
            Serial.println("Startup message from end point received");
            endpoint_node_initial = 1;
            break;
          case 'C':
            Serial.println("Master node is ready!");
            RF24NetworkHeader master_ready(master_node, 'F');
            network.write(master_ready, &place_holder, sizeof(place_holder));
            master_ready = 1;
            break;
          case 'V':
            Serial.println("End point is ready!");
            RF24NetworkHeader endpoint_ready(endpoint, 'F');
            network.write(endpoint_ready, &place_holder, sizeof(place_holder));
            endpoint_ready = 1;
            break;
          default:
            Serial.println("!Error in reading header type");
            break;
          
        }

        if(master_ready && endpoint_ready)
        {
          Serial.println("Both endpoints are ready, now starting system!");
          break;
        }
      }
  }
}

void setup() {

  Serial.begin(9600);
  Serial.println("Initializing Node 01");

  SPI.begin();
  radio.begin();
  network.begin(90, node_01);
  network_ready_coordinator(); 
  
}

/*
 * send_tranmission() is more or less a utility function for handling 
 * transmission to other nodes. Mostly to prevent spaghetti and repetative
 * code.
 */
void send_transmission(const uint16_t receiving_node,
                       long data = 0,
                       const char header_type = 'T', 
                       const char message[] = "This is a transmission from node 01")
{
  RF24NetworkHeader header(receiving_node, header_type);
  switch(header_type)
  {
    case 'T':
      Serial.println("Sending message to endpoint");
      if(!network.write(header, &message, sizeof(message))){
        Serial.println("Send unsuccessful");
      }
      break;
    case 'N':
      Serial.println("Sending temperature value back to the master");
      network.write(header, &data, sizeof(data));
      break;
    case 'M':
      Serial.println("Sending humidity value back to the master");
      network.write(header, &data, sizeof(data));
      break;
    default:
      Serial.println("ERROR! something occured and no transmission will be sent");
      break; 
  }
}

void sleep_mode()
{
   Serial.println("Now Entering sleep mode!");
   Serial.println("Sending relay messages to both master and end node....");
   RF24NetworkHeader master(master_node, 'S');
   RF24NetworkHeader endpoint_send(endpoint, 'S');
   network.write(master, &sleep_message, sizeof(sleep_message));
   network.write(endpoint_send, &sleep_message, sizeof(sleep_message));
   Serial.println("Good night! See you in 8 seconds");
   
   LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF,
                  TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
                  
   Serial.println("I'm awake! back to work!");
}

/*
 * Coordinator loop step - by - step:
 * 1 - Update the network
 * 2 - record the current time and compare it to the previous record time, if 
 *     difference between the 2 is greater than or equal to 9000 milliseconds 
 *     (9 seconds). initiate sleep mode, send messages to other nodes to connect
 *     to the other coordinator and then record the current time as the new start
 *     time.
 * 3 - If there is a message, peek at it and check the type tag. The type tag tells
 *     the coordinator who the message is for.
 * 4 - Once the type tag is checked, select the proper switch case and proceed to send
 *     the message out to whichever node
 */
void loop(){
  network.update();

  
  if(current_time - start_time >= 9000 && coordinator_in_use)
  {
    sleep_mode();
    start_time = current_time;
    coordinator_in_use = 0;
    return;
  }  
  else
  {
    current_time = millis();
  }
  while(network.available())
  {
    Serial.println("item received, now handling");
    RF24NetworkHeader header;
    network.peek(header);
    
    // T - A message from the master, pull it and then send a message to node 02
    // V - A message from node 02, this header contains the temperature value
    // B - A message from node 02, this header contains the humidity value
    switch(header.type){
      case 'T':
        network.read(header, &master_message, sizeof(master_message));
        Serial.println(master_message);
        send_transmission(node_02, 0, 'T'); 
        break;
      case 'V':
        network.read(header, &temperature_data, sizeof(temperature_data));
        Serial.print("The temperature value sent from node 011 is: ");
        Serial.println(temperature_data);
        send_transmission(master_node, temperature_data, 'N');
        break;
      case 'B':
        network.read(header, &humiditiy_data, sizeof(humiditiy_data));
        Serial.print("The humiditiy value sent from node 011 is: ");
        Serial.println(humiditiy_data);
        send_transmission(master_node, humiditiy_data, 'M');
      default:
        Serial.println("ERROR! Something has gone wrong in the main loop switch statement!");
      }   
    }

  delay(1000);
  
}
