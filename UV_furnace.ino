/*----------------------------------------------------------------------*
 * UV furnace                                                           *
 *                                                                      *
 * Copyright 2016 Thomas Rockenbauer, Fablab Leoben,                    *
 * rockenbauer.thomas@gmail.com                                         *
 *                                                                      *
 * http://www.fablab-leoben.at                                          *
 *                                                                      *
 * http://www.github.com/fablab-leoben                                  *
 *                                                                      *
 * This program is free software; you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation; either version 2 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful, but  *
 * WITHOUT ANY WARRANTY; without even the implied warranty of           *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU     *
 * General Public License for more details.                             *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program; if not, write to the Free Software          *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,                *
 * MA 02111-1307 USA                                                    *
 *----------------------------------------------------------------------*/

#include <BlynkSimpleEthernet2.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include "access.h"
#include <PID_AutoTune_v0.h>
#include <PID_v1.h>
//Library for DS3231 clock
#include <DS3232RTC.h>
//Library for LCD touch screen
#include <Nextion.h>
#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <SPI.h>
#include <SD.h>
// So we can save and retrieve settings
#include <EEPROM.h>
#include <Adafruit_MAX31855.h>
#include <elapsedMillis.h>
#include <FiniteStateMachine.h>
#include <SDConfigFile.h>

/************************************************
  Debug
************************************************/
#define DebugStream    Serial
#define UV_FURNACE_DEBUG

#ifdef UV_FURNACE_DEBUG
// need to do some debugging...
  #define DEBUG_PRINT(...)    DebugStream.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...)    DebugStream.println(__VA_ARGS__)
#endif

#define APP_NAME "UV furnace"
const char VERSION[] = "Version 0.1";

typedef struct myBoolStruct
{
   uint8_t preset1: 1;
   uint8_t preset2: 1;
   uint8_t preset3: 1;
   uint8_t preset4: 1;
   uint8_t preset5: 1;
   uint8_t preset6: 1;
   uint8_t preheat: 1;

   uint8_t bLED1State: 1;
   uint8_t bLED2State: 1;
   uint8_t bLED3State: 1;

   uint8_t didReadConfig: 1;
}; 
myBoolStruct myBoolean;

//compatibility with Arduino IDE 1.6.9
void dummy(){}

/*******************************************************************************
 initialize FSM states with proper enter, update and exit functions
*******************************************************************************/
State dummyState = State(dummy);
State initState = State( initEnterFunction, initUpdateFunction, initExitFunction );
State idleState = State( idleEnterFunction, idleUpdateFunction, idleExitFunction );
State settingsState = State( settingsEnterFunction, settingsUpdateFunction, settingsExitFunction );
State setLEDs = State( setLEDsEnterFunction, setLEDsUpdateFunction, setLEDsExitFunction );
State setTemp = State( setTempEnterFunction, setTempUpdateFunction, setTempExitFunction );
State setTimer = State( setTimerEnterFunction, setTimerUpdateFunction, setTimerExitFunction );
State setPID = State( setPIDEnterFunction, setPIDUpdateFunction, setPIDExitFunction ); 
State runState = State( runEnterFunction, runUpdateFunction, runExitFunction );
State errorState = State( errorEnterFunction, errorUpdateFunction, errorExitFunction );
State offState = State ( offEnterFunction, offUpdateFunction, offExitFunction );

//initialize state machine, start in state: Idle
FSM uvFurnaceStateMachine = FSM(initState);

//milliseconds for the init cycle, so everything gets stabilized
#define INIT_TIMEOUT 5000
elapsedMillis initTimer;

// ************************************************
// Pin definitions
// ************************************************

// LEDs
#define LED1 5
#define LED2 6
#define LED3 7

// Output relay
#define RelayPin 32

// Reed switch
#define reedSwitch 19

// ON/OFF Button LED
#define onOffButton 12

/************************************************
 LED variables
************************************************/
byte LED1_intensity = 0;
byte LED2_intensity = 0;
byte LED3_intensity = 0;

byte LED1_intens = 0;
byte LED2_intens = 0;
byte LED3_intens = 0;

/************************************************
Config files
************************************************/
const char CONFIG_preset1[] = "preset1.cfg";
const char CONFIG_preset2[] = "preset2.cfg";
const char CONFIG_preset3[] = "preset3.cfg";
const char CONFIG_preset4[] = "preset4.cfg";
const char CONFIG_preset5[] = "preset5.cfg";
const char CONFIG_preset6[] = "preset6.cfg";

boolean readConfiguration();

/************************************************
Ethernet
************************************************/
#define W5200_CS  10

const unsigned int localPort = 8888;       // local port to listen for UDP packets
char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

/************************************************
 PID Variables and constants
************************************************/

//Define Variables we'll be connecting to
double Setpoint;
double Input;
double Output;

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
const int WindowSize = 10000; 
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

/************************************************
 Timer Variables
************************************************/
byte hourAlarm = 0;
byte minuteAlarm = 0;
byte secondAlarm = 0;

/************************************************
 time settings variables for heating and leds
************************************************/
byte hours_oven = 0;
byte minutes_oven = 0;

byte hours_LED = 0;
byte minutes_LED = 0;

/************************************************
 chart definitions for Nextion 4,3" display
************************************************/
#define inputOffset 0

/*******************************************************************************
 MAX31855 Thermocouple Amplifier
 Creating a thermocouple instance with software SPI on any three
 digital IO pins.
*******************************************************************************/
//#define DO   22
//#define CS   23
//#define CLK  24
#define MAX31855_SAMPLE_INTERVAL   1000    // Sample room temperature every 5 seconds
//Adafruit_MAX31855 thermocouple(CLK, CS, DO);
#define cs_MAX31855   47
Adafruit_MAX31855 thermocouple(cs_MAX31855);
elapsedMillis MAX31855SampleInterval;
float currentTemperature;
float lastTemperature;

/*******************************************************************************
 DS3231 
*******************************************************************************/
#define SQW_PIN 2
#define DS3231_TEMP_INTERVAL   2000
elapsedMillis DS3231TempInterval;

/*******************************************************************************
 Countdown
*******************************************************************************/
#define COUNTDOWN_UPDATE_INTERVAL   1000
elapsedMillis CountdownUpdateInterval;

/*******************************************************************************
 Power LED
*******************************************************************************/
#define POWERLED_BLINK_INTERVAL   500
elapsedMillis POWERLEDBlinkInterval;
boolean onOffState = HIGH;

unsigned long fadeTime = 0;
byte fadeValue;
int periode = 2000;

/*******************************************************************************
 Graph
*******************************************************************************/
#define GRAPH_UPDATE_INTERVAL   5000
elapsedMillis GraphUpdateInterval;

/*******************************************************************************
 InfluxDB
*******************************************************************************/
#define INFLUXDB_UPDATE_INTERVAL   5000
elapsedMillis InfluxdbUpdateInterval;

