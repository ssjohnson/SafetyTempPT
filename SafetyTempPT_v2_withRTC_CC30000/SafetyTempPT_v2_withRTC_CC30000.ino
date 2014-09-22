// xively: grud password: nodiving

#include <Wire.h>
#include <SoftwareSerial.h>
#include <Keypad.h>
#include <OneWire.h>
#include <EEPROM.h>
#include "RTClib.h"
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include <Adafruit_CC3000.h>
#include <stdlib.h>
#include "utility/debug.h"

//Pin Assignment for signal LEDs, Error speaker, Relay

#define GreenPin 12
#define RedPin 11
#define speakerPin 22 
#define HeaterRelayOne 31
#define IndicatorLight 33

//Pin Assignment for CC3000

#define ADAFRUIT_CC3000_IRQ   3
#define ADAFRUIT_CC3000_VBAT  6
#define ADAFRUIT_CC3000_CS    10

//WiFi info for CC3000

#define WLAN_SSID "44DX0"
#define WLAN_PASS "38776bab08"
#define WLAN_SECURITY WLAN_SEC_WPA2

//Xivley.com info for CC3000

#define WEBSITE  "api.xively.com"
#define API_key  "JVDutNPLJjmyjS7pkuf3RyAnzv8q6kf1aLd5BL8kZnfQcQV9"
#define feedID  "1547134853"

//CC3000 declaration

Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIV2);

// LCD Screen pin assignment & declaration

SoftwareSerial mySerial(2, 9);

// Real Time Clock declaration

RTC_DS1307 RTC;

// Thermometer Declaration

OneWire ds(8);

// upthreshold - Max temp heater can be at before cutting off, in degrees Celcius
int upthreshold = 22;

// temp - Temperature read from thermometer
float temp;

// password[4] - Array holding 4 numbers to use as password to change options
char password[4];

// inMenu - Determines whether to loop the temperature message or to enter the menu
static boolean inMenu = false;

// Keypad declaration - uses 2D array to create a map of the keypad to be read by arduino

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {34, 36, 38, 40}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {42, 44, 46}; //connect to the column pinouts of the keypad
 
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

/*******************************************************
// backlightOn()
// 
// Activates backlight on LCD Screen
*******************************************************/

void backlightOn()
{
  mySerial.write(0x7C);
  mySerial.write(157);
}

/*******************************************************************
// clearLCD()
//
// Clears LCD screen. Without this, letters will remain static until
// overwritten
*******************************************************************/
 
void clearLCD()
{
  mySerial.write(0xFE); // hex code to bring up command mode
  mySerial.write(0x01); // hex code to clear screen
}

/*******************************************************
// Setup - 
// A: Set Pins, Attach Interrupt
// B: Begin RTC, Wire, CC3000. Check Status
// C: Display Opening Output
// D: Attach Interrupt
// E: Check Password Exists
*******************************************************/

