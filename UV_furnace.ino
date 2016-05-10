//SD Card Library 
#include <SD.h>
#include <SPI.h>

// PID Library
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>

// DS3231 Library
#include <DS3232RTC.h>
#include <Time.h> 

//Library for LCD touch screen
#include "Nextion.h"

// So we can save and retrieve settings
#include <EEPROM.h>

// Library for Thermocouple Amplifier
#include <SPI.h>
#include "Adafruit_MAX31855.h"

// ************************************************
// chart definitions
// ************************************************

#define inputOffset 0
double targetTemp = 0;

// ************************************************
// Pin definitions
// ************************************************

/*SC-Card
 ** MOSI - pin 51
 ** MISO - pin 50
 ** CLK - pin 52
 ** CS - pin 53
 CS pin for SD card */
const int chipSelect = 53;
File dataFile;

// DS3231 Interrupt Pin
#define SQW_PIN 2

// Output Relay
#define RelayPin 31

// LEDs
byte b0Pin = 13;
byte b1Pin = 12;
byte b2Pin = 11;

// LED Variables
byte LED1_intensity = 0;
byte LED2_intensity = 0;
byte LED3_intensity = 0;

// Thermocouple Amplifier
// Default connection is using software SPI, but comment and uncomment one of
// the two examples below to switch between software SPI and hardware SPI:

// Creating a thermocouple instance with software SPI on any three
// digital IO pins.
#define DO   3
#define CS   4
#define CLK  5
Adafruit_MAX31855 thermocouple(CLK, CS, DO);



// ************************************************
// PID Variables and constants
// ************************************************

//Define Variables we'll be connecting to
double Setpoint;
double Input;
double Output;

char temporaer[10] = {0};
byte hours_oven = 0;
byte minutes_oven = 0;

byte hours_led = 0;
byte minutes_led = 0;

 
volatile long onTime = 0;
 
// pid tuning parameters
double Kp;
double Ki;
double Kd;
 
// EEPROM addresses for persisted data
const int SpAddress = 0;
const int KpAddress = 8;
const int KiAddress = 16;
const int KdAddress = 24;
 
//Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);
 
// 10 second Time Proportional Output window
int WindowSize = 10000; 
unsigned long windowStartTime;
 
// ************************************************
// Auto Tune Variables and constants
// ************************************************
byte ATuneModeRemember=2;
 
double aTuneStep=500;
double aTuneNoise=1;
unsigned int aTuneLookBack=20;
 
boolean tuning = false;
 
PID_ATune aTune(&Input, &Output);


// ************************************************
// Sensor Variables and constants
// ************************************************

bool b0State = false;
bool b1State = false;
bool b2State = false;

const int logInterval = 1000; // log every 1 seconds
long lastLogTime = 0;

// ************************************************
// States for state machine
// ************************************************
enum operatingState { OFF = 0, SETP, RUN, TUNE_P, TUNE_I, TUNE_D, AUTO};
operatingState opState = OFF;

// Timer variables

byte hourAlarm = 0;
byte minuteAlarm = 0;
byte secondAlarm = 0;


// ************************************************
// LCD Variables, constants, declaration
// ************************************************

NexPage page0    = NexPage(0, 0, "page0");
NexPage page1    = NexPage(1, 0, "page1");
NexPage page2    = NexPage(2, 0, "page2");
NexPage page3    = NexPage(3, 0, "page3");
NexPage page4    = NexPage(4, 0, "page4");
NexPage page5    = NexPage(5, 0, "page5");
NexPage page6    = NexPage(6, 0, "page6");


/*
 * Declare a button object [page id:0,component id:1, component name: "b0"]. 
 */

//Page1
NexText tTemp = NexText(1, 5, "tTemp");
NexText hour_uv = NexText(1, 6, "hour_uv");
NexText min_uv = NexText(1, 4, "min_uv");
NexWaveform s0 = NexWaveform(1, 3, "s0");
NexButton bSettings = NexButton(1, 1, "bSettings");
NexButton bOnOff = NexButton(1, 2, "bOnOff");



//Page2
NexButton bPreSet1   = NexButton(2, 1, "bPreSet1");
NexButton bPreSet2   = NexButton(2, 2, "bPreSet2");
NexButton bPreSet3   = NexButton(2, 3, "bPreSet3");
NexButton bPreSet4   = NexButton(2, 4, "bPreSet4");
NexButton bPreSet5   = NexButton(2, 5, "bPreSet5");
NexButton bPreSet6   = NexButton(2, 6, "bPreSet6");
NexButton bTempSetup   = NexButton(2, 7, "bTempSetup");
NexButton bTimerSetup   = NexButton(2, 8, "bTimerSetup");
NexButton bLEDSetup   = NexButton(2, 9, "bLEDSetup");
NexButton bPIDSetup   = NexButton(2, 10, "bPIDSetup");
NexButton bHomeSet = NexButton(2, 11, "bHomeSet");

