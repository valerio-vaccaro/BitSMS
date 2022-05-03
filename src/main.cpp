/* 
  BitSMS: Receive a Bitcoin transaction using SMS. 
  
  Board:
  - TTGO T-Call ESP32 with SIM800L GPRS Module
    https://my.cytron.io/p-ttgo-t-call-esp32-with-sim800l-gprs-module

  External libraries:
  - Adafruit Fona Library by Adafruit
*/

#include "Adafruit_FONA.h"

#define SIM800L_RX     27
#define SIM800L_TX     26
#define SIM800L_PWRKEY 4
#define SIM800L_RST    5
#define SIM800L_POWER  23
#define LED_BLUE  13

#define APN "iliad"
//#define CLEAN_ON_BOOT
#define ELEMENTS_NUMBER 10

String number[ELEMENTS_NUMBER];
String message[ELEMENTS_NUMBER];
long update_millis[ELEMENTS_NUMBER];

HardwareSerial *sim800lSerial = &Serial1;
Adafruit_FONA sim800l = Adafruit_FONA(SIM800L_PWRKEY);
char replybuffer[255];

long prevMillis = 0;
int interval = 60 * 1000;
char sim800lNotificationBuffer[64];
char smsBuffer[250];
String smsString = "";
boolean ledState = false;

void setup()
{
  pinMode(LED_BLUE, OUTPUT);
  pinMode(SIM800L_POWER, OUTPUT);

  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(SIM800L_POWER, HIGH);

  Serial.begin(115200);
  Serial.println("ESP32 with GSM SIM800L");
  Serial.println("Initializing....(May take more than 10 seconds)");
  
  delay(10000);

  // Make it slow so its easy to read!
  sim800lSerial->begin(4800, SERIAL_8N1, SIM800L_TX, SIM800L_RX);
  if (!sim800l.begin(*sim800lSerial)) {
    Serial.println("Couldn't find GSM SIM800L");
    while (1);
  }
  Serial.println("GSM SIM800L is OK");

  // get imei 
  char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = sim800l.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("SIM card IMEI: "); Serial.println(imei);
  }

  // set apn
  sim800l.setGPRSNetworkSettings(F(APN), F(""), F(""));
  delay(1000);

  // use HTTPS, only redirect for L
  // sim800l.HTTP_ssl(true);
  // sim800l.setHTTPSRedirect(true);

  // enable GPRS
  if (!sim800l.enableGPRS(true))
    Serial.println("Failed to turn GPRS on");

  // GSMLOC
  uint16_t returncode;
  if (!sim800l.getGSMLoc(&returncode, replybuffer, 250))
    Serial.println("Failed GSMLOC!");
  if (returncode == 0) {
    Serial.println(replybuffer);
  } else {
    Serial.print("Fail GSMLOC code #"); 
    Serial.println(returncode);
  }

  // Set up the FONA to send a +CMTI notification
  // when an SMS is received
  sim800lSerial->print("AT+CNMI=2,1\r\n");

  // clean sms slots if needed
#ifdef CLEAN_ON_BOOT
  for (int i=0; i<10; i++) {
    if (sim800l.deleteSMS(i)) {
      Serial.println("OK!");
      break;
    }
    else {
      Serial.print("Couldn't delete SMS in slot "); Serial.println(i);
      sim800l.print("AT+CMGD=?\r\n");
    }
  }
  Serial.println("GSM SIM800L clean end");
#endif

  // reset buffer
  for (int i=0; i < ELEMENTS_NUMBER; i++) {
    number[i] = String("");
    message[i] = String("");
    update_millis[i] = millis();
  }

  Serial.println("GSM SIM800L Ready");
}