/*******************************************************************************
 Blynk
 Here you decide if you want to use Blynk or not
 Your blynk token goes in another file to avoid sharing it by mistake
*******************************************************************************/
#define USE_BLYNK "yes"
#define BLYNK_INTERVAL   10000
elapsedMillis BlynkInterval;

/*******************************************************************************
 SD-Card
*******************************************************************************/
const int SDCARD_CS = 4;
File dataFile;
#define SD_CARD_SAMPLE_INTERVAL   1000
elapsedMillis sdCard;

/*******************************************************************************
 Nextion 4,3" LCD touch display
 LCD Variables, constants, declaration
*******************************************************************************/
NexPage page0    = NexPage(0, 0, "page0");
NexPage page1    = NexPage(1, 0, "page1");
NexPage page2    = NexPage(2, 0, "page2");
NexPage page3    = NexPage(3, 0, "page3");
NexPage page4    = NexPage(4, 0, "page4");
NexPage page5    = NexPage(5, 0, "page5");
NexPage page6    = NexPage(6, 0, "page6");
NexPage page7    = NexPage(7, 0, "page7");
NexPage page8    = NexPage(8, 0, "page8");

/*
 * Declare a button object [page id:0,component id:1, component name: "b0"]. 
 */

//Page0
NexText tVersion = NexText(0, 1, "tVersion");

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
NexButton bCredits = NexButton(2, 12, "bCredits");

//Page3
NexText tTempSetup = NexText(3, 1, "tTempSetup");
NexButton bTempPlus1 = NexButton(3, 2, "bTempPlus1");
NexButton bTempPlus10 = NexButton(3, 4, "bTempPlus10");
NexButton bTempMinus1 = NexButton(3, 3, "bTempMinus1");
NexButton bTempMinus10 = NexButton(3, 5, "bTempMinus10");
NexButton bHomeTemp = NexButton(3, 6, "bHomeTemp");
NexButton bPreheat = NexButton(3, 7 , "bPreheat");

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
NexButton bAutotune = NexButton(6, 2, "bAutotune");

//Page7
NexButton bReset = NexButton(7, 1, "bReset");

//Page8
NexButton bHomeCredits = NexButton(8, 1, "bHomeCredits");

char buffer[5] = {0};

NexTouch *nex_listen_list[] = 
{
    &tVersion,
    
    &bSettings, &bOnOff,
    
    &bPreSet1, &bPreSet2, &bPreSet3, &bPreSet4, &bPreSet5, &bPreSet6, &bTempSetup, &bTimerSetup, &bLEDSetup, &bPIDSetup, &bHomeSet, &bCredits,
    
    &bTempPlus1, &bTempPlus10, &bTempMinus1, &bTempMinus10, &bHomeTemp, &bPreheat,
    
    &bOHourPlus1, &bOHourPlus10, &bOHourMinus1, &bOHourMinus10, &bOMinPlus1, &bOMinPlus10, &bOMinMinus1, &bOMinMinus10,
    &bLHourPlus1, &bLHourPlus10, &bLHourMinus1, &bLHourMinus10, &bLMinPlus1, &bLMinPlus10, &bLMinMinus1, &bLMinMinus10, &bHomeTimer,
    
    &bLED1, &bLED2, &bLED3,
    &bLED1plus1, &bLED1plus10, &bLED1minus1, &bLED1minus10,
    &bLED2plus1, &bLED2plus10, &bLED2minus1, &bLED2minus10,
    &bLED3plus1, &bLED3plus10, &bLED3minus1, &bLED3minus10,
    &bHomeLED,
    
    &bHomePID, &bAutotune,

    &bReset,

    &bHomeCredits,
    NULL
};

//Page1
void bSettingsPopCallback(void *ptr)
{ 
  DEBUG_PRINTLN(F("transition to settings")); 
  uvFurnaceStateMachine.transitionTo(settingsState);
}

void bOnOffPopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bOnOff.getPic(&picNum);
     if(picNum == 1) {
      picNum = 2;

      uvFurnaceStateMachine.transitionTo(runState);

    } else {
      picNum = 1;

      //controlLEDs(0, 0, 0);
      uvFurnaceStateMachine.transitionTo(offState);
    }
    bOnOff.setPic(picNum);
    sendCommand("ref bOnOff");
}

//End Page1

//Page2
void bPreSet1PopCallback(void *ptr)
{   
  uint32_t picNum = 0;
  bPreSet1.getPic(&picNum);
  if(picNum == 3) {
    picNum = 4;
    turnOffPresetButtons();
    myBoolean.didReadConfig = readConfiguration(CONFIG_preset1);
    myBoolean.preset1 = 1;

    } else if(picNum == 4) {
      picNum = 3;
      myBoolean.preset1 = 0;

    }
    //Serial.println(picNum);

    bPreSet1.setPic(picNum);
    sendCommand("ref 0");
}

void bPreSet2PopCallback(void *ptr)
{
   uint32_t picNum = 0;
  bPreSet2.getPic(&picNum);
  if(picNum == 3) {
      picNum = 4;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset2);
      myBoolean.preset2 = 1;

    } else if(picNum == 4) {
      picNum = 3;
      myBoolean.preset2 = 0;

    }
    //Serial.println(picNum);
    bPreSet2.setPic(picNum);
    sendCommand("ref bPreSet2");
}

void bPreSet3PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet3.getPic(&picNum);
  if(picNum == 3) {
      picNum = 4;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset3);
      myBoolean.preset3 = 1;

    } else if(picNum == 4) {
      picNum = 3;
      myBoolean.preset3 = 0;
  
    }
    //Serial.println(picNum);
    bPreSet3.setPic(picNum);
    sendCommand("ref bPreSet3");}

void bPreSet4PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet4.getPic(&picNum);
  if(picNum == 3) {
      picNum = 4;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset4);
      myBoolean.preset4 = 1;
   
    } else if(picNum == 4) {
      picNum = 3;
      myBoolean.preset4 = 0;
         
    }
    //Serial.println(picNum);
    bPreSet4.setPic(picNum);
    sendCommand("ref bPreSet4");}

void bPreSet5PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet5.getPic(&picNum);
  if(picNum == 3) {
      picNum = 4;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset5);
      myBoolean.preset5 = 1;


    } else if(picNum == 4) {
      picNum = 3;
      myBoolean.preset5 = 0; 
    }
    //Serial.println(picNum);
    bPreSet5.setPic(picNum);
    sendCommand("ref bPreSet5");}

void bPreSet6PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet6.getPic(&picNum);
  if(picNum == 3) {
      picNum = 4;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset6);
      myBoolean.preset6 = 1;

    } else if(picNum == 4) {
      picNum = 3;
      myBoolean.preset6 = 0;
      
    }
    //Serial.println(picNum);
    bPreSet6.setPic(picNum);
    sendCommand("ref bPreSet6");
}

void turnOffPresetButtons(){
    bPreSet1.setPic(3);
    bPreSet2.setPic(3);
    bPreSet3.setPic(3);
    bPreSet4.setPic(3);
    bPreSet5.setPic(3);
    bPreSet6.setPic(3);
    myBoolean.preset1 = 0;
    myBoolean.preset2 = 0;
    myBoolean.preset3 = 0;
    myBoolean.preset4 = 0;
    myBoolean.preset5 = 0;
    myBoolean.preset6 = 0;

    sendCommand("ref 0");
}

