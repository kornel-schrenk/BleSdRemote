#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

//Please set it to 1, if factory reset is desired during startup otherwise set it to 0
#define FACTORYRESET_ENABLE 0

#if defined(HAVE_HWSERIAL1) //Use the first physical serial UART on Arduino Mega or Due
	Adafruit_BluefruitLE_UART ble(Serial1, BLUEFRUIT_UART_MODE_PIN);
#else //Use Software Serial interface -- Do not forget to adjust the configuration!!!
	SoftwareSerial bluefruitSS = SoftwareSerial(BLUEFRUIT_SWUART_TXD_PIN, BLUEFRUIT_SWUART_RXD_PIN);
	Adafruit_BluefruitLE_UART ble(bluefruitSS, BLUEFRUIT_UART_MODE_PIN, BLUEFRUIT_UART_CTS_PIN, BLUEFRUIT_UART_RTS_PIN);
#endif

const int ledPin = 13;  // led pin
const int sdChipSelectPin = 53; //For Mega TFT LCD shield use 53, for SD card reader use 10

//Message handling related variables 
String messageBuffer = "";
bool recordMessage = false;

void setup() {
  while (!Serial);  // required for Flora & Micro
  delay(500);
  
  Serial.begin(115200);

  Serial.print(F("Initializing SD card..."));

  if (!SD.begin(sdChipSelectPin)) {
    Serial.println(F(" Failed!"));
    return;
  }
  Serial.println(F(" Done."));

  Serial.println(F("Adafruit Bluefruit Command <-> Data Mode Example"));
  Serial.println(F("------------------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if (!ble.begin(VERBOSE_MODE)) {
    Serial.println(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println(F("OK!"));

  if (FACTORYRESET_ENABLE) {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      Serial.println(F("Couldn't do factory reset"));
    }
  }

  ble.echo(false);
  ble.info();  
  ble.verbose(false); 

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in UART mode"));
  Serial.println(F("Then Enter characters to send to Bluefruit"));
  Serial.println();

  /* Wait for connection */
  while (!ble.isConnected()) {
      delay(500);
  }

  // Set module to DATA mode
  ble.setMode(BLUEFRUIT_MODE_DATA);
  Serial.println(F("**************************"));
  Serial.println(F("* Switched to DATA mode! *"));
  Serial.println(F("**************************")); 
}

void loop() {

  //Read messages from the standard Serial interface
  if (Serial.available() > 0) {
    readMessageFromSerial(Serial.read());
  }

  //Read messages from the BLE interface  
  while (ble.available()) {
    readMessageFromSerial((char)ble.read());
  }  
}

void readMessageFromSerial(char data) {  
  if (data == '@') {
    recordMessage = true;
    digitalWrite(ledPin, HIGH); //Turn the LED on
  } else if (data == '#') {
    handleMessage(messageBuffer);
    recordMessage = false;
    messageBuffer = "";    
    digitalWrite(ledPin, LOW); //Turn the LED off      
  } else {
    if (recordMessage) {
      messageBuffer += data;
    }    
  }  
}

void handleMessage(String message) {
  Serial.println(message);    
  Serial.flush();
  
  String responseMessage = "";      
  if (message.startsWith("LIST:") || message.startsWith("list:")) {
    File directory = SD.open(extractFileName(message), FILE_READ);    
    responseMessage += listDirectory(directory);       
  } else if (message.startsWith("LIST") || message.startsWith("list")) {  
    File root = SD.open("/", FILE_READ);  
    responseMessage += listDirectory(root);    
  } else if (message.startsWith("GET:") || message.startsWith("get:") ) {
    String fileName = extractFileName(message);
    //TODO Send the file content trought the serial channel
  } else {
    responseMessage += "ERROR";
  }  

  Serial.println(responseMessage);    
  Serial.flush(); 
  ble.print(responseMessage);    
  ble.flush(); 
}

String extractFileName(String message)
{
  String fileName = message.substring(5, message.length());
  Serial.print(F("File name: "));
  Serial.println(fileName);
  return fileName;    
}

String listDirectory(File dir) {
  String responseMessage = "";
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      //No more files - return to the first file in the directory      
      entry.close();
      dir.close();
      dir.rewindDirectory();
      return responseMessage;
    }
    responseMessage += entry.name();
    if (entry.isDirectory()) {
      responseMessage += "/";
    } 
    responseMessage += ",";
    entry.close();
  }
}