void loop()
{
  // led 
  if (millis() - prevMillis > 60*1000) {
    ledState = !ledState;
    digitalWrite(LED_BLUE, ledState);
    prevMillis = millis();
    long actual_timestamp = millis();
    // send if needed
    for (size_t i=0; i < ELEMENTS_NUMBER; i++) {
      uint16_t statuscode;
      int16_t length;
      bool complete = false;
      bool sent = false;
      String result = String("error");
      if (number[i] != String("")){
        // check if complete
        String url = "psbt.it/testmempoolaccept.php";
        Serial.println(url);
        if (!sim800l.HTTP_POST_start((char*) url.c_str(), F("text/plain"), (uint8_t *) message[i].c_str(), strlen(message[i].c_str()), &statuscode, (uint16_t *)&length)) {
          Serial.print(url);
          Serial.println(" Failed!");
        } else {
          result = String("");
          while (length > 0) {
            while (sim800l.available()) {
              char c = sim800l.read();
              result = result + String(c);
              length--;
            }
          }
          sim800l.HTTP_GET_end();
          Serial.println(result);
          if (result.indexOf("error") == -1){
            complete = true;
          }
        }

        result = String("error");
        if (complete){
          // try to broadcast
          String url = "psbt.it/sendrawtransaction.php";
          Serial.println(url);
          if (!sim800l.HTTP_POST_start((char*) url.c_str(), F("text/plain"), (uint8_t *) message[i].c_str(), strlen(message[i].c_str()), &statuscode, (uint16_t *)&length)) {
            Serial.print(url);
            Serial.println(" Failed!");
          }
          result = String("");
          while (length > 0) {
            while (sim800l.available()) {
              char c = sim800l.read();
              result = result + String(c);
              length--;
            }
          }
          sim800l.HTTP_GET_end();
          Serial.println(result);
          if (result.indexOf("error") == -1){
            sent = true;
          }

          if (sent){
            // send reply sms
            if (!sim800l.sendSMS((char*)number[i].c_str(), "Transaction was sent!")) {
              Serial.println("Failed");
            } else {
              Serial.println("Sent!");
            }
          } else {
            // send reply sms
            if (!sim800l.sendSMS((char*)number[i].c_str(), "Transaction was not sent!")) {
              Serial.println("Failed");
            } else {
              Serial.println("Sent!");
            }
          }

          // clean table 
          number[i] = String("");
          message[i] = String("");
          update_millis[i] = millis();
        }
      }

    }
    // cleanup if needed
    for (size_t i=0; i < ELEMENTS_NUMBER; i++) {
      // if expired (and used) delete
      if ((number[i] != String("")) && ((actual_timestamp - update_millis[i]) > 300*1000)){
        Serial.print("EXPIRED!!! Element: "); Serial.print(i);
        Serial.print(" Number: "); Serial.print(number[i]);
        Serial.print(" -> "); Serial.println(message[i]);
        // send reply sms
        if (!sim800l.sendSMS((char*)number[i].c_str(), "Transaction was incomplete!")) {
          Serial.println("Failed");
        } else {
          Serial.println("Sent!");
        }
        // clean table 
        number[i] = String("");
        message[i] = String("");
        update_millis[i] = millis();
      }
    }
  }
  
  char* bufPtr = sim800lNotificationBuffer;    //handy buffer pointer

  if (sim800l.available()) {
    int slot = 0; // this will be the slot number of the SMS
    int charCount = 0;

    // Read the notification into fonaInBuffer
    do {
      *bufPtr = sim800l.read();
      Serial.write(*bufPtr);
      delay(10);
    } while ((*bufPtr++ != '\n') && (sim800l.available()) && (++charCount < (sizeof(sim800lNotificationBuffer)-1)));
    
    // Add a terminal NULL to the notification string
    *bufPtr = 0;

    // Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(sim800lNotificationBuffer, "+CMTI: \"SM\",%d", &slot)) {
      Serial.print("slot: "); Serial.println(slot);
      
      char callerIDbuffer[32];  //we'll store the SMS sender number in here
      
      // Retrieve SMS sender address/phone number.
      if (!sim800l.getSMSSender(slot, callerIDbuffer, 31)) {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print("FROM: "); Serial.println(callerIDbuffer);

      // Retrieve SMS value.
      uint16_t smslen;
      // Pass in buffer and max len!
      if (sim800l.readSMS(slot, smsBuffer, 250, &smslen)) {
        smsString = String(smsBuffer);
        Serial.println(smsString);
      }

      // add message
      size_t counter;
      for (counter=0; counter < ELEMENTS_NUMBER; counter++)
        if (number[counter] == String(callerIDbuffer))
          break;
      if (counter == ELEMENTS_NUMBER) { // not found
        for (counter=0; counter < ELEMENTS_NUMBER; counter++)
          if (number[counter]  == String(""))
            break;
        if (counter == ELEMENTS_NUMBER) { // no free space
           Serial.println("No more free space!");
        } else {
          number[counter] = String(callerIDbuffer);
          message[counter] = smsString;
          update_millis[counter] = millis();
        }

      } else { // found
        number[counter] = String(callerIDbuffer);
        message[counter] = message[counter] + smsString;
        update_millis[counter] = millis();
      }

      // print buffer
      for (size_t i=0; i < ELEMENTS_NUMBER; i++) {
        Serial.print("Element: "); Serial.print(i);
        Serial.print(" Number: "); Serial.print(number[i]);
        Serial.print(" -> "); Serial.println(message[i]);
      }

      // clean up
      while (1) {
        if (sim800l.deleteSMS(slot)) {
          Serial.println("OK!");
          break;
        }
        else {
          Serial.print("Couldn't delete SMS in slot "); Serial.println(slot);
          sim800l.print("AT+CMGD=?\r\n");
        }
      }

    }
  }
}