void bTempSetupPopCallback(void *ptr)
{
    uvFurnaceStateMachine.transitionTo(setTemp);
}

void bTimerSetupPopCallback(void *ptr)
{   
    uvFurnaceStateMachine.transitionTo(setTimer);
}

void bLEDSetupPopCallback(void *ptr)
{
    uvFurnaceStateMachine.transitionTo(setLEDs);
}

void bPIDSetupPopCallback(void *ptr)
{
    uvFurnaceStateMachine.transitionTo(setPID);
}

void bHomeSetPopCallback(void *ptr)
{
    uvFurnaceStateMachine.transitionTo(offState);    
}

void bCreditsPopCallback(void *ptr)
{ 
  page8.show();
}

//End Page2

//Page3
void bTempPlus1PopCallback(void *ptr)
{
    dbSerialPrintln("bTempPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    Setpoint = atoi(buffer);
    Setpoint += 1;

    if (Setpoint > 100)
    {
        Setpoint = 100;
    }

    tTempSetup.setText(intToChar(Setpoint));
}

char* intToChar(int variable){
    memset(buffer, 0, sizeof(buffer));
    itoa(variable, buffer, 10);
    
    return buffer;
}

void bTempPlus10PopCallback(void *ptr)
{
    dbSerialPrintln("bTempPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    Setpoint = atoi(buffer);
    Setpoint += 10;

    if (Setpoint > 100)
    {
        Setpoint = 100;
    }
    
   tTempSetup.setText(intToChar(Setpoint));
}

void bTempMinus1PopCallback(void *ptr)
{
    dbSerialPrintln("bTempMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    Setpoint = atoi(buffer);
    Setpoint -= 1;

    if (Setpoint < 0)
    {
        Setpoint = 0;
    }
   
    tTempSetup.setText(intToChar(Setpoint));
}

void bTempMinus10PopCallback(void *ptr)
{
    dbSerialPrintln("bTempMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tTempSetup.getText(buffer, sizeof(buffer));

    Setpoint = atoi(buffer);
    Setpoint -= 10;

    if (Setpoint < 0)
    {
        Setpoint = 0;
    }
    
    tTempSetup.setText(intToChar(Setpoint));
}

void bHomeTempPopCallback(void *ptr)
{
    uvFurnaceStateMachine.transitionTo(settingsState);   
}

void bPreheatPopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bPreheat.getPic(&picNum);
    DEBUG_PRINTLN(picNum);
    if(picNum == 5) {
      picNum = 7;

      myBoolean.preheat = 1;
      
    } else if(picNum == 7) {
      picNum = 5;

      myBoolean.preheat = 0;
    }
    DEBUG_PRINTLN(myBoolean.preheat);
    bPreheat.setPic(picNum);
    sendCommand("ref bPreheat");
}
//End Page3

//Page4
void bOHourPlus1PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven += 1;

    if (hours_oven > 99)
    {
        hours_oven = 99;
    }

    tOvenHourT.setText(intToChar(hours_oven));
}

void bOHourPlus10PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven += 10;

    if (hours_oven > 99)
    {
        hours_oven = 99;
    }
    
    tOvenHourT.setText(intToChar(hours_oven));
}

void bOHourMinus1PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven -= 1;

    if (hours_oven < 0)
    {
        hours_oven = 0;
    }

    tOvenHourT.setText(intToChar(hours_oven));
}

void bOHourMinus10PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenHourT.getText(buffer, sizeof(buffer));

    hours_oven = atoi(buffer);
    hours_oven -= 10;

    if (hours_oven < 0)
    {
        hours_oven = 0;
    }
    
    tOvenHourT.setText(intToChar(hours_oven));
}

void bOMinPlus1PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven += 1;

    if (minutes_oven > 60)
    {
        minutes_oven = 60;
    }

    tOvenMinuteT.setText(intToChar(minutes_oven));
}

void bOMinPlus10PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven += 10;

    if (minutes_oven > 60)
    {
        minutes_oven = 60;
    }

    tOvenMinuteT.setText(intToChar(minutes_oven));
}

void bOMinMinus1PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven -= 1;

    if (minutes_oven < 0)
    {
        minutes_oven = 0;
    }

    tOvenMinuteT.setText(intToChar(minutes_oven));
}

void bOMinMinus10PopCallback(void *ptr)
{
    dbSerialPrintln("bOHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tOvenMinuteT.getText(buffer, sizeof(buffer));

    minutes_oven = atoi(buffer);
    minutes_oven -= 10;

    if (minutes_oven < 0)
    {
        minutes_oven = 0;
    }

    tOvenMinuteT.setText(intToChar(minutes_oven));
}

//Page4 LED Timer
void bLHourPlus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_LED = atoi(buffer);
    hours_LED += 1;

    if (hours_LED > 99)
    {
        hours_LED = 99;
    }

    tLEDsHourT.setText(intToChar(hours_LED));
}

void bLHourPlus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_LED = atoi(buffer);
    hours_LED += 10;

    if (hours_LED > 99)
    {
        hours_LED = 99;
    }

    tLEDsHourT.setText(intToChar(hours_LED));
}

void bLHourMinus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_LED = atoi(buffer);
    hours_LED -= 1;

    if (hours_LED < 0)
    {
        hours_LED = 0;
    }

    tLEDsHourT.setText(intToChar(hours_LED));
}

void bLHourMinus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsHourT.getText(buffer, sizeof(buffer));

    hours_LED = atoi(buffer);
    hours_LED -= 10;

    if (hours_LED < 0)
    {
        hours_LED = 0;
    }

    tLEDsHourT.setText(intToChar(hours_LED));
}

void bLMinPlus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourPlus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_LED = atoi(buffer);
    minutes_LED += 1;

    if (minutes_LED > 60)
    {
        minutes_LED = 60;
    }

    tLEDsMinuteT.setText(intToChar(minutes_LED));
}

void bLMinPlus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourPlus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_LED = atoi(buffer);
    minutes_LED += 10;

    if (minutes_LED > 60)
    {
        minutes_LED = 60;
    }

    tLEDsMinuteT.setText(intToChar(minutes_LED));
}

void bLMinMinus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourMinus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_LED = atoi(buffer);
    minutes_LED -= 1;

    if (minutes_LED < 0)
    {
        minutes_LED = 0;
    }

    tLEDsMinuteT.setText(intToChar(minutes_LED));
}

void bLMinMinus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLHourMinus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLEDsMinuteT.getText(buffer, sizeof(buffer));

    minutes_LED = atoi(buffer);
    minutes_LED -= 10;

    if (minutes_LED < 0)
    {
        minutes_LED = 0;
    }

    tLEDsMinuteT.setText(intToChar(minutes_LED));
}

void bHomeTimerPopCallback(void *ptr)
{
    
    uvFurnaceStateMachine.transitionTo(settingsState);   
}
//End Page4