//Page3
NexText tTempSetup = NexText(3, 1, "tTempSetup");
NexButton bTempPlus1 = NexButton(3, 2, "bTempPlus1");
NexButton bTempPlus10 = NexButton(3, 4, "bTempPlus10");
NexButton bTempMinus1 = NexButton(3, 3, "bTempMinus1");
NexButton bTempMinus10 = NexButton(3, 5, "bTempMinus10");
NexButton bHomeTemp = NexButton(3, 6, "bHomeTemp");

//Page4
NexButton bOHourPlus1 = NexButton(4, 1, "bOHourPlus1");
NexButton bOHourPlus10 = NexButton(4, 2, "bOHourPlus10");
NexButton bOHourMinus1 = NexButton(4, 3, "bOHourMinus1");
NexButton bOHourMinus10 = NexButton(4, 4, "bOHourMinus10");
NexButton bOMinPlus1 = NexButton(4, 5, "bOMinPlus1");
NexButton bOMinPlus10 = NexButton(4, 6, "bOMinPlus10");
NexButton bOMinMinus1 = NexButton(4, 7, "bOMinMinus1");
NexButton bOMinMinus10 = NexButton(4, 8, "bOMinMinus10");
NexButton bLHourPlus1 = NexButton(4, 9, "bLHourPlus1");
NexButton bLHourPlus10 = NexButton(4, 10, "bLHourPlus10");
NexButton bLHourMinus1 = NexButton(4, 11, "bLHourMinus1");
NexButton bLHourMinus10 = NexButton(4, 12, "bLHourMinus10");
NexButton bLMinPlus1 = NexButton(4, 13, "bLMinPlus1");
NexButton bLMinPlus10 = NexButton(4, 14, "bLMinPlus10");
NexButton bLMinMinus1 = NexButton(4, 15, "bLMinMinus1");
NexButton bLMinMinus10 = NexButton(4, 16, "bLMinMinus10");
NexButton bHomeTimer = NexButton(4, 17, "bHomeTimer");
NexText tOvenHourT = NexText(4, 18, "tOvenHourT");
NexText tOvenMinuteT = NexText(4, 19, "tOvenMinuteT");
NexText tLEDsHourT = NexText(4, 20, "tLEDsHourT");
NexText tLEDsMinuteT = NexText(4, 21, "tLEDsMinuteT");

//Page5
NexButton bLED1 = NexButton(5, 1, "bLED1");
NexButton bLED2 = NexButton(5, 2, "bLED2");
NexButton bLED3 = NexButton(5, 3, "bLED3");
NexButton bLED1plus1 = NexButton(5, 4, "bLED1plus1");
NexButton bLED1plus10 = NexButton(5, 5, "bLED1plus10");
NexButton bLED1minus1 = NexButton(5, 6, "bLED1minus1");
NexButton bLED1minus10 = NexButton(5, 7, "bLED1minus10");
NexButton bLED2plus1 = NexButton(5, 8, "bLED2plus1");
NexButton bLED2plus10 = NexButton(5, 9, "bLED2plus10");
NexButton bLED2minus1 = NexButton(5, 10, "bLED2minus1");
NexButton bLED2minus10 = NexButton(5, 11, "bLED2minus10");
NexButton bLED3plus1 = NexButton(5, 12, "bLED3plus1");
NexButton bLED3plus10 = NexButton(5, 13, "bLED3plus10");
NexButton bLED3minus1 = NexButton(5, 14, "bLED3minus1");
NexButton bLED3minus10 = NexButton(5, 15, "bLED3minus10");
NexButton bHomeLED = NexButton(5, 16, "bHomeLED");
NexText tLED1 = NexText(5, 17, "tLED1");
NexText tLED2 = NexText(5, 18, "tLED2");
NexText tLED3 = NexText(5, 19, "tLED3");

//Page6
NexButton bHomePID = NexButton(6, 1, "bHomePID");



char buffer[50] = {0};

NexTouch *nex_listen_list[] = 
{
    &bSettings, &bOnOff,
    
    &bPreSet1, &bPreSet2, &bPreSet3, &bPreSet4, &bPreSet5, &bPreSet6, &bTempSetup, &bTimerSetup, &bLEDSetup, &bPIDSetup, &bHomeSet,
    
    &bTempPlus1, &bTempPlus10, &bTempMinus1, &bTempMinus10, &bHomeTemp,
    
    &bOHourPlus1, &bOHourPlus10, &bOHourMinus1, &bOHourMinus10, &bOMinPlus1, &bOMinPlus10, &bOMinMinus1, &bOMinMinus10,
    &bLHourPlus1, &bLHourPlus10, &bLHourMinus1, &bLHourMinus10, &bLMinPlus1, &bLMinPlus10, &bLMinMinus1, &bLMinMinus10, &bHomeTimer,
    
    &bLED1, &bLED2, &bLED3,
    &bLED1plus1, &bLED1plus10, &bLED1minus1, &bLED1minus10,
    &bLED2plus1, &bLED2plus10, &bLED2minus1, &bLED2minus10,
    &bLED3plus1, &bLED3plus10, &bLED3minus1, &bLED3minus10,
    &bHomeLED,
    
    &bHomePID,
    NULL
};

