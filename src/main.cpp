/*
This code is designed to work on Ebyte's E77 Development Board.
LoRa APRS packets are received, parsed, and then re-formed into 
a packet and then digipeated. Digipeater location transmitting 
has not been added yet. Only WIDE1-1 packets are digipeated, 
all others are ignored. 

*/

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <SubGhz.h>
#include <Wire.h> //Needed for I2C to GPS
#include "STM32RTC.h"
#include "STM32LowPower.h"

float loraFrequency = 433.775;  // tx and rx frequency for LoRa APRS for most countries
float loraBandWith = 125.0f;
uint8_t spreadingFactor = 12;
uint8_t codingRate = 5;
String myCall = "NOCALL-9"; // change this to your call sign
String str;
String LoRa_incoming_Data;
String LoRa_outgoing_Data;

// no need to configure pins, signals are routed to the radio internally
STM32WLx radio = new STM32WLx_Module();

APRSClient aprs(&radio);

// set RF switch configuration for EBytes E77 dev board
// PB3 is an LED - activates while transmitting
// NOTE: other boards may be different!
//       Some boards may not have either LP or HP.
//       For those, do not set the LP/HP entry in the table.
static const uint32_t rfswitch_pins[] = {PA6,  PA7,  PB3, RADIOLIB_NC, RADIOLIB_NC};
static const Module::RfSwitchMode_t rfswitch_table[] = {
{STM32WLx::MODE_IDLE,  {LOW,  LOW,  LOW}},
{STM32WLx::MODE_RX,    {LOW, HIGH, LOW}},
{STM32WLx::MODE_TX_LP, {HIGH, LOW, HIGH}},
{STM32WLx::MODE_TX_HP, {HIGH, LOW, HIGH}},
END_OF_MODE_TABLE,
};

// save transmission state between loops
int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;

// flag to indicate that a packet was received
//volatile bool receivedFlag = false;

// flag to indicate that a packet was sent or received
volatile bool operationDone = false;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) {
  // we got a packet, set the flag
  operationDone = true;
}

String doPacketDecode() {
  // decode packet and create packet for digipeating
  Serial.println(F("[STM32WL] Received packet!"));

  // print data of the packet
  Serial.print(F("[STM32WL] Data:\t\t"));
  Serial.println(str);

  //state = radio.setTCXO(1.7);

  String LoRaHeader;
  String sourceCall;
  String destCall;
  String message;
  String digiPath;
  String digis[8];

  LoRa_incoming_Data = str;

  if (LoRa_incoming_Data.length() < 5)
    goto skip; // bad packet start over
  int pos1, pos2;
  LoRaHeader = LoRa_incoming_Data.substring(0, 3);
  //Serial.println(LoRaHeader);
  pos1 = LoRa_incoming_Data.indexOf('>');
  if (pos1 < 5)
    goto skip; // bad packet start over
  sourceCall = LoRa_incoming_Data.substring(3, pos1);
  Serial.print("sourceCall= ");
  Serial.println(sourceCall);
  pos2 = LoRa_incoming_Data.indexOf(':');
  if (pos2 < pos1)
    goto skip; // bad packet start over
  destCall = LoRa_incoming_Data.substring(pos1 + 1, pos2);
  Serial.print("destCall as received= ");
  Serial.println(destCall);
  Serial.println(destCall.substring(pos2 + 1));
  Serial.println(destCall.substring(0, pos2));

  message = LoRa_incoming_Data.substring(pos2 + 1);
  digiPath = "";
  pos2 = destCall.indexOf(',');
  if (pos2 > 0) {
    digiPath = destCall.substring(pos2 + 1);
    destCall = destCall.substring(0, pos2);
  }
  if (destCall == "")
    goto skip; // bad packet start over
  Serial.print("digiPath= ");
  Serial.println(digiPath);
  Serial.print("destCall= ");
  Serial.println(destCall);

  if (digiPath != ("WIDE1-1")){
    Serial.println("do not repeat"); 
    goto skip; //do not digipeat if not WIDE1-1
  }
  //Change digiPath prior to digipeating
  digiPath = myCall + ",WIDE1*";
  LoRa_outgoing_Data = LoRaHeader + sourceCall + ">" + destCall + "," + digiPath + ":" + message;
  Serial.print("outgoing= ");
  Serial.println(LoRa_outgoing_Data);
  return LoRa_outgoing_Data;
  skip: {
    LoRa_outgoing_Data = "";
    return LoRa_outgoing_Data;
  } // when data doesn't meet expectations - skip decoding packet
}

void doTransmitPacket(String LoRa_outgoing_Data) {
  // start transmitting the first packet
  Serial.print(F("[STM32WL] Sending first packet ... "));
  if (LoRa_outgoing_Data != "") {
    // check to make sure there is a string in LoRa_outgoing_Data
    // you can transmit C-string or Arduino string up to
    // 256 characters long
    transmissionState = radio.startTransmit(LoRa_outgoing_Data);
    transmitFlag = true;
  
    if (transmissionState == RADIOLIB_ERR_NONE) {
      // packet was successfully sent
      Serial.println(F("transmission finished!"));
  
    } else {
      Serial.print(F("failed, code "));
      Serial.println(transmissionState);
    }
  }
}


void setup() {
  Serial.begin(115200);

  // set RF switch control configuration
  // this has to be done prior to calling begin()
  radio.setRfSwitchTable(rfswitch_pins, rfswitch_table);

  // initialize STM32WL with default settings, except frequency
  Serial.print(F("[STM32WL] Initializing ... "));
  //int state = radio.begin(868.0);
  int state = radio.begin(loraFrequency, loraBandWith, spreadingFactor, codingRate);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // set appropriate TCXO voltage for STM32WLE
  state = radio.setTCXO(1.7);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // set the function that will be called
  // when new packet is received
  radio.setDio1Action(setFlag);

  // start listening for LoRa packets
  Serial.print(F("[STM32WL] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // set output power to 22 dBm (accepted range is -17 - 22 dBm)
  if (radio.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
    Serial.println(F("Selected output power is invalid for this module!"));
    while (true) { delay(10); }
  }

  // set over current protection limit to 140 mA (accepted range is 45 - 240 mA) 
  if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
    Serial.println(F("Selected current limit is invalid for this module!"));
    while (true);
  }  
}


void loop() {
  // check if the flag is set
  if(operationDone) {
    // reset flag
    operationDone = false;
    if (transmitFlag) {
      // the previous operation was a transmission, listen for response
      // print the result
      if (transmissionState == RADIOLIB_ERR_NONE) {
        // packet was successfully sent
        Serial.println(F("transmission finished!"));
      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);
      } 
      // listen for response
      radio.startReceive();
      transmitFlag = false;

    } else {  // the previous operation was a reception
      // do rest of the receive function 
      // print the data and send another packet
      // you can read received data as an Arduino String
      int state = radio.readData(str);

      if (state == RADIOLIB_ERR_NONE) {
        // packet was successfully received
        doPacketDecode();
      } // finishes receive function
    // digipeat valid heard station
    doTransmitPacket(LoRa_outgoing_Data);
    }
  }
}