//Page5
void bLED1PopCallback(void *ptr)
{ 
  uint32_t picNum = 0;
  bLED1.getPic(&picNum);
  if(picNum == 9) {
      picNum = 11;

      myBoolean.bLED1State = true;
      
    } else if(picNum == 11) {
      picNum = 9;

      myBoolean.bLED1State = false;

    }
    //Serial.println(picNum);
    bLED1.setPic(picNum);
    sendCommand("ref bLED1");
}

void bLED1plus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLED1plus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity += 1;

    if (LED1_intensity > 100)
    {
        LED1_intensity = 100;
    }

    tLED1.setText(intToChar(LED1_intensity));
}

void bLED1plus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLED1plus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity += 10;

    if (LED1_intensity > 100)
    {
        LED1_intensity = 100;
    }
    
    tLED1.setText(intToChar(LED1_intensity));
}

void bLED1minus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLED1minus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity -= 1;

    if (LED1_intensity < 0)
    {
        LED1_intensity = 0;
    }

    tLED1.setText(intToChar(LED1_intensity));
}

void bLED1minus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLED1minus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));

    LED1_intensity = atoi(buffer);
    LED1_intensity -= 10;

    if (LED1_intensity < 0)
    {
        LED1_intensity = 0;
    }

    tLED1.setText(intToChar(LED1_intensity));
}

void bLED2PopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bLED2.getPic(&picNum);
     if(picNum == 9) {
      picNum = 11;

      myBoolean.bLED2State = true;

    } else if(picNum == 11) {
      picNum = 9;

      myBoolean.bLED2State = false;

    }
    //Serial.println(picNum);
    bLED2.setPic(picNum);
    sendCommand("ref bLED2");
}

void bLED2plus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLED2plus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity += 1;

    if (LED2_intensity > 100)
    {
        LED2_intensity = 100;
    }

    tLED2.setText(intToChar(LED2_intensity));
}

void bLED2plus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLED2plus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity += 10;

    if (LED2_intensity > 100)
    {
        LED2_intensity = 100;
    }

    tLED2.setText(intToChar(LED2_intensity));
}

void bLED2minus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLED2minus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity -= 1;

    if (LED2_intensity <= 0)
    {
        LED2_intensity = 0;
    }

    tLED2.setText(intToChar(LED2_intensity));
}

void bLED2minus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLED2minus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));

    LED2_intensity = atoi(buffer);
    LED2_intensity -= 10;

    if (LED2_intensity < 0)
    {
        LED2_intensity = 0;
    }

    tLED2.setText(intToChar(LED2_intensity));
}

void bLED3PopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bLED3.getPic(&picNum);
    if(picNum == 9) {
      picNum = 11;

      myBoolean.bLED3State = true;

    } else if(picNum == 11) {
      picNum = 9;

      myBoolean.bLED3State = false;

    }
    //Serial.println(picNum);
    bLED3.setPic(picNum);
    sendCommand("ref bLED3");
}

void bLED3plus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLED3plus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity += 1;

    if (LED3_intensity > 100)
    {
        LED3_intensity = 100;
    }
 
    tLED3.setText(intToChar(LED3_intensity));
}

void bLED3plus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLED3plus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity += 10;

    if (LED3_intensity > 100)
    {
        LED3_intensity = 100;
    }

    tLED3.setText(intToChar(LED3_intensity));
}

void bLED3minus1PopCallback(void *ptr)
{
    dbSerialPrintln("bLED3minus1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity -= 1;

    if (LED3_intensity < 0)
    {
        LED3_intensity = 0;
    }

    tLED3.setText(intToChar(LED3_intensity));
}

void bLED3minus10PopCallback(void *ptr)
{
    dbSerialPrintln("bLED3minus10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));

    LED3_intensity = atoi(buffer);
    LED3_intensity -= 10;

    if (LED3_intensity < 0)
    {
        LED3_intensity = 0;
    }

    tLED3.setText(intToChar(LED3_intensity));
}

void bHomeLEDPopCallback(void *ptr)
{   
    memset(buffer, 0, sizeof(buffer));
    tLED1.getText(buffer, sizeof(buffer));
    LED1_intensity = atoi(buffer);

    memset(buffer, 0, sizeof(buffer));
    tLED2.getText(buffer, sizeof(buffer));
    LED2_intensity = atoi(buffer);
    
    memset(buffer, 0, sizeof(buffer));
    tLED3.getText(buffer, sizeof(buffer));
    LED3_intensity = atoi(buffer);

    LED1_intens = LED1_intensity;
    LED2_intens = LED2_intensity;
    LED3_intens = LED3_intensity;
    
    LED1_intensity = map(LED1_intensity, 0, 100, 0, 255);
    LED2_intensity = map(LED2_intensity, 0, 100, 0, 255);
    LED3_intensity = map(LED3_intensity, 0, 100, 0, 255);

    uvFurnaceStateMachine.transitionTo(settingsState);   
}

//End Page5

//Page6
void bHomePIDPopCallback(void *ptr)
{
    uvFurnaceStateMachine.transitionTo(settingsState);   
}

void bAutotunePopCallback(void *ptr)
{

}
//End Page6

//Page7
void bResetPopCallback(void *ptr)
{
  
}
//End Page7

//Page8
void bHomeCreditsPopCallback(void *ptr)
{
  page2.show();
}
//End Page8

/*******************************************************************************
 interrupt service routine for DS3231 clock
*******************************************************************************/
volatile boolean alarmIsrWasCalled = false;

void alarmIsr(){
    alarmIsrWasCalled = true;
}

/*******************************************************************************
 IO mapping
*******************************************************************************/
void setup() {
  // Initialize LEDs:
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  controlLEDs(0, 0, 0);
  // Initialize Relay Control:
  pinMode(RelayPin, OUTPUT);    // Output mode to drive relay
  digitalWrite(RelayPin, HIGH);  // make sure it is off to start

  nexInit();

  myBoolean.preset1 = 0;
  myBoolean.preset2 = 0;
  myBoolean.preset3 = 0;
  myBoolean.preset4 = 0;
  myBoolean.preset5 = 0;
  myBoolean.preset6 = 0;
  myBoolean.preheat = 0;

  myBoolean.bLED1State = false;
  myBoolean.bLED2State = false;
  myBoolean.bLED3State = false;

  myBoolean.didReadConfig = false;
  
  #ifdef DEBUG
    Serial.begin(9600);
  #endif
//  Serial2.begin(9600);

  //pinMode(reedSwitch, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(reedSwitch), furnaceDoor, FALLING);

  pinMode(onOffButton, OUTPUT);
  digitalWrite(onOffButton, onOffState);

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
  bCredits.attachPop(bCreditsPopCallback, &bCredits);

  //Page3
  bTempPlus1.attachPop(bTempPlus1PopCallback, &bTempPlus1);
  bTempPlus10.attachPop(bTempPlus10PopCallback, &bTempPlus10);
  bTempMinus1.attachPop(bTempMinus1PopCallback, &bTempMinus1);
  bTempMinus10.attachPop(bTempMinus10PopCallback, &bTempMinus10);
  bHomeTemp.attachPop(bHomeTempPopCallback, &bHomeTemp);
  bPreheat.attachPop(bPreheatPopCallback, &bPreheat);

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
  bAutotune.attachPop(bAutotunePopCallback, &bAutotune);

  //Page7
  bReset.attachPop(bResetPopCallback, &bReset);

  //Page8
  bHomeCredits.attachPop(bHomeCreditsPopCallback, &bHomeCredits);
  
  //declare and init pins
  
  //Disable the default square wave of the SQW pin.
  RTC.squareWave(SQWAVE_NONE);
  
  pinMode(SQW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), alarmIsr, FALLING);

  // Initialize the PID and related variables
  LoadParameters();
  myPID.SetTunings(Kp,Ki,Kd);
  myPID.SetSampleTime(1000);
  myPID.SetOutputLimits(0, WindowSize);

  //Initializing Chip Select pin for MAX31855
  pinMode(cs_MAX31855, OUTPUT);
  selMAX31855();
  
  selSD();
  //Initializing SD Card
  DEBUG_PRINTLN(F("Initializing SD card..."));
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  //pinMode(4, OUTPUT);
  //digitalWrite(4, HIGH);
    // see if the card is present and can be initialized:
  
  if (!SD.begin(SDCARD_CS)) {
    DEBUG_PRINTLN(F("Card failed, or not present"));
    // don't do anything more:
    while (1) ;
  }
  DEBUG_PRINTLN(F("card initialized."));

  selETH();
  if (Ethernet.begin(mac) == 0) {
    // no point in carrying on, so do nothing forevermore:
    while (1) {
      DEBUG_PRINTLN(F("Failed to configure Ethernet using DHCP"));
      delay(10000);
    }
  }
  Udp.begin(localPort);

  DEBUG_PRINTLN(F("IP number assigned by DHCP is "));
  DEBUG_PRINTLN(Ethernet.localIP());

  // Run timer2 interrupt every 15 ms 
  TCCR2A = 0;
  TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20;

  //Timer2 Overflow Interrupt Enable
  TIMSK2 |= 1<<TOIE2;
  
   if (USE_BLYNK == "yes") {
    //init Blynk
    Blynk.begin(auth);
  }

  DEBUG_PRINTLN(F("setup ready"));

}

/************************************************
 Timer Interrupt Handler
************************************************/
SIGNAL(TIMER2_OVF_vect) 
{
  if (uvFurnaceStateMachine.isInState(offState) == true)
  {
    digitalWrite(RelayPin, HIGH);  // make sure relay is off
  }
  else
  {
    DriveOutput();
    //DEBUG_PRINTLN("DriveOutput");
  }
}

/************************************************
 Timer Interrupt Handler
************************************************/
void furnaceDoor() {
  if(uvFurnaceStateMachine.isInState(runState) == true){
      uvFurnaceStateMachine.immediateTransitionTo(errorState);
  }else {
    return;
  }
}

/************************************************
 Called by ISR every 15ms to drive the output
************************************************/
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
     digitalWrite(RelayPin,LOW);
  }
  else
  {
     digitalWrite(RelayPin,HIGH);
  }
}