void setup()
{
  mySerial.begin(9600);
  Serial.begin(57600);
  
  pinMode(HeaterRelayOne, OUTPUT);
  pinMode(IndicatorLight, OUTPUT);
  pinMode(GreenPin, OUTPUT); 
  pinMode(RedPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);
  
  attachInterrupt(0, startMenu, HIGH);
  
  Wire.begin();
  RTC.begin();
  
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  if (! RTC.isrunning())
  {
     RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  
  clearLCD();
  delay(500);
  mySerial.print(" **safetytemp**");
  mySerial.print(" (c)Giov-Rud Tech");
  delay(3000);
  
  if(!flash_check())
    startPW();
  else
    getPW();
}

/************************************************************
// flash_check()
// Checks first four locations of EEPROM for data. 
// Returns False: No Data in any singular location
// Returns True: Data in ALL four locations.
************************************************************/

boolean flash_check()
{
  for(int i = 0; i < 4; i++)
  {
    if(EEPROM.read(i) == 255)
      return false; 
  }
  return true;
}

/************************************************************
// startPW()
//
// Asks user for input in the form of 4 integers from the
// keypad to use as a password
************************************************************/

void startPW()
{
  clearLCD();
  mySerial.println("Enter Password: ");   
  for(int i = 0; i < 4; i++)
  {
    char key = keypad.waitForKey();
    mySerial.print(key);
    password[i] = key;
    EEPROM.write(i, key);
  }
  delay(1000);
}

/**************************************************************
// getPW()
// Reads first four locations of EEPROM and stores them in the
// global password[] array.
**************************************************************/

void getPW()
{
  for(int i = 0; i < 4; i++)
  {
    password[i] = EEPROM.read(i);
  }  
}

/***************************************************************
// checkPW()
// Prompts user to enter password. Checks that against the
// password saved in the first four locations of EEPROM.
// 
// Return TRUE: Input and EEPROM contents match
// Return FALSE: Input and EEPROM contents do not match
***************************************************************/

boolean checkPW()
{
  char new_password[4];
  clearLCD();
  mySerial.print("Confirm Password");
  for(int i = 0; i < 4; i++)
  {
    char key = keypad.waitForKey();
    mySerial.print(key);
    new_password[i] = key;
  }
  delay(1000);
  for(int i = 0; i < 4; i++)
  {  
    if(password[i] != new_password[i])
    {
      clearLCD();
      mySerial.print("Wrong Password");
      delay(1000);
      return false;   
    }  
  }
  return true;
}

/**********************************************************************
// loop() 
// A: Checks global boolean inMenu
//    FALSE - Enter looptemp()
//    TRUE - 1: Enter optionChoice(). Returns int.
//           2: Returns 1:
//               A: check password. 
//               B: prompt user for new password & store
//               C: exit menu by changing global bool inMenu = f
//              Returns 2: 
//               A: check password
//               B: prompt user for new upper limit. 3 digit, ºC
//               C: store in local array, pass into createUpperLimit()
//               D: redisplay info to user
*********************************************************************/
void loop()
{
  if(inMenu == false)
  {
    looptemp();
  }
  else if(inMenu == true)
  {
    int choice = optionChoice();
    if(choice == 1)
    {
      if(checkPW())
      {
        clearLCD();
        mySerial.print("Enter New PW    ");
        for(int i = 0; i < 4; i++)
        {
          char key = keypad.waitForKey();
          mySerial.print(key);
          password[i] = key;
          EEPROM.write(i, key);
        }
        delay(1000);
        inMenu = false;
      }
    }
    else if(choice == 0)
    {
      if(checkPW())
      {
        clearLCD();
        mySerial.print("Enter Upper Limit:");
        char upperlimit[3];
        for(int i = 0; i < 3; i++)
        {
          char key = keypad.waitForKey();
          upperlimit[i] = key;
          mySerial.print(key);
          upthreshold = createUpper(upperlimit);
        }
        clearLCD();
        mySerial.print("Upper Limit Is Now: ");
        mySerial.print(upthreshold);
        delay(2000);
        inMenu = false;
      }
    }
  }
}

/*********************************************************************
// createUpper(char upperlimit[3])
//
// Takes char array passed in by user, and stores the three keystrokes
// in each element. Subtract '0' from each to convert it to an int.
// Multiply by denominations of ten to create a three digit number.
//
// Return integer total
**********************************************************************/

int createUpper(char upperlimit[3])
{
  int first = upperlimit[0] - '0';
  int sec = upperlimit[1] - '0';
  int third = upperlimit[2] - '0';
  return first*100 + sec * 10 + third;  
}

/********************************************************************************************************************
// int optionChoice()
//
// Function to display the options menu. 
//
// --exit_timer, scroll_time, enter_time - Timers keeping track of how long we are present within the menu
// --last_scroll_time, last_enter_time - Timers keeping track of how long since the last scroll / enter button press to 
//                                       ensure only one input is read at a time
// --menu_choice - integer representing 2 menu options:
//                 0: Change Upper Limit
//                 1: Change Password
// --scroll - HIGH is scroll button is pressed
// --enter - HIGH if enter button is pressed
// 
// While loop checks time Executes while loop until:
//        A: Idle for 10 seconds. Return to looptemp()
//        B: Enter button selects an option. Return menu_choice
//        C: Scroll button pressed: display other options
//
********************************************************************************************************************/

int optionChoice()
{
  static unsigned long exit_timer = millis();
  unsigned long exit_timer_reset = millis();
  static unsigned long last_scroll_time = 0;
  unsigned long scroll_time = millis();
  static unsigned long last_enter_time = 0;
  unsigned long enter_time = millis();
  
  int menu_choice = 0;
  boolean notPrinted = true;
  
  clearLCD();
  
  while(inMenu)
  {
    exit_timer = millis();
    if(exit_timer - exit_timer_reset > 10000)
    {
      inMenu = false;     
    }
    if(notPrinted)
    {
      switch(menu_choice) {
        case 0:
          clearLCD(); 
          mySerial.print("Change Upper Lim");
          break;
        case 1:
          clearLCD();
          mySerial.print("Change Password");
          break;
      }
      notPrinted = false;
    }
    
    int scroll = digitalRead(5);
    int enter = digitalRead(4);
    
    if(scroll == HIGH)
    {
      scroll_time = millis();
      if(scroll_time - last_scroll_time > 300)
      {
        if(menu_choice == 0)
          menu_choice = 1;
        else
          menu_choice = 0;
        notPrinted = true;
      }
    }
    else if(enter == HIGH)
    {
      enter_time = millis();
      if(enter_time - last_enter_time > 300)
      {
        return menu_choice;  
      }
    }
    last_scroll_time = scroll_time;
    last_enter_time = enter_time;    
  }  
}

//********************************************************************************
// startMenu()
//
// Function to debounce the interrupt on Arduino board, so only one press is read.
// On interrupt, sets inMenu to true, so looptemp() enters the menu during loop.
//
//********************************************************************************
void startMenu()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 1000)
  {
    inMenu = true;
  }
  last_interrupt_time = interrupt_time;
}

