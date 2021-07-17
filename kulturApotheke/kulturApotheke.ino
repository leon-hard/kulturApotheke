#include <ClickEncoder.h>

#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <ClickEncoder.h>
#include <TimerOne.h>

struct Tag 
{
  int byte_length;
  int song_number;
  bool continue_playing;
  uint8_t id[7];
};


Tag tags[] = { {4, 1, 0, {0xD6,0xA0,0x1B,0xF8}}, 
               {4, 2, 0, {0xDB,0x5E,0x43,0x0B}}, 
               {7, 3, 1, {0x04,0x3F,0x78,0xC2,0x83,0x36,0x80}}, 
             };

/*
Tag tags[] = { {4, 1, 0, 0}, 
               {4, 2, 0, 1}, 
             //  {7, 3, 1, {0x04,0x3F,0x78,0xC2,0x83,0x36,0x80}}, 
             };
*/
// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield
#define POWER_ON_OFF (9)
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
int volume = 20;
uint16_t isPlaying = 0;
uint8_t keepPlaying;
uint16_t counter = 0;
uint16_t counter_shutdown = 0;
uint16_t n_shutdown = 0;

const int analogInPinButton = A3;  
const int analogInPinBattery = A2;  // 480 = 3.8 V => 3.2 V = 404
#define BATTERY_LOW 420
#define BATTERY_LOW_HYSTERESIS  5
int sensorValue = 0;        // value read from the pot

int LEDblue = 6; 
int LEDred = 4;
int LEDgreen = 5; 


int continue_playing = 0;
int isFinished = 0;



ClickEncoder *encoder;
int16_t last, value;

void timerIsr()
{
  encoder->service();
}


void setup()
{
  pinMode(POWER_ON_OFF, OUTPUT);
  digitalWrite(POWER_ON_OFF, HIGH);

  pinMode(LEDblue, OUTPUT);
  digitalWrite(LEDblue, HIGH);
  pinMode(LEDgreen, OUTPUT);
  digitalWrite(LEDgreen, LOW);
  pinMode(LEDred, OUTPUT);
  digitalWrite(LEDred, HIGH);
  
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

  myDFPlayer.volume(volume);  //Set volume value. From 0 to 30
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

  encoder = new ClickEncoder(A0, A1);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  last = -1;
  
}