void loop() {
  nexLoop(nex_listen_list);
  
  if (USE_BLYNK == "yes") {
    //all the Blynk magic happens here
    Blynk.run();
  }  
  //this function reads the temperature of the MAX31855 Thermocouple Amplifier
  readTemperature();
  readInternalTemperature();
  updateCurrentTemperature();
  updateBlynk();

  if (alarmIsrWasCalled){
     if (RTC.alarm(ALARM_1)) {
        uvFurnaceStateMachine.transitionTo(offState);
     }
     alarmIsrWasCalled = false;
  }

  //this function updates the FSM
  // the FSM is the heart of the UV furnace - all actions are defined by its states
  uvFurnaceStateMachine.update();
}

/*******************************************************************************
 * Function Name  : updateCurrentTemperature
 * Description    : updates the value of target temperature of the uv furnace
 * Return         : none
 *******************************************************************************/
void updateCurrentTemperature()
{   
    if(currentTemperature != lastTemperature && (uvFurnaceStateMachine.isInState(runState) || uvFurnaceStateMachine.isInState(offState))) {
      memset(buffer, 0, sizeof(buffer));
      itoa(int(currentTemperature), buffer, 10);
      tTemp.setText(buffer);
    }
    lastTemperature = currentTemperature;
}

/*******************************************************************************
 * Function Name  : readTemperature
 * Description    : reads the temperature of the MAX31855 sensor at every MAX31855_SAMPLE_INTERVAL
                    corrected temperature reading for a K-type thermocouple
                    allowing accurate readings over an extended range
                    http://forums.adafruit.com/viewtopic.php?f=19&t=32086&p=372992#p372992
                    assuming global: Adafruit_MAX31855 thermocouple(CLK, CS, DO);
 * Return         : 0
 *******************************************************************************/
int readTemperature(){ 
   //time is up? no, then come back later
   if (MAX31855SampleInterval < MAX31855_SAMPLE_INTERVAL) {
    return 0;
   }

   selMAX31855();

   //time is up, reset timer
   MAX31855SampleInterval = 0;
   // MAX31855 thermocouple voltage reading in mV
   float thermocoupleVoltage = (thermocouple.readCelsius() - thermocouple.readInternal()) * 0.041276;
   
   // MAX31855 cold junction voltage reading in mV
   float coldJunctionTemperature = thermocouple.readInternal();
   float coldJunctionVoltage = -0.176004136860E-01 +
      0.389212049750E-01  * coldJunctionTemperature +
      0.185587700320E-04  * pow(coldJunctionTemperature, 2.0) +
      -0.994575928740E-07 * pow(coldJunctionTemperature, 3.0) +
      0.318409457190E-09  * pow(coldJunctionTemperature, 4.0) +
      -0.560728448890E-12 * pow(coldJunctionTemperature, 5.0) +
      0.560750590590E-15  * pow(coldJunctionTemperature, 6.0) +
      -0.320207200030E-18 * pow(coldJunctionTemperature, 7.0) +
      0.971511471520E-22  * pow(coldJunctionTemperature, 8.0) +
      -0.121047212750E-25 * pow(coldJunctionTemperature, 9.0) +
      0.118597600000E+00  * exp(-0.118343200000E-03 * 
                           pow((coldJunctionTemperature-0.126968600000E+03), 2.0) 
                        );                     
                        
   // cold junction voltage + thermocouple voltage         
   float voltageSum = thermocoupleVoltage + coldJunctionVoltage;
   
   // calculate corrected temperature reading based on coefficients for 3 different ranges   
   float b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10;
   if(thermocoupleVoltage < 0){
      b0 = 0.0000000E+00;
      b1 = 2.5173462E+01;
      b2 = -1.1662878E+00;
      b3 = -1.0833638E+00;
      b4 = -8.9773540E-01;
      b5 = -3.7342377E-01;
      b6 = -8.6632643E-02;
      b7 = -1.0450598E-02;
      b8 = -5.1920577E-04;
      b9 = 0.0000000E+00;
   }
   
   else if(thermocoupleVoltage < 20.644){
      b0 = 0.000000E+00;
      b1 = 2.508355E+01;
      b2 = 7.860106E-02;
      b3 = -2.503131E-01;
      b4 = 8.315270E-02;
      b5 = -1.228034E-02;
      b6 = 9.804036E-04;
      b7 = -4.413030E-05;
      b8 = 1.057734E-06;
      b9 = -1.052755E-08;
   }
   
   else if(thermocoupleVoltage < 54.886){
      b0 = -1.318058E+02;
      b1 = 4.830222E+01;
      b2 = -1.646031E+00;
      b3 = 5.464731E-02;
      b4 = -9.650715E-04;
      b5 = 8.802193E-06;
      b6 = -3.110810E-08;
      b7 = 0.000000E+00;
      b8 = 0.000000E+00;
      b9 = 0.000000E+00;
   }
   
   else {
      // TODO: handle error - out of range
      return 0;
   }
   
   currentTemperature = b0 + 
      b1 * voltageSum +
      b2 * pow(voltageSum, 2.0) +
      b3 * pow(voltageSum, 3.0) +
      b4 * pow(voltageSum, 4.0) +
      b5 * pow(voltageSum, 5.0) +
      b6 * pow(voltageSum, 6.0) +
      b7 * pow(voltageSum, 7.0) +
      b8 * pow(voltageSum, 8.0) +
      b9 * pow(voltageSum, 9.0);

    DEBUG_PRINT(currentTemperature);
    DEBUG_PRINTLN(F(" C"));
}

