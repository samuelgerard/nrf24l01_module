#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

/* A Quick Overview:
 * 
 * The RF24 module is the most basic library that supports the nrf24 wireless chip.
 * There are, however, other available officially support (not "official", just open-source
 * libraries that are widely support amongst the arduino community for the nrf24); these 
 * include: RF24Network, RF24Mesh, RF24Ethernet, and couple others including some python 
 * wrappers. Each of these librarys supports a different layer of the OSI (E.G RF24Ethernet
 * supports upwards layers 4 & 5 of the OSI model) and suited around whatever networking
 * needs are demanded from the individuals project. 
 * 
 * The Project so far:
 * 
 * I was able to set up a master and slave/receiver setup in which the master arduino creates
 * a pipline to the receiver and the receiver is constantly listening for data. When the button 
 * is pushed down, the master will send a transmission to the receiver that the status of the button
 * is HIGH and will light up the LED as a response.
 * 
 * Consult "FINAL NOTES" at the bottom for current conclusions and my next steps working with this 
 * project
 * /  

/*
 * CE (Chip Enable) and CSN (Chip Select Not) are the pins directly on the nrf24 that
 * deal with what state it will be set to (receiver, transmitter, full-duplex). In this case,
 * CE deals with what the role of the chip is (E.G: receiver) and CSN deals with active listening
 * of the SPI (Serial Peripheral Ports) of the arduino.
 * 
 * the CE and CSN pins will be connected to pins 7 and 8 on my arduino
 */
RF24 radio(7, 8); // CE, CSN

/*
 * On my arduino UNO, the pin that will be handling input from the button
 * will be designated towards pin 2. I also have a button_state pin that 
 * will keep track of the state of the button; when the button isn not pressed,
 * the state will always be percieved as LOW. When the button is pressed,
 * an electrical current is connected and sent to pin 2 on the arduino indicating 
 * that the input is HIGH and will send a transmission to the receiver module.
 */
int button_pin = 2;
boolean button_state = 0;
boolean transmission_successful = 0;

/*
 * A quick explanation of what is going on in the setup. The arduino IDE serial CLI 
 * is intiated along with pin 2 being set to an INPUT state to take in the state of the button. 
 * In order to initialize the NRF, the function radio.begin() must be called (don't know exactly what is going on at 
 * a low-level, need to read more about it, will update). Afterwards, a connection
 * is made.
 * 
 * NOTE: In order for nrf24 modules to connect and create a pipe to each other they
 * must both have the same byte address, in this case, look at the 'const byte address'
 * I have set below and am calling with in the fuction.
 * 
 * Here, we set the PA level to a macro that is the minimum PA level; this means that
 * less wattage/power will be used but the range at which the transmitter can send out
 * will also be reduced. Finally, in order to change the state of this module as a 
 * "tranmitter", the radio.stopListening() function is called (for "receiver" state, the 
 * equivalent function would be radio.stopListening()).
 */
const byte address[6] = "00001";
void setup() {
  Serial.begin(9600);
  pinMode(button_pin, INPUT);
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN); //<----- lowest Power Amplifier so lowest level of range
  radio.stopListening();
}

/*
 * Here in the loop, we are constantly reading what the state of the button is 
 * and sending out transmissions to the receiver. If the state of the button is 
 * high (meaning it's pressed), we transmit a message of the current button state
 * to the receiving module along with a boolean of the current button state. Otherwise,
 * a transmission is still made but just as an indicator that the button is low and 
 * do nothing on the other end. 
 * 
 * radio.write() is actually a boolean function and will return a 1 or 0 if a transmission
 * was successful or not which helps to identify if we've made a successful transmission.
 * 
 * delay(1000) is there so that the CLI isn't cluttered with output.
 */
void loop() {

  button_state = digitalRead(button_pin);
  if (button_state == HIGH){
     const char text[] = "Current button state is HIGH";
     radio.write(&text, sizeof(text)); //checksum
     Serial.println(text);
  }
  else{
    const char text[] = "Current button state is LOW";
    radio.write(&text, sizeof(text));
    Serial.println(text);
  }
  
  transmission_successful = radio.write(&button_state, sizeof(button_state));
  transmission_successful ? Serial.println("TRANSMISSION SUCCESSFUL") : Serial.println("TRANSMISSION UNSUCCESSFUL");
  delay(1000);
}

/*
 * FINAL NOTES:
 *  - Currently reading more with that exactly is going under the hood here in terms of
 *    networking and the OSI layer to have a better idea on for future task
 *  - I'm planning to experiment in making my nrf24 modules full duplex so that they
 *    can send and receive transmissions from each other. By low-level design, this shouldn't
 *    be possible; however, the nrf24 chips can switch roles via double ackpayload integration
 *    amongst both chips with the RF24 Module. 
 *  - I will test this implementation but I will more than like switch over to the officially supported RF24Network Module 
 *    which acts, from an abstract point of view, an OSI layer 3 driver + OSI layer 2. At a base level, the RF24Network module
 *    is designed around a tree topology.
 */