//Page1

void bSettingsPopCallback(void *ptr)
{
    page2.show();
    sendCommand("ref 0");
}

void bOnOffPopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bOnOff.getPic(&picNum);
     if(picNum == 4) {
      picNum = 5;

      createLogFile();
      writeHeader();
      
      opState = RUN;
      
      //set alarm
      setDS3231Alarm(minutes_oven, hours_oven);

    } else {
      picNum = 4;

      opState = OFF;
    }
    bOnOff.setPic(picNum);
    sendCommand("ref bOnOff");
}

void createLogFile(){
    // Open/Create the file we're going to log to with current date and time!
    time_t t = now();
    String filename = String(day(t)) + String(month(t)) + String(hour(t)) + String(minute(t)) + ".csv";
    Serial.println(filename); 
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      dataFile = SD.open(filename, FILE_WRITE);
      Serial.println("create new file");
    }
    
    if (! dataFile) {
    Serial.println("couldnt create file");
    // Wait forever since we cant write data
    while (1) ;
    }
}

//Write header to logfile
void writeHeader(){

   dataFile.println(F("Lehrstuhl für Chemie der Kunststoffe"));
   dataFile.println(F("Timer Ofen: "));
   dataFile.println(F("Timer LED: "));
   dataFile.println(F("Tist, Tsoll, UV_LED1, UV_LED2, UV_LED3"));

   dataFile.flush();
}

//End Page1

//Page2

void bPreSet1PopCallback(void *ptr)
{   
  uint32_t picNum = 0;
  bPreSet1.getPic(&picNum);
  if(picNum == 6) {
    picNum = 7;
    turnOffButtons();
    
    } else if(picNum == 7) {
      picNum = 6;
      
    }
    //Serial.println(picNum);

    bPreSet1.setPic(picNum);
    sendCommand("ref 0");
}



void bPreSet2PopCallback(void *ptr)
{
   uint32_t picNum = 0;
  bPreSet2.getPic(&picNum);
  if(picNum == 6) {
      picNum = 7;
      turnOffButtons();
    
    } else if(picNum == 7) {
      picNum = 6;
      
    }
    //Serial.println(picNum);
    bPreSet2.setPic(picNum);
    sendCommand("ref bPreSet2");
}

void bPreSet3PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet3.getPic(&picNum);
  if(picNum == 6) {
      picNum = 7;
      turnOffButtons();
    
    } else if(picNum == 7) {
      picNum = 6;
      
    }
    //Serial.println(picNum);
    bPreSet3.setPic(picNum);
    sendCommand("ref bPreSet3");}

void bPreSet4PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet4.getPic(&picNum);
  if(picNum == 6) {
      picNum = 7;
      turnOffButtons();
    
    } else if(picNum == 7) {
      picNum = 6;
      
    }
    //Serial.println(picNum);
    bPreSet4.setPic(picNum);
    sendCommand("ref bPreSet4");}

void bPreSet5PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet5.getPic(&picNum);
  if(picNum == 6) {
      picNum = 7;
      turnOffButtons();
    
    } else if(picNum == 7) {
      picNum = 6;
      
    }
    //Serial.println(picNum);
    bPreSet5.setPic(picNum);
    sendCommand("ref bPreSet5");}

void bPreSet6PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet6.getPic(&picNum);
  if(picNum == 6) {
      picNum = 7;
      turnOffButtons();
    
    } else if(picNum == 7) {
      picNum = 6;
      
    }
    //Serial.println(picNum);
    bPreSet6.setPic(picNum);
    sendCommand("ref bPreSet6");
}

void turnOffButtons(){
    bPreSet1.setPic(6);
    bPreSet2.setPic(6);
    bPreSet3.setPic(6);
    bPreSet4.setPic(6);
    bPreSet5.setPic(6);
    bPreSet6.setPic(6);

    sendCommand("ref 0");
}

void bTempSetupPopCallback(void *ptr)
{
    page3.show();
    sendCommand("ref 0");
}

void bTimerSetupPopCallback(void *ptr)
{
    page4.show();
    sendCommand("ref 0");
}

void bLEDSetupPopCallback(void *ptr)
{
    page5.show();
    sendCommand("ref 0");
}

void bPIDSetupPopCallback(void *ptr)
{
    page6.show();
    sendCommand("ref 0");
}

void bHomeSetPopCallback(void *ptr)
{
    page1.show();
    sendCommand("ref 0");
}

//End Page2

//Page3