//***************************************************************************************
// internet_count - Global so is not reset with every instance of looptemp()
//                  Used to submit data to xivley with every 4 iterations of looptemp()
//
//***************************************************************************************
int internet_count = 0;

//***************************************************************************************
// looptemp()
//
// A: Checks inMenu at several stages. If true return to loop()
// B: If False:
//      1- obtain temperature from thermometer
//      2- convert to farenheit
//      3- Print temp message on LCD
//      4- If temp not within range, shut off relay, display error message
//      5- If temp is in range, display message, return to loop()
//
//***************************************************************************************

void looptemp()
{
    if(inMenu == true)
      return;
    getTemperature();
    float faren = ((temp * 9)/5 + 32); //farenheit conversion
    clearLCD();
    mySerial.print("The Spa Temp is");
    delay(2000);
    if(inMenu == true)
      return;
    clearLCD();
    mySerial.print(temp);
    mySerial.print(" degrees C ");
    mySerial.print(faren);
    mySerial.print (" degrees F");
    delay(2000);
    if(inMenu == true)
      return;
    if(temp > upthreshold && !inMenu) 
    {
      digitalWrite(RedPin, HIGH); // turn on red pin
      digitalWrite(GreenPin, LOW); // turn off green pin
      digitalWrite(HeaterRelayOne, LOW);
      digitalWrite(IndicatorLight, LOW);
      analogWrite(speakerPin, 128); // turn on piezo speaker
      clearLCD();
      mySerial.print(" SYSTEM NOT OK  :( "); 
      delay(2000);
    }
    else if (!inMenu)
    {
      digitalWrite(RedPin, LOW); // turn off red pin
      digitalWrite(GreenPin, HIGH); //turn on green pin
      digitalWrite(HeaterRelayOne, HIGH);
      digitalWrite(IndicatorLight, HIGH);
      digitalWrite(speakerPin, LOW); //leave speaker off
      clearLCD();
      mySerial.print("  SYSTEM OK :) ");
      mySerial.print("   HEATER ON :)    ");
      delay(2000);
    }
    callTime();
    delay(2000);
    if(internet_count == 4)
    {
        if(transmitData(temp, faren))
        {
        }
        else
        {
          mySerial.print("Check Conn");
          delay(2000);
        }
        internet_count = 0;
    }
    internet_count++;        
}