/*******************************************************************************
 * Function Name  : readInternalTemperature
 * Description    : reads the temperature of the DS3231
 * Return         : 0
 *******************************************************************************/
int readInternalTemperature(){
  if(DS3231TempInterval < DS3231_TEMP_INTERVAL) {
    return 0;
  }
  
  if(RTC.temperature() / 4.0 > 40) {
    DEBUG_PRINTLN(F("ERROR"));
    uvFurnaceStateMachine.immediateTransitionTo(offState);
  }
}

/*******************************************************************************
 * Function Name  : setDS3231Alarm
 * Description    : sets the alarm for the time the user has choosen
 * Return         : 0
*******************************************************************************/
int setDS3231Alarm(byte minutes, byte hours) {

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

  return 0;
}

/*******************************************************************************
 * Function Name  : updateBlynk
 * Description    : updates the blynk app
 * Return         : 0
*******************************************************************************/
int updateBlynk(){
   if (BlynkInterval < BLYNK_INTERVAL) {
    return 0;
   }
   //DEBUG_PRINTLN(F("updating Blynk"));
   selETH();
   Blynk.virtualWrite(V0, currentTemperature);
}

/*******************************************************************************
 * Function Name  : SaveParameters
 * Description    : Save any parameter changes to EEPROM
 * Return         : 0
*******************************************************************************/
void SaveParameters()
{
   if (Setpoint != EEPROM_readDouble(SpAddress))
   {
      DEBUG_PRINTLN(F("Save any parameter changes to EEPROM"));
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

/*******************************************************************************
 * Function Name  : LoadParameters
 * Description    : Load parameters from EEPROM
 * Return         : 0
*******************************************************************************/
void LoadParameters()
{
   DEBUG_PRINTLN(F("Load parameters from EEPROM"));

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

/*******************************************************************************
 * Function Name  : EEPROM_writeDouble
 * Description    : Write floating point values to EEPROM
 * Return         : 0
*******************************************************************************/
void EEPROM_writeDouble(int address, double value)
{
   DEBUG_PRINTLN(F("EEPROM_writeDouble"));
   
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
   {
      EEPROM.write(address++, *p++);
   }
}

/*******************************************************************************
 * Function Name  : EEPROM_readDouble
 * Description    : Read floating point values from EEPROM
 * Return         : 0
*******************************************************************************/
double EEPROM_readDouble(int address)
{
   DEBUG_PRINTLN(F("EEPROM_readDouble"));

   double value = 0.0;
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
   {
      *p++ = EEPROM.read(address++);
   }
   return value;
}

/*******************************************************************************
 * Function Name  : DoControl
 * Description    : Execute the control loop
 * Return         : 0
*******************************************************************************/
void DoControl()
{
  // Read the input:
    Input = currentTemperature;
    
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

/*******************************************************************************
 * Function Name  : StartAutoTune
 * Description    : Start the Auto-Tuning cycle
 * Return         : 0
*******************************************************************************/
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

/*******************************************************************************
 * Function Name  : FinishAutoTune
 * Description    : Return to normal control
 * Return         : 0
*******************************************************************************/ 
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

/*******************************************************************************
 * Function Name  : selETH
 * Description    : selects the Ethernet chip to make communication possible
 * Return         : 0
 *******************************************************************************/
 void selETH() {    
  digitalWrite(SDCARD_CS, HIGH);
  digitalWrite(W5200_CS, LOW);
  digitalWrite(cs_MAX31855, HIGH); 
}

/*******************************************************************************
 * Function Name  : selSD
 * Description    : selects the SD card to make communication possible
 * Return         : 0
 *******************************************************************************/
void selSD() {     
  digitalWrite(W5200_CS, HIGH);
  digitalWrite(SDCARD_CS, LOW);
  digitalWrite(cs_MAX31855, HIGH); 
}


/*******************************************************************************
 * Function Name  : selMAX31855
 * Description    : selects the MAX31855 thermocouple sensor to make 
 *                  communication possible
 * Return         : 0
 *******************************************************************************/
void selMAX31855(){  
  digitalWrite(W5200_CS, HIGH);
  digitalWrite(SDCARD_CS, HIGH);
  digitalWrite(cs_MAX31855, LOW); 
}

/*******************************************************************************
 * Function Name  : controlLEDs
 * Description    : sets the intensity of the UV LEDs
 * Return         : 0
 *******************************************************************************/
void controlLEDs(byte intensity1, byte intensity2, byte intensity3){
  if(myBoolean.bLED1State == true){
    analogWrite(LED1, intensity1);
    DEBUG_PRINTLN(LED1_intensity);
  }else {
    analogWrite(LED1, 0);
  }
  if(myBoolean.bLED2State == true){
    analogWrite(LED2, intensity2);
    DEBUG_PRINTLN(LED2_intensity);
  }else {
    analogWrite(LED2, 0);
  }
  if(myBoolean.bLED3State == true){
    analogWrite(LED3, intensity3);
    DEBUG_PRINTLN(LED3_intensity);
  }else {
    analogWrite(LED3, 0);
  }
}

/*******************************************************************************
 * Function Name  : updateGraph
 * Description    : updates the temperature graph on the display
 * Return         : 0
 *******************************************************************************/
void updateGraph(){
  if(GraphUpdateInterval < GRAPH_UPDATE_INTERVAL){
    return;
  }
  s0.addValue(0, currentTemperature);
  s0.addValue(1, Setpoint);
  GraphUpdateInterval = 0;
}

void sendToInfluxDB(){
  if(InfluxdbUpdateInterval < INFLUXDB_UPDATE_INTERVAL){
    return;
  }
  String line = "UV Tset=" + String(Setpoint, 0) + ",T=" + String(currentTemperature, 1);
  Udp.beginPacket(INFLUXDB_HOST, INFLUXDB_PORT);
  Udp.print(line);
  Udp.endPacket();
  InfluxdbUpdateInterval = 0;
}

/*******************************************************************************
 * Function Name  : sendNTPpacket
 * Description    : send an NTP request to the time server at the given address
 * Return         : timestamp
 *******************************************************************************/ 
unsigned long sendNTPpacket(char* address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/*******************************************************************************
 * Function Name  : refreshCountdown
 * Description    : updates the countdown on the Display
 * Return         : 0
 *******************************************************************************/
void refreshCountdown(){
      //refresh countdown
      if(CountdownUpdateInterval < COUNTDOWN_UPDATE_INTERVAL){
        return;
      }
      
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

      CountdownUpdateInterval = 0;
}

/*******************************************************************************
 * Function Name  : blinkPowerLED
 * Description    : blinks the LED of the Power button
 * Return         : 0
 *******************************************************************************/
void blinkPowerLED(){
  if (POWERLEDBlinkInterval > POWERLED_BLINK_INTERVAL){
    if (onOffState == LOW) {
      onOffState = HIGH;
    } else {
      onOffState = LOW;
    }
    digitalWrite(onOffButton, onOffState);
    POWERLEDBlinkInterval = 0;
  }
}

/*******************************************************************************
 * Function Name  : fadePowerLED
 * Description    : fades the LED of the Power Button
 * Return         : 0
 *******************************************************************************/
void fadePowerLED(){
   fadeTime = millis();
   fadeValue = 128+127*cos(2*PI/periode*fadeTime);
   analogWrite(onOffButton, fadeValue);           // sets the value (range from 0 to 255) 
}

/*******************************************************************************
 * Function Name  : readConfiguration
 * Description    : reads the configuration of a config file on the SD card.
 * Return         : true
 *******************************************************************************/
boolean readConfiguration(const char CONFIG_FILE[]) {
  /*
   * Length of the longest line expected in the config file.
   * The larger this number, the more memory is used
   * to read the file.
   * You probably won't need to change this number.
   */
  selSD();
  const uint8_t CONFIG_LINE_LENGTH = 20;
  
  // The open configuration file.
  SDConfigFile cfg;
  
  // Open the configuration file.
  if (!cfg.begin(CONFIG_FILE, CONFIG_LINE_LENGTH)) {
    DEBUG_PRINT(F("Failed to open configuration file: "));
    DEBUG_PRINTLN(CONFIG_preset1);
    return false;
  }
  
  // Read each setting from the file.
  while (cfg.readNextSetting()) {
    
    // Put a nameIs() block here for each setting you have.

    // doDelay
    if (cfg.nameIs("myBoolean.bLED1State")) {
      
      myBoolean.bLED1State = cfg.getBooleanValue();
      DEBUG_PRINT(F("Read myBoolean.bLED1State: "));
      if (myBoolean.bLED1State) {
        DEBUG_PRINTLN(F("true"));
      } else {
        DEBUG_PRINTLN(F("false"));
      }
    
    // waitMs integer
    } else if (cfg.nameIs("myBoolean.bLED2State")) {
      
      myBoolean.bLED2State = cfg.getBooleanValue();
      DEBUG_PRINT(F("Read myBoolean.bLED2State: "));
      if (myBoolean.bLED2State) {
        DEBUG_PRINTLN(F("true"));
      } else {
        DEBUG_PRINTLN(F("false"));
      }
    } else if (cfg.nameIs("myBoolean.bLED3State")) {
      
      myBoolean.bLED1State = cfg.getBooleanValue();
      DEBUG_PRINT(F("Read myBoolean.bLED3State: "));
      if (myBoolean.bLED3State) {
        DEBUG_PRINTLN(F("true"));
      } else {
        DEBUG_PRINTLN(F("false"));
      }
    } else if (cfg.nameIs("temp")) {
      
      Setpoint = cfg.getIntValue();
      DEBUG_PRINT(F("Read Setpoint: "));
      DEBUG_PRINTLN(Setpoint);

    // hello string (char *)
    } else if (cfg.nameIs("LED1_intensity")) {

      LED1_intensity = cfg.getIntValue();
      DEBUG_PRINT(F("Read LED1_intensity: "));
      DEBUG_PRINTLN(LED1_intensity);
      
    } else if (cfg.nameIs("LED2_intensity")) {

      LED2_intensity = cfg.getIntValue();
      DEBUG_PRINT(F("Read LED2_intensity: "));
      DEBUG_PRINTLN(LED2_intensity);
    
    } else if (cfg.nameIs("LED3_intensity")) {

      LED1_intensity = cfg.getIntValue();
      DEBUG_PRINT(F("Read LED3_intensity: "));
      DEBUG_PRINTLN(LED3_intensity);

    } else if (cfg.nameIs("hours_oven")) {

      hours_oven = cfg.getIntValue();
      DEBUG_PRINT(F("Read hours_oven: "));
      DEBUG_PRINTLN(hours_oven);

    } else if (cfg.nameIs("minutes_oven")) {

      minutes_oven = cfg.getIntValue();
      DEBUG_PRINT(F("Read minutes_oven: "));
      DEBUG_PRINTLN(minutes_oven);

    }
    else {
      // report unrecognized names.
      DEBUG_PRINT(F("Unknown name in config: "));
      DEBUG_PRINTLN(cfg.getName());
    }
  }
  
  // clean up
  cfg.end();
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 FINITE STATE MACHINE FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/
void initEnterFunction(){
  DEBUG_PRINTLN(F("initEnter"));

  //start the timer of this cycle
  initTimer = 0;
  page0.show();
  tVersion.setText(VERSION); 

  sendNTPpacket(timeServer); // send an NTP packet to a time server
  selETH();
  // wait to see if a reply is available
  delay(1000);
  if ( Udp.parsePacket() ) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    DEBUG_PRINT(F("Seconds since Jan 1 1900 = " ));
    DEBUG_PRINTLN(secsSince1900);

    // now convert NTP time into everyday time:
    DEBUG_PRINT(F("Unix time = "));
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    DEBUG_PRINTLN(epoch);

    Udp.stop();

    // print the hour, minute and second:
    DEBUG_PRINT(F("The UTC time is "));       // UTC is the time at Greenwich Meridian (GMT)
    DEBUG_PRINT((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    DEBUG_PRINT(F(":"));
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      DEBUG_PRINT(F("0"));
    }
    DEBUG_PRINT((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    DEBUG_PRINT(F(":"));
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      DEBUG_PRINT(F("0"));
    }
    DEBUG_PRINTLN(epoch % 60); // print the second

    DEBUG_PRINTLN(RTC.set(epoch));

    setSyncProvider(RTC.get);   // the function to get the time from the RTC

    tmElements_t tm;
    RTC.read(tm);
    DEBUG_PRINT(tm.Day, DEC);
    DEBUG_PRINT(F("."));
    DEBUG_PRINT(tm.Month, DEC);
    DEBUG_PRINT(F("."));
    DEBUG_PRINT(year(), DEC);
    DEBUG_PRINT(F(" "));
    DEBUG_PRINT(tm.Hour, DEC);
    DEBUG_PRINT(F(":"));
    DEBUG_PRINT(tm.Minute,DEC);
    DEBUG_PRINT(F(":"));
    DEBUG_PRINTLN(tm.Second,DEC);   
  }
}

void initUpdateFunction(){
  //DEBUG_PRINTLN("initUpdate");
  //time is up?
  if (initTimer > INIT_TIMEOUT) {
    uvFurnaceStateMachine.transitionTo(offState);
  }
}
void initExitFunction(){
  DEBUG_PRINTLN(F("Initialization done"));
}

void idleEnterFunction(){
  
  //DEBUG_PRINTLN(F("idleEnter"));
}
void idleUpdateFunction(){
  //DEBUG_PRINTLN(F("idleUpdate"));
}
void idleExitFunction(){
  //DEBUG_PRINTLN(F("idleExit"));
}

void settingsEnterFunction(){
  DEBUG_PRINTLN(F("settingsEnter"));
  page2.show();
  
  if(myBoolean.preset1 == 1){
      bPreSet1.setPic(4);
  }
  if(myBoolean.preset2 == 1){
      bPreSet2.setPic(4);
  }
  if(myBoolean.preset3 == 1){
      bPreSet3.setPic(4);
  }
  if(myBoolean.preset4 == 1){
      bPreSet4.setPic(4);
  }
  if(myBoolean.preset5 == 1){
      bPreSet5.setPic(4);
  }
  if(myBoolean.preset6 == 1){
      bPreSet6.setPic(4);
  }
 
  sendCommand("ref 0");
}
void settingsUpdateFunction(){
  //DEBUG_PRINTLN(F("settingsUpdate"));
}
void settingsExitFunction(){
  DEBUG_PRINTLN("settingsExit");
}

void setLEDsEnterFunction(){
  DEBUG_PRINTLN("setLEDsEnter");
  page5.show();

  tLED1.setText(intToChar(LED1_intens));
  tLED2.setText(intToChar(LED2_intens)); 
  tLED3.setText(intToChar(LED3_intens));

  if(myBoolean.bLED1State == true){
    bLED1.setPic(11);
  }else{
    bLED1.setPic(9);
  }
  if(myBoolean.bLED2State == true){
    bLED2.setPic(11);
  }else{
    bLED2.setPic(9);
  }
  if(myBoolean.bLED3State == true){
    bLED3.setPic(11);
  }else{
    bLED3.setPic(9);
  } 
  sendCommand("ref 0");
}
void setLEDsUpdateFunction(){
  //DEBUG_PRINTLN(F("setLEDsUpdate"));
}
void setLEDsExitFunction(){
  DEBUG_PRINTLN("setLEDsExit");
}

void setTempEnterFunction(){
  DEBUG_PRINTLN("setTempEnter");
  page3.show();

  tTempSetup.setText(intToChar(Setpoint));

  sendCommand("ref 0");
}

void setTempUpdateFunction(){
  //DEBUG_PRINTLN(F("setTempUpdate"));
  
}
void setTempExitFunction(){
  DEBUG_PRINTLN(F("setTempExit"));
  selETH();
  Blynk.virtualWrite(V1, Setpoint);
}

void setTimerEnterFunction(){
  DEBUG_PRINTLN(F("setTimerEnter"));
  page4.show();
  tOvenMinuteT.setText(intToChar(minutes_oven));
  tOvenHourT.setText(intToChar(hours_oven));
  tLEDsMinuteT.setText(intToChar(minutes_LED));
  tLEDsHourT.setText(intToChar(hours_LED));
  
  sendCommand("ref 0");
}
void setTimerUpdateFunction(){
  //DEBUG_PRINTLN(F("setTimerUpdate"));
}
void setTimerExitFunction(){
  DEBUG_PRINTLN(F("setTimerExit"));
}

void setPIDEnterFunction(){
  DEBUG_PRINTLN(F("setPIDEnter"));
  page6.show();
  sendCommand("ref 0");
}
void setPIDUpdateFunction(){
  //DEBUG_PRINTLN(F("setPIDUpdate"));
}
void setPIDExitFunction(){
  DEBUG_PRINTLN(F("setPIDExit"));
}

void runEnterFunction(){
   DEBUG_PRINTLN(F("runEnter"));
 
   //set alarm
   setDS3231Alarm(minutes_oven, hours_oven);

   controlLEDs(LED1_intensity, LED2_intensity, LED3_intensity);
  
   //turn the PID on
   myPID.SetMode(AUTOMATIC);
   windowStartTime = millis();

   SaveParameters();
   myPID.SetTunings(Kp,Ki,Kd);

   Udp.begin(INFLUXDB_PORT);
   InfluxdbUpdateInterval = 0;
}

void runUpdateFunction(){
   //DEBUG_PRINTLN(F("runUpdate"));
    
   DoControl();
      
   float pct = map(Output, 0, WindowSize, 0, 1000);
      
   // periodically log to serial port in csv format
   //if (millis() - lastLogTime > logInterval)  
   //{
    //  Serial.print(Input);
    //  Serial.print(",");
    //  Serial.println(Output);
   //}
   updateGraph();
   
   fadePowerLED();
   refreshCountdown();
   sendToInfluxDB();
}

void runExitFunction(){
  //DEBUG_PRINTLN(F("runExit"));
  selETH();
  Blynk.notify("Curing finished!");
}

void errorEnterFunction(){
  DEBUG_PRINTLN(F("errorEnter"));
  //Turn off LEDs
  myPID.SetMode(MANUAL);
  controlLEDs(0,0,0);
  digitalWrite(RelayPin, HIGH);  // make sure it is off
  RTC.alarm(ALARM_1);
  RTC.alarmInterrupt(ALARM_1, false);
  selETH();
  Blynk.notify("Error occured!");
}
void errorUpdateFunction(){
  DEBUG_PRINTLN(F("errorUpdate"));
  blinkPowerLED();
}
void errorExitFunction(){
  //DEBUG_PRINTLN(F("errorExit"));
}

void offEnterFunction(){
    DEBUG_PRINTLN(F("offEnter"));

    page1.show();
    hour_uv.setText(intToChar(hours_oven));
    min_uv.setText(intToChar(minutes_oven));
    sendCommand("ref 0");
    
    myPID.SetMode(MANUAL);
    controlLEDs(0,0,0);
    digitalWrite(RelayPin, HIGH);  // make sure it is off
}

void offUpdateFunction(){ 
}

void offExitFunction(){
}