void bTempPlus1PopCallback(void *ptr)
{
    uint16_t len;
    //uint8_t minutes = 0;
    dbSerialPrintln("bTempPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    targetTemp = atoi(buffer);
    targetTemp += 1;

    if (targetTemp > 100)
    {
        targetTemp = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(targetTemp, buffer, 10);
    
    tTempSetup.setText(buffer);
}

void bTempPlus10PopCallback(void *ptr)
{
    uint16_t len;
    //uint8_t minutes = 0;
    dbSerialPrintln("bTempPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    targetTemp = atoi(buffer);
    targetTemp += 10;

    if (targetTemp > 100)
    {
        targetTemp = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(targetTemp, buffer, 10);
    
    tTempSetup.setText(buffer);
}

void bTempMinus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bTempMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    targetTemp = atoi(buffer);
    targetTemp -= 1;

    if (targetTemp < 0)
    {
        targetTemp = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(targetTemp, buffer, 10);
    
    tTempSetup.setText(buffer);
}

void bTempMinus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bTempMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    targetTemp = atoi(buffer);
    targetTemp -= 10;

    if (targetTemp < 0)
    {
        targetTemp = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(targetTemp, buffer, 10);
    
    tTempSetup.setText(buffer);
}

void bHomeTempPopCallback(void *ptr)
{
    page2.show();
    sendCommand("ref 0");
}

//End Page3

//Page4

void bOHourPlus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven += 1;

    if (hours_oven > 99)
    {
        hours_oven = 99;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_oven, buffer, 10);
    
    tOvenHourT.setText(buffer);
}

void bOHourPlus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven += 10;

    if (hours_oven > 99)
    {
        hours_oven = 99;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_oven, buffer, 10);
    
    tOvenHourT.setText(buffer);
}

void bOHourMinus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven -= 1;

    if (hours_oven < 0)
    {
        hours_oven = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_oven, buffer, 10);
    
    tOvenHourT.setText(buffer);
}

void bOHourMinus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven -= 10;

    if (hours_oven < 0)
    {
        hours_oven = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_oven, buffer, 10);
    
    tOvenHourT.setText(buffer);
}

void bOMinPlus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven += 1;

    if (minutes_oven > 60)
    {
        minutes_oven = 60;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_oven, buffer, 10);
    
    tOvenMinuteT.setText(buffer);
}

void bOMinPlus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven += 10;

    if (minutes_oven > 60)
    {
        minutes_oven = 60;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_oven, buffer, 10);
    
    tOvenMinuteT.setText(buffer);
}

void bOMinMinus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven -= 1;

    if (minutes_oven < 0)
    {
        minutes_oven = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_oven, buffer, 10);
    
    tOvenMinuteT.setText(buffer);
}

void bOMinMinus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bOHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven -= 10;

    if (minutes_oven < 0)
    {
        minutes_oven = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_oven, buffer, 10);
    
    tOvenMinuteT.setText(buffer);
}

//Page4 LED Timer

void bLHourPlus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_led = atoi(buffer);
    hours_led += 1;

    if (hours_led > 99)
    {
        hours_led = 99;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_led, buffer, 10);
    
    tLEDsHourT.setText(buffer);
}

void bLHourPlus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_led = atoi(buffer);
    hours_led += 10;

    if (hours_led > 99)
    {
        hours_led = 99;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_led, buffer, 10);
    
    tLEDsHourT.setText(buffer);
}

void bLHourMinus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_led = atoi(buffer);
    hours_led -= 1;

    if (hours_led < 0)
    {
        hours_led = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_led, buffer, 10);
    
    tLEDsHourT.setText(buffer);
}

void bLHourMinus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_led = atoi(buffer);
    hours_led -= 10;

    if (hours_led < 0)
    {
        hours_led = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(hours_led, buffer, 10);
    
    tLEDsHourT.setText(buffer);
}

void bLMinPlus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_led = atoi(buffer);
    minutes_led += 1;

    if (minutes_led > 60)
    {
        minutes_led = 60;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_led, buffer, 10);
    
    tLEDsMinuteT.setText(buffer);
}

void bLMinPlus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_led = atoi(buffer);
    minutes_led += 10;

    if (minutes_led > 60)
    {
        minutes_led = 60;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_led, buffer, 10);
    
    tLEDsMinuteT.setText(buffer);
}

void bLMinMinus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_led = atoi(buffer);
    minutes_led -= 1;

    if (minutes_led < 0)
    {
        minutes_led = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_led, buffer, 10);
    
    tLEDsMinuteT.setText(buffer);
}

void bLMinMinus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_led = atoi(buffer);
    minutes_led -= 10;

    if (minutes_led < 0)
    {
        minutes_led = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(minutes_led, buffer, 10);
    
    tLEDsMinuteT.setText(buffer);
}

void bHomeTimerPopCallback(void *ptr)
{
    page2.show();
    sendCommand("ref 0");
}

//End Page4

//Page5

