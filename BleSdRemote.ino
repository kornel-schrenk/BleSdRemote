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

SdFat SD;

//Message handling related variables 
String messageBuffer = "";
bool recordMessage = false;

bool storeInFileMode = false;
unsigned long storeFileSize = 0;
unsigned long receivedFileSize = 0;
SdFile storeFile;

void setup() {
	Serial.begin(115200);

	while (!Serial) { // Wait for USB Serial
		SysCall::yield();
	}
	delay(1000);

	Serial.println(F("\nBlueSdRemote - START\n"));

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
	if (!storeInFileMode) {
		if (data == '@') {
			messageBuffer = "";
			recordMessage = true;
			digitalWrite(ledPin, HIGH); //Turn the LED on
		} else if (data == '#') {
			handleMessage(messageBuffer);
			recordMessage = false;
			digitalWrite(ledPin, LOW); //Turn the LED off
		} else {
			if (recordMessage) {
				messageBuffer += data;
			}
		}
	} else {
		if (receivedFileSize < storeFileSize && storeFile.isOpen()) {
			storeFile.write(data);
			receivedFileSize++;
		} else if (receivedFileSize == storeFileSize) {
			storeFile.flush();
			storeFile.close();

			Serial.print(receivedFileSize);
			Serial.print("/");
			Serial.println(storeFileSize);
			Serial.flush();

			Serial.println(F("Switched back into normal message processing mode!"));

			messageBuffer = "";
			storeInFileMode = false;
		}

		if (receivedFileSize % 512 == 0) {
			storeFile.flush();

			Serial.print(receivedFileSize);
			Serial.print("/");
			Serial.println(storeFileSize);
			Serial.flush();
		}
	}
}

void handleMessage(String message) {
	Serial.println(message);
	Serial.flush();

	String responseMessage = "";
	if (message.startsWith(F("LIST:")) || message.startsWith(F("list:"))) {

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractDirectoryPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		if (SD.chdir(directoryPathArray)) {
			responseMessage += listDirectory(SD.vwd(), false);
			SD.chdir(); //Change back to the ROOT directory
		} else {
			Serial.print(F("Failed to open "));
			Serial.println(directoryPathArray);
			responseMessage += F("ERROR");
		}
		Serial.println(responseMessage);
		Serial.flush();
		ble.print(responseMessage);
		ble.flush();
	} else if (message.startsWith(F("LIST")) || message.startsWith(F("list"))) {
		//Change back to the ROOT directory
		if (SD.chdir()) {
			responseMessage += listDirectory(SD.vwd(), true);
		} else {
			responseMessage += F("ERROR");
		}
		Serial.println(responseMessage);
		Serial.flush();
		ble.print(responseMessage);
		ble.flush();
	} else if (message.startsWith(F("INFO:")) || message.startsWith(F("info:"))) {

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

	} else if (message.startsWith(F("GETF:")) || message.startsWith(F("getf:"))) {

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractDirectoryPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		SdFile currentFile;
		if (currentFile.open(directoryPathArray, O_READ)) {
			dumpFile(&currentFile, &ble, currentFile.fileSize());
			currentFile.close();
		}
	} else if (message.startsWith(F("DELF:")) || message.startsWith(F("delf:"))) {

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractDirectoryPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		if (SD.remove(directoryPathArray)) {
			Serial.print(F("@OK%"));
			Serial.print(directoryPath);
			Serial.print("#");
			Serial.flush();
			ble.print(F("@OK%"));
			ble.print(directoryPath);
			ble.print("#");
			ble.flush();
		} else {
			Serial.print(F("@KO%"));
			Serial.print(directoryPath);
			Serial.print("#");
			Serial.flush();
			ble.print(F("@KO%"));
			ble.print(directoryPath);
			ble.print("#");
			ble.flush();
		}
	} else if (message.startsWith(F("PUTF:")) || message.startsWith(F("putf:"))) {

		//The String has to be converted into a char array, otherwise the board will reset itself
		String directoryPath = extractUploadPath(message);
		char directoryPathArray[directoryPath.length()+1];
		directoryPath.toCharArray(directoryPathArray, directoryPath.length()+1);

		if (SD.exists(directoryPathArray)) {
			SD.remove(directoryPathArray);
			Serial.println(F("Existing file was removed."));
		}

		if (storeFile.open(directoryPathArray, O_WRITE | O_CREAT | O_APPEND)) {
			storeFileSize = extractFileSize(message);
			receivedFileSize = 0;

			Serial.println(F("@OK#"));
			Serial.flush();
			ble.print(F("@OK#"));
			ble.flush();

			storeInFileMode = true;
			Serial.println(F("Switched into file STORAGE mode!"));
		} else {
			Serial.println(F("@KO#"));
			Serial.flush();
			ble.print(F("@KO#"));
			ble.flush();

			storeInFileMode = false;
		}
	}
}

void dumpFile(SdFile* currentFile, Adafruit_BluefruitLE_UART* out, uint32_t totalSize) {

	ble.print("@");
	ble.print(totalSize);
	ble.print("#");

	Serial.print(F("File size: "));
	Serial.print(totalSize);
	Serial.println(F(" bytes"));

	unsigned long readSize = 0;
	byte buffer[1024];
	int readBytes;
	while (true) {
		readBytes = currentFile->read(buffer, sizeof(buffer));
		readSize += readBytes;
		if (readBytes == -1) {
			Serial.println(F("File read error."));
			return;
		} else if (readBytes < 1024) {
			out->write(buffer, readBytes);
			out->flush();
			Serial.print(readSize);
			Serial.print("/");
			Serial.println(totalSize);
			Serial.println(F("GETF completed."));
			return;
		} else {
			out->write(buffer, readBytes);
			out->flush();
			Serial.print(readSize);
			Serial.print("/");
			Serial.println(totalSize);
			delay(500); //Delay was added to switch the line back into normal state
		}
	}
}

String extractDirectoryPath(String message) {
	String directoryPath = message.substring(5, message.length());
	Serial.print(F("File path: "));
	Serial.println(directoryPath);
	return directoryPath;
}

String extractUploadPath(String message) {
	String directoryPath = message.substring(5, message.indexOf('%'));
	Serial.print(F("Upload path: "));
	Serial.println(directoryPath);
	return directoryPath;
}

unsigned long extractFileSize(String message) {
	String fileSize = message.substring(message.indexOf('%')+1);
	Serial.print(F("Upload file size: "));
	Serial.println(fileSize);
	return fileSize.toInt();
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
