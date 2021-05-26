#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield
const int DELAY_BETWEEN_CARDS = 100;
long timeLastCardRead = 0;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;
// Or use this line for a breakout or shield with an I2C connection:
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);


SoftwareSerial mySoftwareSerial(11, 10); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);
uint16_t detectNFC(void);
int volume = 10;
uint16_t isPlaying = 0;
uint16_t counter = 0;

void setup()
{
  mySoftwareSerial.begin(9600);
  Serial.begin(115200);

  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true);
  }
  Serial.println(F("DFPlayer Mini online."));

  myDFPlayer.volume(10);  //Set volume value. From 0 to 30
  //myDFPlayer.play(1);  //Play the first mp3



  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

  //nfc.setPassiveActivationRetries(0x01);
  nfc.SAMConfig();
  //attachInterrupt(0, detectNFC, RISING);
  startListeningToNFC();

  //attachInterrupt(digitalPinToInterrupt(PN532_IRQ), handleCardDetected, FALLING);

  
  //Serial.println("Waiting for an ISO14443A Card ...");
}

void loop()
{
  static unsigned long timer = millis();

  if (millis() - timer > 100) {
    timer = millis();

    if(counter++ > 1) {
      counter = 0;
      isPlaying = 0;
      myDFPlayer.pause();
    }
    //startListeningToNFC();
    //myDFPlayer.next();  //Play next mp3 every x second.
  }

  int x = 0;
  String str;

  String str_volume = "volume";
  String play = "play";
  String pause = "pause";
  String start = "start";

  if(Serial.available() > 0) {
    
    str = Serial.readStringUntil(' ');
    //str.trim();
    x = Serial.parseInt();

    //Serial.println(str);
    //Serial.println(x);
  }



  if(str == str_volume) {
    //Serial.println(str);
    //Serial.println(x);
    volume = x;
    myDFPlayer.volume(x);
  } else if (str == play) {
      myDFPlayer.play(x);
  } else if (str == pause) {
    //myDFPlayer.volume(1);
    myDFPlayer.pause();
  } else if (str == start) {
    //myDFPlayer.volume(volume);
    myDFPlayer.start();
  }

  if (myDFPlayer.available()) {
    printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
  }

  

  if (readerDisabled) {
    if (millis() - timeLastCardRead > DELAY_BETWEEN_CARDS) {
      readerDisabled = false;
      startListeningToNFC();



    }
    
  } else {
    irqCurr = digitalRead(PN532_IRQ);

    // When the IRQ is pulled low - the reader has got something for us.
    if (irqCurr == LOW && irqPrev == HIGH) {
       //Serial.println("Got NFC IRQ");  
       //counter = 0;
       handleCardDetected(); 
    } else {
      

      //isPlaying = 0;
    }
  
    irqPrev = irqCurr;
  }



  //detectNFC();
}

void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

void startListeningToNFC() {
  // Reset our IRQ indicators
  irqPrev = irqCurr = HIGH;
  
  //Serial.println("Waiting for an ISO14443A Card ...");
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

void handleCardDetected() {
    uint8_t success = false;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    // read the NFC tag's info
    success = nfc.readDetectedPassiveTargetID(uid, &uidLength);
    //Serial.println(success ? "Read successful" : "Read failed (not a card?)");

    if (success) {
      // Display some basic information about the card
      //Serial.println("Found an ISO14443A card");
      //Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      //Serial.print("  UID Value: ");
      nfc.PrintHex(uid, uidLength);

      
      counter = 0;
          
      if (uidLength == 4) {

        if(uid[0] == 0xD6 && uid[1] == 0xA0 && uid[2] == 0x1B && uid[3] == 0xF8) {
          if(isPlaying != 3) {
            isPlaying = 3;
            myDFPlayer.play(isPlaying);
          }
        }
        else if(uid[0] == 0xDB && uid[1] == 0x5E && uid[2] == 0x43 && uid[3] == 0x0B) {
          if(isPlaying != 10) {
            isPlaying = 10;
            myDFPlayer.play(isPlaying);
          }
        }
      } else if (uidLength == 7) {
        if(uid[0] == 0x04 && uid[1] == 0x3F && uid[2] == 0x78 && uid[3] == 0xC2 && uid[4] == 0x83 && uid[5] == 0x36 && uid[6] == 0x80) {
          if(isPlaying != 2) {
            isPlaying = 2;
            myDFPlayer.play(isPlaying);
          }
        }
      }

      


      /*
      if (uidLength == 4)
      {
        // We probably have a Mifare Classic card ... 
        uint32_t cardid = uid[0];
        cardid <<= 8;
        cardid |= uid[1];
        cardid <<= 8;
        cardid |= uid[2];  
        cardid <<= 8;
        cardid |= uid[3]; 
        Serial.print("Seems to be a Mifare Classic card #");
        Serial.println(cardid);
      }
      Serial.println("");

      */

      timeLastCardRead = millis();
    }

    // The reader will be enabled again after DELAY_BETWEEN_CARDS ms will pass.
    readerDisabled = true;
}