//Page5
void bLED1PopCallback(void *ptr)
{ 
  uint32_t picNum = 0;
  bLED1.getPic(&picNum);
  if(picNum == 10) {
      picNum = 11;

      b0State = true;
      
    } else if(picNum == 11) {
      picNum = 10;

      b0State = false;

    }
    //Serial.println(picNum);
    bLED1.setPic(picNum);
    sendCommand("ref bLED1");
}

void bLED1plus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED1plus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity += 1;

    if (LED1_intensity > 100)
    {
        LED1_intensity = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED1_intensity, buffer, 10);
    
    tLED1.setText(buffer);
}

void bLED1plus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED1plus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity += 10;

    if (LED1_intensity > 100)
    {
        LED1_intensity = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED1_intensity, buffer, 10);
    
    tLED1.setText(buffer);
}

void bLED1minus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED1minus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity -= 1;

    if (LED1_intensity < 0)
    {
        LED1_intensity = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED1_intensity, buffer, 10);
    
    tLED1.setText(buffer);
}

void bLED1minus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED1minus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity -= 10;

    if (LED1_intensity < 0)
    {
        LED1_intensity = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED1_intensity, buffer, 10);
    
    tLED1.setText(buffer);
}

void bLED2PopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bLED2.getPic(&picNum);
     if(picNum == 10) {
      picNum = 11;

      b1State = true;

    } else if(picNum == 11) {
      picNum = 10;

      b1State = false;

    }
    //Serial.println(picNum);
    bLED2.setPic(picNum);
    sendCommand("ref bLED2");
}

void bLED2plus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED2plus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity += 1;

    if (LED2_intensity > 100)
    {
        LED2_intensity = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED2_intensity, buffer, 10);
    
    tLED2.setText(buffer);
}

void bLED2plus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED2plus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity += 10;

    if (LED2_intensity > 100)
    {
        LED2_intensity = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED2_intensity, buffer, 10);
    
    tLED2.setText(buffer);
}

void bLED2minus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED2minus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity -= 1;

    if (LED2_intensity <= 0)
    {
        LED2_intensity = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED2_intensity, buffer, 10);
    
    tLED2.setText(buffer);
}

void bLED2minus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED2minus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity -= 10;

    if (LED2_intensity < 0)
    {
        LED2_intensity = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED2_intensity, buffer, 10);
    
    tLED2.setText(buffer);
}

void bLED3PopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bLED3.getPic(&picNum);
     if(picNum == 10) {
      picNum = 11;

      b2State = true;

    } else if(picNum == 11) {
      picNum = 10;

      b2State = false;

    }
    //Serial.println(picNum);
    bLED3.setPic(picNum);
    sendCommand("ref bLED3");
}

void bLED3plus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED3plus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity += 1;

    if (LED3_intensity > 100)
    {
        LED3_intensity = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED3_intensity, buffer, 10);
    
    tLED3.setText(buffer);
}

void bLED3plus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED3plus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity += 10;

    if (LED3_intensity > 100)
    {
        LED3_intensity = 100;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED3_intensity, buffer, 10);
    
    tLED3.setText(buffer);
}

void bLED3minus1PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED3minus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity -= 1;

    if (LED3_intensity < 0)
    {
        LED3_intensity = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED3_intensity, buffer, 10);
    
    tLED3.setText(buffer);
}

