#include "Arduino.h"
#include "SPI.h"
#include "SdFat.h"

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

SdFat SD;

void setup() {
	Serial.begin(115200);
	// Wait for USB Serial
	while (!Serial) {
		SysCall::yield();
	}
	delay(1000);

	Serial.println(F("\nBleSdRemote - START\n"));

	Serial.print(F("SD - Initializing SD card..."));
	pinMode(sdChipSelectPin, OUTPUT);
	if (!SD.begin(sdChipSelectPin, SPI_HALF_SPEED)) {
		//Stop the sketch execution
		SD.initErrorHalt();
	} else {
		Serial.println(F(" Done."));
	}

	/* Initialise the module */
	Serial.print(F("BLE - Initializing the Bluefruit LE module: "));

	if (!ble.begin(VERBOSE_MODE)) {
		Serial.println(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
	}
	Serial.println(F("OK!"));

	if (FACTORYRESET_ENABLE) {
		/* Perform a factory reset to make sure everything is in a known state */
		Serial.println(F("BLE - Performing a factory reset: "));
		if (!ble.factoryReset()) {
			Serial.println(F("Couldn't do factory reset"));
		} else {
			Serial.println(F("OK!"));
		}
	}

	ble.echo(false);
	ble.info();
	ble.verbose(false);

	/* Wait for connection - Adafruit Bluefruit LE app has to be used to establish an UART connection. */
	while (!ble.isConnected()) {
		delay(500);
	}

	// Set module to DATA mode
	ble.setMode(BLUEFRUIT_MODE_DATA);
	Serial.println(F("BLE - Switched to DATA mode!"));
}

void loop() {

	//Read messages from the standard Serial interface
	if (Serial.available() > 0) {
		readMessageFromSerial(Serial.read());
	}

	//Read messages from the BLE interface
	while (ble.available()) {
		readMessageFromSerial((char) ble.read());
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

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractDirectoryPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		if (SD.chdir(directoryPathArray)) {
			responseMessage += listDirectory(SD.vwd(), false);
			SD.chdir(); //Change back to the ROOT directory
		} else {
			Serial.print("Failed to open ");
			Serial.println(directoryPathArray);
			responseMessage += "ERROR";
		}
		Serial.println(responseMessage);
		Serial.flush();
		ble.print(responseMessage);
		ble.flush();
	} else if (message.startsWith("LIST") || message.startsWith("list")) {
		//Change back to the ROOT directory
		if (SD.chdir()) {
			responseMessage += listDirectory(SD.vwd(), true);
		} else {
			responseMessage += "ERROR";
		}
		Serial.println(responseMessage);
		Serial.flush();
		ble.print(responseMessage);
		ble.flush();
	} else if (message.startsWith("INFO:") || message.startsWith("info:")) {

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractDirectoryPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		SdFile currentFile;
		if (currentFile.open(directoryPathArray, O_READ)) {
			Serial.print("@");
			currentFile.printName(&Serial);
			Serial.print("%");
			Serial.print(currentFile.fileSize());
			Serial.print("%");
			currentFile.printCreateDateTime(&Serial);
			Serial.print("%");
			currentFile.printModifyDateTime(&Serial);
			Serial.println("#");
			Serial.flush();

			ble.print("@");
			currentFile.printName(&ble);
			ble.print("%");
			ble.print(currentFile.fileSize());
			ble.print("%");
			currentFile.printCreateDateTime(&ble);
			ble.print("%");
			currentFile.printModifyDateTime(&ble);
			ble.print("#");
			ble.flush();
		}

	} else if (message.startsWith("GETF:") || message.startsWith("getf:")) {

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractDirectoryPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		SdFile currentFile;
		if (currentFile.open(directoryPathArray, O_READ)) {
			dumpFile(&currentFile, &ble, currentFile.fileSize());
			currentFile.close();
		}
	} else if (message.startsWith("DELF:") || message.startsWith("delf:")) {

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractDirectoryPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		if (SD.remove(directoryPathArray)) {
			Serial.print("@OK%");
			Serial.print(directoryPath);
			Serial.print("#");
			Serial.flush();
			ble.print("@OK%");
			ble.print(directoryPath);
			ble.print("#");
			ble.flush();
		} else {
			Serial.print("@KO%");
			Serial.print(directoryPath);
			Serial.print("#");
			Serial.flush();
			ble.print("@KO%");
			ble.print(directoryPath);
			ble.print("#");
			ble.flush();
		}
	}
}

void dumpFile(SdFile* currentFile, Adafruit_BluefruitLE_UART* out, uint32_t totalSize) {

	ble.print("@");
	ble.print(totalSize);
	ble.print("#");

	Serial.print("File size: ");
	Serial.print(totalSize);
	Serial.println(" bytes");

	unsigned long readSize = 0;
	byte buffer[1024];
	int readBytes;
	while (true) {
		readBytes = currentFile->read(buffer, sizeof(buffer));
		readSize += readBytes;
		if (readBytes == -1) {
			Serial.println("File read error.");
			return;
		} else if (readBytes < 1024) {
			out->write(buffer, readBytes);
			out->flush();
			Serial.print(readSize);
			Serial.print("/");
			Serial.println(totalSize);
			Serial.println("GETF completed.");
			return;
		} else {
			out->write(buffer, readBytes);
			out->flush();
			Serial.print(readSize);
			Serial.print("/");
			Serial.println(totalSize);
			delay(500);
		}
	}
}

String extractDirectoryPath(String message) {
	String directoryPath = message.substring(5, message.length());
	Serial.print(F("File path: "));
	Serial.println(directoryPath);
	delay(1000);
	return directoryPath;
}

String listDirectory(FatFile* currentDir, bool isRoot) {
	//Start the reply with the @ sign
	String responseMessage = "@";

	if (!isRoot) {
		//Append the level up command
		responseMessage += "../,";
	}

	char longFileName[256];
	while (true) {
		SdFile currentFile;
		if (!currentFile.openNext(currentDir, O_READ)) {
			//No more files - close the message
			responseMessage += "#";
			return responseMessage;
		}
		currentFile.getName(longFileName, 256);
		responseMessage += longFileName;
		if (currentFile.isDir()) {
			responseMessage += "/";
		}
		responseMessage += ",";
		currentFile.close();
	}
}