//***********************************************************************************
// getTemperature()
//
// Uses OneWire library to read values from thermometer and convert into a temperature
// value. 
//
//***********************************************************************************
boolean getTemperature()
{
    byte i;
    byte present = 0;
    byte data[12];
    byte addr[8];
    if (!ds.search(addr)) 
    {
      ds.reset_search();
      return false;
    }
    if (OneWire::crc8( addr, 7) != addr[7]) 
    {
      return false;
    }
    ds.reset();
    ds.select(addr);
    // Start conversion from HEX
    ds.write(0x44, 1);
    // Wait...
    delay(850);
    present = ds.reset();
    ds.select(addr);
    // Issue Read scratchpad command
    ds.write(0xBE);
    // Receive 9 bytes
    for ( i = 0; i < 9; i++) 
    {
      data[i] = ds.read();
    }
    // Calculate temperature value
    temp = ( (data[1] << 8) + data[0] )*0.0625;
    return true;
}

//***********************************************************************************
// callTime()
//
// Displays real time date & time using RTC library. Prints to screen and returns
//
//***********************************************************************************

void callTime()
{
    clearLCD();
    DateTime now = RTC.now();
    mySerial.print(now.month(), DEC);
    mySerial.print('/');
    mySerial.print(now.day(), DEC);
    mySerial.print('/');
    mySerial.print(now.year(), DEC);
    mySerial.print(' ');
    if(now.hour() > 12)
      mySerial.print(now.hour() - 12, DEC);
    else if(now.hour() == 0)
      mySerial.print(12, DEC);
    else  
      mySerial.print(now.hour(), DEC);
    mySerial.print(':');
    if(now.minute() < 10)
      mySerial.print("0");
    mySerial.print(now.minute(), DEC);
    mySerial.print("  **safetytemp**");
}

//*********************************************************************************************
// transmitData(int celcius, int faren)
// - int celcius - temperature in degrees celcius
// - int faren - temperature in degrees farenheit
//
// Function connects to xivley.com using the CC3000 library. 
// A: CC3000 connects to local wifi network
// B: Checks to see if CC3000 can connect to host website at ip
// C: Converts data to JSON string, readable by xivley
// D: Connects and transmits data to xivley
// E: Waits to see if any reply from site
// F: Closes Connection. Return true to loop()
//
//***********************************************************************************************

boolean transmitData(int celcius, int faren)
{
  uint32_t ip;
  
  Serial.println("Celcius: " + String(celcius) + "\nFaren: " + String(faren) + "\n");
  
  cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);
  while(!cc3000.checkDHCP())
  {
    delay(100);
  }
  ip=0;
  Serial.println(WEBSITE);
  Serial.println(F(" -> "));
  while (ip == 0) {
    if(! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }
  cc3000.printIPdotsRev(ip);
  int length = 0;
 
  String data = "";
  data = data + "\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ {\"id\" : \"Celcius\",\"current_value\" : \"" + String(celcius) + "\"}," 
   + "{\"id\" : \"Farenheight\",\"current_value\" : \"" + String(faren) + "\"}]}";
  
  length = data.length();
  
  Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
  
  if (client.connected()) {
    Serial.println("Connected!");
    client.println("PUT /v2/feeds/" + String(feedID) + ".json HTTP/1.0");
    Serial.println("PUT /v2/feeds/" + String(feedID) + ".json HTTP/1.0");
    client.println("Host: api.xively.com");
    Serial.println("Host: api.xively.com");    
    client.println("X-ApiKey: " + String(API_key));    
    Serial.println("X-ApiKey: " + String(API_key));
    client.println("Content-Length: " + String(length));
    Serial.println("Content-Length: " + String(length));    
    client.print("Connection: close");
    Serial.print("Connection: close");
    client.println();
    Serial.println();
    client.print(data);
    Serial.print(data);
    client.println();
    Serial.println();
  }
  else
  {
    return false;
  }
  
  
  while (client.connected()) {
    Serial.println("READING!");
    while (client.available()) 
    {
     char c = client.read();
     Serial.print(c);
     Serial.println("DONE");
    }
  }
 
  client.close();
  cc3000.disconnect();
  Serial.println("DCed");
  return true;

}