void bLED3minus10PopCallback(void *ptr)
{
    uint16_t len;
    dbSerialPrintln("bLED3minus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity -= 10;

    if (LED3_intensity < 0)
    {
        LED3_intensity = 0;
    }
    
    memset(buffer, 0, sizeof(buffer));
    itoa(LED3_intensity, buffer, 10);
    
    tLED3.setText(buffer);
}

void bHomeLEDPopCallback(void *ptr)
{
    page2.show();
    sendCommand("ref 0");
}


//End Page5

//Page6
void bHomePIDPopCallback(void *ptr)
{
    page2.show();
    sendCommand("ref 0");
}
//End Page6

volatile boolean alarmIsrWasCalled = false;

void alarmIsr(){
    alarmIsrWasCalled = true;
}

void setup(void)
{
    nexInit();

    /* Register the pop event callback function of the current button component. */
    //Page1
    bOnOff.attachPop(bOnOffPopCallback, &bOnOff);
    bSettings.attachPop(bSettingsPopCallback, &bSettings);

    //Page2
    bPreSet1.attachPop(bPreSet1PopCallback, &bPreSet1);
    bPreSet2.attachPop(bPreSet2PopCallback, &bPreSet2);
    bPreSet3.attachPop(bPreSet3PopCallback, &bPreSet3);
    bPreSet4.attachPop(bPreSet4PopCallback, &bPreSet4);
    bPreSet5.attachPop(bPreSet5PopCallback, &bPreSet5);
    bPreSet6.attachPop(bPreSet6PopCallback, &bPreSet6);
    bTempSetup.attachPop(bTempSetupPopCallback, &bTempSetup);
    bTimerSetup.attachPop(bTimerSetupPopCallback, &bTimerSetup);
    bLEDSetup.attachPop(bLEDSetupPopCallback, &bLEDSetup);
    bPIDSetup.attachPop(bPIDSetupPopCallback, &bPIDSetup);
    bHomeSet.attachPop(bHomeSetPopCallback, &bHomeSet);

    //Page3
    bTempPlus1.attachPop(bTempPlus1PopCallback, &bTempPlus1);
    bTempPlus10.attachPop(bTempPlus10PopCallback, &bTempPlus10);
    bTempMinus1.attachPop(bTempMinus1PopCallback, &bTempMinus1);
    bTempMinus10.attachPop(bTempMinus10PopCallback, &bTempMinus10);
    bHomeTemp.attachPop(bHomeTempPopCallback, &bHomeTemp);

    //Page4
    bOHourPlus1.attachPop(bOHourPlus1PopCallback, &bOHourPlus1);
    bOHourPlus10.attachPop(bOHourPlus10PopCallback, &bOHourPlus10);
    bOHourMinus1.attachPop(bOHourMinus1PopCallback, &bOHourMinus1);
    bOHourMinus10.attachPop(bOHourMinus10PopCallback, &bOHourMinus10);
    bOMinPlus1.attachPop(bOMinPlus1PopCallback, &bOMinPlus1);
    bOMinPlus10.attachPop(bOMinPlus10PopCallback, &bOMinPlus10);
    bOMinMinus1.attachPop(bOMinMinus1PopCallback, &bOMinMinus1);
    bOMinMinus10.attachPop(bOMinMinus10PopCallback, &bOMinMinus10);
    bLHourPlus1.attachPop(bLHourPlus1PopCallback, &bLHourPlus1);
    bLHourPlus10.attachPop(bLHourPlus10PopCallback, &bLHourPlus10);
    bLHourMinus1.attachPop(bLHourMinus1PopCallback, &bLHourMinus1);
    bLHourMinus10.attachPop(bLHourMinus10PopCallback, &bLHourMinus10);
    bLMinPlus1.attachPop(bLMinPlus1PopCallback, &bLMinPlus1);
    bLMinPlus10.attachPop(bLMinPlus10PopCallback, &bLMinPlus10);
    bLMinMinus1.attachPop(bLMinMinus1PopCallback, &bLMinMinus1);
    bLMinMinus10.attachPop(bLMinMinus10PopCallback, &bLMinMinus10);
    bHomeTimer.attachPop(bHomeTimerPopCallback, &bHomeTimer);

    //Page5
    bLED1.attachPop(bLED1PopCallback, &bLED1);
    bLED2.attachPop(bLED2PopCallback, &bLED2);
    bLED3.attachPop(bLED3PopCallback, &bLED3);
    bLED1plus1.attachPop(bLED1plus1PopCallback, &bLED1plus1);
    bLED1plus10.attachPop(bLED1plus10PopCallback, &bLED1plus10);
    bLED1minus1.attachPop(bLED1minus1PopCallback, &bLED1minus1);
    bLED1minus10.attachPop(bLED1minus10PopCallback, &bLED1minus10);
    bLED2plus1.attachPop(bLED2plus1PopCallback, &bLED2plus1);
    bLED2plus10.attachPop(bLED2plus10PopCallback, &bLED2plus10);
    bLED2minus1.attachPop(bLED2minus1PopCallback, &bLED2minus1);
    bLED2minus10.attachPop(bLED2minus10PopCallback, &bLED2minus10);
    bLED3plus1.attachPop(bLED3plus1PopCallback, &bLED3plus1);
    bLED3plus10.attachPop(bLED3plus10PopCallback, &bLED3plus10);
    bLED3minus1.attachPop(bLED3minus1PopCallback, &bLED3minus1);
    bLED3minus10.attachPop(bLED3minus10PopCallback, &bLED3minus10);
    bHomeLED.attachPop(bHomeLEDPopCallback, &bHomeLED);

    //Page6
    bHomePID.attachPop(bHomePIDPopCallback, &bHomePID);

    dbSerialPrintln("setup done");
    delay(1000);
    page1.show();

    pinMode(RelayPin, OUTPUT);    // Output mode to drive relay
    digitalWrite(RelayPin, LOW);  // make sure it is off to start

    // Initialize LED Control
    pinMode(b0Pin, OUTPUT);   // Output mode to drive LED
    digitalWrite(b0Pin, LOW);  // make sure it is off to start
    pinMode(b1Pin, OUTPUT);
    digitalWrite(b1Pin, LOW);
    pinMode(b2Pin, OUTPUT);
    digitalWrite(b2Pin, LOW);

    //Attach an interrupt on the falling of the SQW pin.
    digitalWrite(SQW_PIN, HIGH);
    pinMode(SQW_PIN, INPUT_PULLUP);
    attachInterrupt(INT0, alarmIsr, FALLING);

    // the function to get the time from the RTC
    setSyncProvider(RTC.get);   
    if(timeStatus() != timeSet) 
        Serial.println("Unable to sync with the RTC");
    else
        Serial.println("RTC has set the system time");  

    //Disable the default square wave of the SQW pin.
    RTC.squareWave(SQWAVE_NONE);

    //Initializing SD Card
    Serial.print("Initializing SD card...");
    // make sure that the default chip select pin is set to
    // output, even if you don't use it:
    pinMode(SS, OUTPUT);
    // see if the card is present and can be initialized:
    
    if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1) ;
    }
    Serial.println("card initialized.");
  
    

    //  Initialize Relay Control

    // Start up the Thermocouple Amplifier MAX31855

    // Initialize the PID and related variables
    LoadParameters();
    myPID.SetTunings(Kp,Ki,Kd);
 
    myPID.SetSampleTime(1000);
    myPID.SetOutputLimits(0, WindowSize);


    // Run timer2 interrupt every 15 ms 
    TCCR2A = 0;
    TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20;
 
    //Timer2 Overflow Interrupt Enable
    TIMSK2 |= 1<<TOIE2;
}
 