void loop()
{

  value = volume;
  value += encoder->getValue();

  if (value != last)
  {
    if (value > 30)
    {
      value = 30;
    }
    else if (value < 0)
    {
      value = 0;
    }
    last = value;
    volume = last;
    Serial.print("Encoder Value: ");
    Serial.println(value);
    myDFPlayer.volume(volume);
  }
  
  static unsigned long timer = millis();

  static int counter_pause = 0;
  static int counter_finished = 0;

  if (millis() - timer > 100) {
    timer = millis();

    if(counter++ > 1) {

      if(keepPlaying == 0) {
        counter = 0;
        isPlaying = 0;
        digitalWrite(LEDblue, HIGH);
        myDFPlayer.pause();
      }
    

      if(continue_playing) {
        if(counter_pause++ > 50) {
          counter_pause = 0;
          continue_playing = 0;
        }
      }

      if(isFinished) {
        if(counter_finished++ > 1) {
          isFinished = 0;
          //continue_playing = 0;
          keepPlaying = 0;
          counter_finished = 0;
        }
      }


    }



    
    //startListeningToNFC();
    //myDFPlayer.next();  //Play next mp3 every x second.


    if(counter_shutdown++ > 5) {
      counter_shutdown = 0;

      static uint8_t counter_lowpass_battery = 0;
      static uint16_t sum_lowpass_battery = 0;


      sum_lowpass_battery += analogRead(analogInPinBattery);

      if(++counter_lowpass_battery >= 8) {
        
        int analogInBattery = sum_lowpass_battery >> 3;
        sum_lowpass_battery = 0;
        counter_lowpass_battery = 0;
  
        Serial.print("Battery\t");
        Serial.println(analogInBattery);
        //Serial.print(" | ");

        if(analogInBattery < (BATTERY_LOW - BATTERY_LOW_HYSTERESIS)) {
          digitalWrite(LEDred, LOW);
        } else if(analogInBattery > (BATTERY_LOW + BATTERY_LOW_HYSTERESIS)) {
          digitalWrite(LEDred, HIGH);
        }
      }
      

      static uint16_t LED_state = 0;
      
      if(LED_state == 0) {
        
        static uint16_t LED_counter = 0;

        if(LED_counter++ >= 3) {
            digitalWrite(LEDred, HIGH);
            digitalWrite(LEDgreen, HIGH);
            digitalWrite(LEDblue, HIGH);
            LED_state = 1;
        }


      }
      
      sensorValue = analogRead(analogInPinButton);

      //Serial.print("Switch: ");
      //Serial.println(sensorValue);

      if(sensorValue > 650) {
        n_shutdown++;
        digitalWrite(LEDred, LOW);
      } else {
        n_shutdown = 0;
        //digitalWrite(LEDred, HIGH);
      }

      if(n_shutdown >= 3){

        digitalWrite(LEDblue, HIGH);
        digitalWrite(LEDgreen, HIGH);
        digitalWrite(LEDred, HIGH);
        digitalWrite(POWER_ON_OFF, LOW);
      } else {



        /*
        switch(LED_counter) {
          case 0:
            digitalWrite(LEDblue, LOW);
            digitalWrite(LEDgreen, HIGH);
            digitalWrite(LEDred, HIGH);
            break;
          case 1:
            digitalWrite(LEDblue, HIGH);
            digitalWrite(LEDgreen, LOW);
            digitalWrite(LEDred, HIGH);
            break;
          case 2:
            digitalWrite(LEDblue, HIGH);
            digitalWrite(LEDgreen, HIGH);
            digitalWrite(LEDred, LOW);
            break;
        }
  
        if(++LED_counter > 2) {
          LED_counter = 0;
        }
        */
      }
    }
  }
  

  int x = 0;
  String str;

  String str_volume = "volume";
  String play = "play";
  String pause = "pause";
  String start = "start";
  String power = "power";

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
  } else if (str == power) {
    digitalWrite(POWER_ON_OFF, x);
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

      //continue_playing = 0;
      //isPlaying = 0;
      isFinished = 1;
      keepPlaying = 0;
      digitalWrite(LEDblue, HIGH);
      
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

      for(Tag &tag:tags) {

        if(tag.byte_length == 4) {
          if(uid[0] == tag.id[0] && uid[1] == tag.id[1] && uid[2] == tag.id[2] && uid[3] == tag.id[3]) {
          //if(uid[0] == tag.id) {
            if(isPlaying != tag.song_number) {
              isPlaying = tag.song_number;
              keepPlaying = tag.continue_playing;
              if(!isFinished) {
                
                digitalWrite(LEDblue, LOW);
                if(!continue_playing) {
                  continue_playing = 1;
                  myDFPlayer.play(isPlaying);
                } else {
                  myDFPlayer.start();
                }
              }

            }
          }
        } else if(tag.byte_length == 7) {
          if(uid[0] == tag.id[0] && uid[1] == tag.id[1] && uid[2] == tag.id[2] && uid[3] == tag.id[3] && uid[4] == tag.id[4] && uid[5] == tag.id[5] && uid[6] == tag.id[6]) {
            if(isPlaying != tag.song_number) {
              isPlaying = tag.song_number;
              keepPlaying = tag.continue_playing;
              if(!isFinished) {
                
                digitalWrite(LEDblue, LOW);
                if(!continue_playing) {
                  continue_playing = 1;
                  myDFPlayer.play(isPlaying);
                } else {
                  myDFPlayer.start();
                }
              }
            }
          }
        }
      }


      /*
      if (uidLength == 4) {

        if(uid[0] == 0xD6 && uid[1] == 0xA0 && uid[2] == 0x1B && uid[3] == 0xF8) {
          if(isPlaying != 3) {
            isPlaying = 3;
            digitalWrite(LEDblue, LOW);
            myDFPlayer.play(isPlaying);
          }
        }
        else if(uid[0] == 0xDB && uid[1] == 0x5E && uid[2] == 0x43 && uid[3] == 0x0B) {
          if(isPlaying != 1) {
            isPlaying = 1;
            digitalWrite(LEDblue, LOW);
            myDFPlayer.play(isPlaying);
          }
        }
      } else if (uidLength == 7) {
        if(uid[0] == 0x04 && uid[1] == 0x3F && uid[2] == 0x78 && uid[3] == 0xC2 && uid[4] == 0x83 && uid[5] == 0x36 && uid[6] == 0x80) {
          if(isPlaying != 2) {
            isPlaying = 2;
            digitalWrite(LEDblue, LOW);
            myDFPlayer.play(isPlaying);
          }
        }
      }

      */

      


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