// ************************************************
// Timer Interrupt Handler
// ************************************************
SIGNAL(TIMER2_OVF_vect) 
{
  if (opState == OFF)
  {
    digitalWrite(RelayPin, LOW);  // make sure relay is off
  }
  else
  {
    DriveOutput();
  }
}

// ************************************************
// Main Control Loop
//
// All state changes pass through here
// ************************************************
void loop(void)
{
    nexLoop(nex_listen_list);

    if (alarmIsrWasCalled == true){
      opState = OFF;
      alarmIsrWasCalled = false;
    }

   switch (opState)
   {
   case OFF:
      Off();
      break;
  // case SETP:
    //  Tune_Sp();
    //  break;
    case RUN:
      Run();
      break;
   /*case TUNE_P:
      TuneP();
      break;
   case TUNE_I:
      TuneI();
      break;
   case TUNE_D:
      TuneD();
      break;*/
   }
}

// ************************************************
// Initial State
// ************************************************
void Off()
{
   myPID.SetMode(MANUAL);
   digitalWrite(RelayPin, LOW);  // make sure it is off
   digitalWrite(b0Pin, LOW); // make sure UV LEDs are off
   digitalWrite(b1Pin, LOW);
   digitalWrite(b2Pin, LOW);

   
   //turn the PID on
   myPID.SetMode(AUTOMATIC);
   windowStartTime = millis();
  // opState = RUN; // start control

     DoControl();
  
  // periodically do
   if (millis() - lastLogTime > logInterval)  
   {  
      lastLogTime = millis();
      
      char temp[10] = {0};
      utoa(Input, temp, 10);
      tTemp.setText(temp);
   }
  
}
/*
void Tune_Sp()
{
   while(true)
   {
      DoControl();
   }
}


// ************************************************
// Proportional Tuning State
// ************************************************
void TuneP()
{

 
   while(true)
   {
      DoControl();
   }
}
 
// ************************************************
// Integral Tuning State
// ************************************************
void TuneI()
{
   while(true)
   {
      DoControl();
   }
}
 
// ************************************************
// Derivative Tuning State
// ************************************************
void TuneD()
{
   while(true)
   {
      DoControl();
   }
}
*/


// ************************************************
// RUN Control State
// ************************************************
void Run()
{ 

   SaveParameters();
   myPID.SetTunings(Kp,Ki,Kd);
 
   DoControl();
      
   float pct = map(Output, 0, WindowSize, 0, 1000);
     
   // periodically do
   if (millis() - lastLogTime > logInterval)  
   {  
      lastLogTime = millis();
      s0.addValue(0, inputOffset + Input);
      s0.addValue(1, targetTemp);

      //refresh countdown
      byte calcMinutes = 0;
      byte calcHours = 0;
      if(minuteAlarm < minute()) {
        calcMinutes = 60 - (minute() - minuteAlarm);
        calcHours = hourAlarm - hour() - 1;
      } else {
        calcMinutes = minuteAlarm - minute();
        calcHours = hourAlarm - hour();
      }

      
     
      char temp[10] = {0};
      utoa(calcHours, temp, 10);
      hour_uv.setText(temp);
      
      temp[10] = {0};
      utoa(calcMinutes, temp, 10);
      min_uv.setText(temp);
      
      temp[10] = {0};
      utoa(Input, temp, 10);
      tTemp.setText(temp);
      //Serial.print(Input);
      //Serial.print(",");
      //Serial.println(Output);
   }
  
  return;
}

// ************************************************
// Execute the control loop
// ************************************************
void DoControl()
{
  Input = thermocouple.readCelsius();
  
  if (tuning) // run the auto-tuner
  {
     if (aTune.Runtime()) // returns 'true' when done
     {
        FinishAutoTune();
     }
  }
  else // Execute control algorithm
  {
     myPID.Compute();
  }
  
  // Time Proportional relay state is updated regularly via timer interrupt.
  onTime = Output; 
}
 
// ************************************************
// Called by ISR every 15ms to drive the output
// ************************************************
void DriveOutput()
{  
  long now = millis();
  // Set the output
  // "on time" is proportional to the PID output
  if(now - windowStartTime>WindowSize)
  { //time to shift the Relay Window
     windowStartTime += WindowSize;
  }
  if((onTime > 100) && (onTime > (now - windowStartTime)))
  {
     digitalWrite(RelayPin, HIGH);
  }
  else
  {
     digitalWrite(RelayPin, LOW);
  }
}
 
// ************************************************
// Start the Auto-Tuning cycle
// ************************************************
 
void StartAutoTune()
{
   // REmember the mode we were in
   ATuneModeRemember = myPID.GetMode();
 
   // set up the auto-tune parameters
   aTune.SetNoiseBand(aTuneNoise);
   aTune.SetOutputStep(aTuneStep);
   aTune.SetLookbackSec((int)aTuneLookBack);
   tuning = true;
}
 
// ************************************************
// Return to normal control
// ************************************************
void FinishAutoTune()
{
   tuning = false;
 
   // Extract the auto-tune calculated parameters
   Kp = aTune.GetKp();
   Ki = aTune.GetKi();
   Kd = aTune.GetKd();
 
   // Re-tune the PID and revert to normal control mode
   myPID.SetTunings(Kp,Ki,Kd);
   myPID.SetMode(ATuneModeRemember);
   
   // Persist any changed parameters to EEPROM
   SaveParameters();
}
 
// ************************************************
// Save any parameter changes to EEPROM
// ************************************************
void SaveParameters()
{
   if (Setpoint != EEPROM_readDouble(SpAddress))
   {
      EEPROM_writeDouble(SpAddress, Setpoint);
   }
   if (Kp != EEPROM_readDouble(KpAddress))
   {
      EEPROM_writeDouble(KpAddress, Kp);
   }
   if (Ki != EEPROM_readDouble(KiAddress))
   {
      EEPROM_writeDouble(KiAddress, Ki);
   }
   if (Kd != EEPROM_readDouble(KdAddress))
   {
      EEPROM_writeDouble(KdAddress, Kd);
   }
}
 
// ************************************************
// Load parameters from EEPROM
// ************************************************
void LoadParameters()
{
  // Load from EEPROM
   Setpoint = EEPROM_readDouble(SpAddress);
   Kp = EEPROM_readDouble(KpAddress);
   Ki = EEPROM_readDouble(KiAddress);
   Kd = EEPROM_readDouble(KdAddress);
   
   // Use defaults if EEPROM values are invalid
   if (isnan(Setpoint))
   {
     Setpoint = 60;
   }
   if (isnan(Kp))
   {
     Kp = 850;
   }
   if (isnan(Ki))
   {
     Ki = 0.5;
   }
   if (isnan(Kd))
   {
     Kd = 0.1;
   }  
}
 
 
// ************************************************
// Write floating point values to EEPROM
// ************************************************
void EEPROM_writeDouble(int address, double value)
{
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
   {
      EEPROM.write(address++, *p++);
   }
}
 
// ************************************************
// Read floating point values from EEPROM
// ************************************************
double EEPROM_readDouble(int address)
{
   double value = 0.0;
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
   {
      *p++ = EEPROM.read(address++);
   }
   return value;
}


// ************************************************
// Set DS3231 Alarm
// ************************************************
void setDS3231Alarm(byte minutes, byte hours) {

  tmElements_t tm;
  RTC.read(tm);

  hourAlarm = hour() + hours;
  minuteAlarm = minute() + minutes;
  secondAlarm = second();

  if(minuteAlarm >= 60){
    minuteAlarm = minuteAlarm - 60;
    hourAlarm += 1;
  }
  if(hourAlarm >= 24) {
    hourAlarm = hourAlarm - 24;
  }
  
  RTC.setAlarm(ALM1_MATCH_HOURS, secondAlarm, minuteAlarm, hourAlarm, 1);
  RTC.alarm(ALARM_1);
  RTC.alarmInterrupt(ALARM_1, true);

}

/*
String createString(){
  return;
}
*/
void writeDataToSD(String dataString){
    
  dataFile.println(dataString);

  // print to the serial port too:
  Serial.println(dataString);
  
  // The following line will 'save' the file to the SD card after every
  // line of data - this will use more power and slow down how much data
  // you can read but it's safer! 
  // If you want to speed up the system, remove the call to flush() and it
  // will save the file only every 512 bytes - every time a sector on the 
  // SD card is filled with data.
  dataFile.flush();
}

