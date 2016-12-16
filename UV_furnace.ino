/* UV furnace                                                          *
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

#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <BlynkSimpleEthernet2.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include "access.h"
#include "configuration.h"
#include <PID_AutoTune_v0.h>
#include <PID_v1.h>
//Library for DS3231 clock
#include <DS3232RTC.h>
//Library for LCD touch screen
#include <Nextion.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <SPI.h>
#include "SD.h"
// So we can save and retrieve settings
#include <EEPROM.h>
#include <Adafruit_MAX31855.h>
#include <elapsedMillis.h>
#include <FiniteStateMachine.h>
#include <SDConfigFile.h>
#include "NexUpload.h"
#include <SoftReset.h>
#include <stdlib.h>

#define APP_NAME "UV furnace"
const char VERSION[] = "Version 0.2";

struct myBoolStruct
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
   uint8_t bLED4State: 1;

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
State preheatState = State ( preheatEnterFunction, preheatUpdateFunction, preheatExitFunction );

//initialize state machine, start in state: Idle
FSM uvFurnaceStateMachine = FSM(initState);

//milliseconds for the init cycle, so everything gets stabilized
#define INIT_TIMEOUT 5000
elapsedMillis initTimer;

/************************************************
 Pin definitions
************************************************/

// LEDs
#define LED1 5
#define LED2 6
#define LED3 7
#define LED4 8
#define LEDlight 9

// Output relay
#define RelayPin 32

// Reed switch
#define reedSwitch 19
volatile boolean doorChanged;

// ON/OFF Button LED
#define onOffButton 12

/************************************************
 LED variables
************************************************/
uint32_t LED1_intensity = 100;
uint32_t LED2_intensity = 100;
uint32_t LED3_intensity = 100;
uint32_t LED4_intensity = 100;

uint32_t LED1_mapped = 0;
uint32_t LED2_mapped = 0;
uint32_t LED3_mapped = 0;
uint32_t LED4_mapped = 0;

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
bool ethernetAvailable = false;

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

/************************************************
 Auto Tune Variables and constants
************************************************/
byte ATuneModeRemember = 2;

double aTuneStep = 500;
double aTuneNoise = 1;
unsigned int aTuneLookBack = 20;

boolean tuning = false;

PID_ATune aTune(&Input, &Output);

/************************************************
 Timer Variables
************************************************/
byte hourAlarm = 0;
byte minuteAlarm = 0;
byte secondAlarm = 0;
uint16_t dayAlarm = 0;

/************************************************
 time settings variables for heating and leds
************************************************/
uint32_t hours_oven = 0;
uint32_t minutes_oven = 0;

uint32_t hours_LED = 0;
uint32_t minutes_LED = 0;

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
#define MAX31855_SAMPLE_INTERVAL   100    // Sample room temperature every 5 seconds
//Adafruit_MAX31855 thermocouple(CLK, CS, DO);
#define cs_MAX31855   47
Adafruit_MAX31855 thermocouple(cs_MAX31855);
elapsedMillis MAX31855SampleInterval;
float currentTemperature;
float lastTemperature;
bool newTemperature = false;

const int NUMBER_OF_SAMPLES = 10;
#define DUMMY -100
#define DUMMY_ARRAY { DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY, DUMMY };
float temperatureSamples[NUMBER_OF_SAMPLES] = DUMMY_ARRAY;
float averageTemperature;

/*******************************************************************************
 DS3231
*******************************************************************************/
#define SQW_PIN 3
#define DS3231_TEMP_INTERVAL   2000
elapsedMillis DS3231TempInterval;

//Central European Time (Vienna, Berlin)
TimeChangeRule myCEST = {"CEST", Last, Sat, Mar, 2, +120};    //Daylight time = UTC + 2 hours
TimeChangeRule myCET = {"CET", Sun, Sun, Oct, 3, +60};     //Standard time = UTC + 1 hours
Timezone myTZ(myCEST, myCET);

TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
time_t utc, local;

byte calcMinutes = 0;
byte calcHours = 0;

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
char msg[30];

/*******************************************************************************
 Blynk
 Your blynk token goes in another file to avoid sharing it by mistake
*******************************************************************************/
#define BLYNK_INTERVAL   10000
elapsedMillis BlynkInterval;

int pushNotification = 0;
int emailNotification = 0;
int twitterNotification = 0;
#define BLYNK_GREEN     "#23C48E"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"

/*******************************************************************************
 SD-Card
*******************************************************************************/
const int SDCARD_CS = 4;
File dataFile;
#define SD_CARD_SAMPLE_INTERVAL   1000
elapsedMillis sdCard;

/*******************************************************************************
 Display
*******************************************************************************/

/*******************************************************************************
 interrupt service routine for DS3231 clock
*******************************************************************************/
volatile boolean alarmIsrWasCalled = false;

void alarmIsr(){
    alarmIsrWasCalled = true;
}

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
NexPage page9    = NexPage(9, 0, "page9");

/*
 * Declare a button object [page id:0,component id:1, component name: "b0"].
 */

//Page0
NexText tVersion = NexText(0, 1, "tVersion");

//Page1
NexText tTemp = NexText(1, 4, "tTemp");
NexNumber nhour_uv = NexNumber(1, 11, "nhour_uv");
NexNumber nmin_uv = NexNumber(1, 12, "nmin_uv");
NexWaveform sChart = NexWaveform(1, 3, "sChart");
NexButton bSettings = NexButton(1, 1, "bSettings");
NexButton bOnOff = NexButton(1, 2, "bOnOff");
NexNumber nSetpoint = NexNumber(1, 13, "nSetpoint");
NexCrop cLED1 = NexCrop(1, 5, "cLED1");
NexCrop cLED2 = NexCrop(1, 6, "cLED2");
NexCrop cLED3 = NexCrop(1, 7, "cLED3");
NexCrop cLED4 = NexCrop(1, 8, "cLED4");
NexText tToast = NexText(1, 9, "tToast");
NexCrop cPreheat = NexCrop(1, 10, "cPreheat");

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
NexButton bUpdate = NexButton(2, 13, "bUpdate");

//Page3
NexNumber nTempSetup = NexNumber(3, 7, "nTempSetup");
NexButton bHomeTemp = NexButton(3, 5, "bHomeTemp");
NexButton bPreheat = NexButton(3, 6 , "bPreheat");

//Page4
NexButton bHomeTimer = NexButton(4, 17, "bHomeTimer");
NexNumber nOvenHourT = NexNumber(4, 18, "nOvenHourT");
NexNumber nOvenMinuteT = NexNumber(4, 19, "nOvenMinuteT");
NexNumber nLEDsHourT = NexNumber(4, 20, "nLEDsHourT");
NexNumber nLEDsMinuteT = NexNumber(4, 21, "nLEDsMinuteT");

//Page5
NexButton bLED1 = NexButton(5, 1, "bLED1");
NexButton bLED2 = NexButton(5, 2, "bLED2");
NexButton bLED3 = NexButton(5, 3, "bLED3");
NexButton bLED4 = NexButton(5, 17, "bLED4");
NexButton bHomeLED = NexButton(5, 16, "bHomeLED");
NexNumber nLED1 = NexNumber(5, 26, "nLED1");
NexNumber nLED2 = NexNumber(5, 27, "nLED2");
NexNumber nLED3 = NexNumber(5, 28, "nLED3");
NexNumber nLED4 = NexNumber(5, 29, "nLED4");

//Page6
NexButton bHomePID = NexButton(6, 1, "bHomePID");
NexButton bAutotune = NexButton(6, 2, "bAutotune");
NexText tP = NexText(6, 3, "tP");
NexText tI = NexText(6, 4, "tI");
NexText tD = NexText(6, 5, "tD");

//Page7
NexButton bReset = NexButton(7, 1, "bReset");

//Page8
NexButton bHomeCredits = NexButton(8, 1, "bHomeCredits");

//page9
NexButton bEnter = NexButton(9, 2, "bEnter");

char buffer[10] = {0};

NexTouch *nex_listen_list[] =
{
    &bSettings, &bOnOff,

    &bPreSet1, &bPreSet2, &bPreSet3, &bPreSet4, &bPreSet5, &bPreSet6, &bTempSetup,
    &bTimerSetup, &bLEDSetup, &bPIDSetup, &bHomeSet, &bCredits, &bUpdate,

    &bHomeTemp, &bPreheat,

    &bHomeTimer,

    &bLED1, &bLED2, &bLED3, &bLED4, &bHomeLED,

    &bHomePID, &bAutotune,

    &bReset,

    &bHomeCredits,

    &bEnter,
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
    bOnOff.Get_background_crop_picc(&picNum);
     if(picNum == 1) {
      picNum = 2;
      uvFurnaceStateMachine.transitionTo(runState);

     } else if (picNum == 1 && myBoolean.preheat == 1) {
       picNum = 2;
       uvFurnaceStateMachine.transitionTo(preheatState);
     } else {
       picNum = 1;
       uvFurnaceStateMachine.transitionTo(offState);
     }
     bOnOff.Set_background_crop_picc(picNum);
     sendCommand("ref bOnOff");
}

//End Page1

//Page2
void bPreSet1PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet1.Get_background_crop_picc(&picNum);
  if(picNum == 3) {
    picNum = 5;
    turnOffPresetButtons();
    myBoolean.didReadConfig = readConfiguration(CONFIG_preset1);
    if (!myBoolean.didReadConfig) {
      DEBUG_PRINTLN(F("Error reading config"));
      return;
    }
    myBoolean.preset1 = 1;

  } else if(picNum == 5) {
      picNum = 3;
      myBoolean.preset1 = 0;

  }
    //DEBUG_PRINTLN(picNum);

    bPreSet1.Set_background_crop_picc(picNum);
    sendCommand("ref 0");
}

void bPreSet2PopCallback(void *ptr)
{
   uint32_t picNum = 0;
  bPreSet2.Get_background_crop_picc(&picNum);
  if(picNum == 3) {
      picNum = 5;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset2);
      myBoolean.preset2 = 1;

    } else if(picNum == 5) {
      picNum = 3;
      myBoolean.preset2 = 0;

    }
    //DEBUG_PRINTLN(picNum);
    bPreSet2.Set_background_crop_picc(picNum);
    sendCommand("ref bPreSet2");
}

void bPreSet3PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet3.Get_background_crop_picc(&picNum);
  if(picNum == 3) {
      picNum = 5;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset3);
      myBoolean.preset3 = 1;

    } else if(picNum == 5) {
      picNum = 3;
      myBoolean.preset3 = 0;

    }
    //DEBUG_PRINTLN(picNum);
    bPreSet3.Set_background_crop_picc(picNum);
    sendCommand("ref bPreSet3");}

void bPreSet4PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet4.Get_background_crop_picc(&picNum);
  if(picNum == 3) {
      picNum = 5;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset4);
      myBoolean.preset4 = 1;

    } else if(picNum == 5) {
      picNum = 3;
      myBoolean.preset4 = 0;

    }
    //DEBUG_PRINTLN(picNum);
    bPreSet4.Set_background_crop_picc(picNum);
    sendCommand("ref bPreSet4");}

void bPreSet5PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet5.Get_background_crop_picc(&picNum);
  if(picNum == 3) {
      picNum = 5;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset5);
      myBoolean.preset5 = 1;


    } else if(picNum == 5) {
      picNum = 3;
      myBoolean.preset5 = 0;
    }
    //DEBUG_PRINTLN(picNum);
    bPreSet5.Set_background_crop_picc(picNum);
    sendCommand("ref bPreSet5");}

void bPreSet6PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bPreSet6.Get_background_crop_picc(&picNum);
  if(picNum == 3) {
      picNum = 5;
      turnOffPresetButtons();
      myBoolean.didReadConfig = readConfiguration(CONFIG_preset6);
      myBoolean.preset6 = 1;

    } else if(picNum == 5) {
      picNum = 3;
      myBoolean.preset6 = 0;

    }
    //DEBUG_PRINTLN(picNum);
    bPreSet6.Set_background_crop_picc(picNum);
    sendCommand("ref bPreSet6");
}

void turnOffPresetButtons(){
    bPreSet1.Set_background_crop_picc(3);
    bPreSet2.Set_background_crop_picc(3);
    bPreSet3.Set_background_crop_picc(3);
    bPreSet4.Set_background_crop_picc(3);
    bPreSet5.Set_background_crop_picc(3);
    bPreSet6.Set_background_crop_picc(3);

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

void bUpdatePopCallback(void *ptr)
{
  selSD();
  SD.end();
  DEBUG_PRINTLN(F("updating..."));
  NexUpload nex_download("furnace.tft", 4, 115200);
  nex_download.upload();
  delay(500);
  soft_restart();
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
void bHomeTempPopCallback(void *ptr)
{
    uint32_t number;
    dbSerialPrintln("bTempPlus1PopCallback");

    nTempSetup.getValue(&number);
    Setpoint = number;

    DEBUG_PRINTLN(Setpoint);

    uvFurnaceStateMachine.transitionTo(settingsState);
}

void bPreheatPopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bPreheat.Get_background_crop_picc(&picNum);
    DEBUG_PRINTLN(picNum);
    if(picNum == 6) {
      picNum = 8;

      myBoolean.preheat = 1;

    } else if(picNum == 8) {
      picNum = 6;

      myBoolean.preheat = 0;
    }
    DEBUG_PRINTLN(myBoolean.preheat);
    bPreheat.Set_background_crop_picc(picNum);
    sendCommand("ref bPreheat");
}
//End Page3

//Page4
void bHomeTimerPopCallback(void *ptr)
{
  nOvenMinuteT.getValue(&minutes_oven);
  nOvenHourT.getValue(&hours_oven);
  uvFurnaceStateMachine.transitionTo(settingsState);
}
//End Page4

//Page5
void bLED1PopCallback(void *ptr)
{
  uint32_t picNum = 0;
  bLED1.Get_background_crop_picc(&picNum);
  if(picNum == 11) {
      picNum = 13;
      myBoolean.bLED1State = true;

    } else if(picNum == 13) {
      picNum = 11;
      myBoolean.bLED1State = false;
    }
    //DEBUG_PRINTLN(picNum);
    bLED1.Set_background_crop_picc(picNum);
    sendCommand("ref bLED1");
}



void bLED2PopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bLED2.Get_background_crop_picc(&picNum);
     if(picNum == 11) {
      picNum = 13;
      myBoolean.bLED2State = true;
    } else if(picNum == 13) {
      picNum = 11;
      myBoolean.bLED2State = false;
    }
    //DEBUG_PRINTLN(picNum);
    bLED2.Set_background_crop_picc(picNum);
    sendCommand("ref bLED2");
}

void bLED3PopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bLED3.Get_background_crop_picc(&picNum);
    if(picNum == 11) {
      picNum = 13;
      myBoolean.bLED3State = true;
    } else if(picNum == 13) {
      picNum = 11;
      myBoolean.bLED3State = false;
    }
    //DEBUG_PRINTLN(picNum);
    bLED3.Set_background_crop_picc(picNum);
    sendCommand("ref bLED3");
}

void bLED4PopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bLED4.Get_background_crop_picc(&picNum);
    if(picNum == 11) {
      picNum = 13;
      myBoolean.bLED4State = true;
    } else if(picNum == 13) {
      picNum = 11;
      myBoolean.bLED4State = false;
    }
    //DEBUG_PRINTLN(picNum);
    bLED4.Set_background_crop_picc(picNum);
    sendCommand("ref bLED4");
}

void bHomeLEDPopCallback(void *ptr)
{
    nLED1.getValue(&LED1_intensity);
    nLED2.getValue(&LED2_intensity);
    nLED3.getValue(&LED3_intensity);
    nLED4.getValue(&LED4_intensity);

    if(myBoolean.bLED1State == true){
       LED1_mapped = map(LED1_intensity, 0, 100, 0, 255);
    }else{
      LED1_intensity = 0;
    }

    if(myBoolean.bLED2State == true){
       LED2_mapped = map(LED2_intensity, 0, 100, 0, 255);
    }else{
      LED2_intensity = 0;
    }

    if(myBoolean.bLED3State == true){
       LED3_mapped = map(LED3_intensity, 0, 100, 0, 255);
    }else{
      LED3_intensity = 0;
    }

    if(myBoolean.bLED4State == true){
       LED4_mapped = map(LED4_intensity, 0, 100, 0, 255);
    }else{
      LED4_intensity = 0;
    }

    DEBUG_PRINTLN(LED1_mapped);
    DEBUG_PRINTLN(LED2_mapped);
    DEBUG_PRINTLN(LED3_mapped);
    DEBUG_PRINTLN(LED4_mapped);

    uvFurnaceStateMachine.transitionTo(settingsState);
}
//End Page5

//Page6
void bHomePIDPopCallback(void *ptr)
{
  memset(buffer, 0, sizeof(buffer));
  tP.getText(buffer, sizeof(buffer));
  Kp = atof(buffer);
  DEBUG_PRINTLN(Kp);
  memset(buffer, 0, sizeof(buffer));
  tI.getText(buffer, sizeof(buffer));
  Ki = atof(buffer);
  DEBUG_PRINTLN(Ki);
  memset(buffer, 0, sizeof(buffer));
  tD.getText(buffer, sizeof(buffer));
  Kd = atof(buffer);
  DEBUG_PRINTLN(Kd);

  SaveParameters();

  uvFurnaceStateMachine.transitionTo(settingsState);
}

void bAutotunePopCallback(void *ptr)
{
  StartAutoTune();
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

//page9
void bEnterPopCallback(void *ptr)
{

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
 IO mapping
*******************************************************************************/
void setup() {
  // Initialize LEDs:
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  controlLEDs(0, 0, 0, 0);
  // Initialize Relay Control:
  pinMode(RelayPin, OUTPUT);    // Output mode to drive relay
  digitalWrite(RelayPin, LOW);  // make sure it is off to start

  //reset samples array to default so we fill it up with new samples
  uint8_t i;
  for (i=0; i<NUMBER_OF_SAMPLES; i++) {
    temperatureSamples[i] = DUMMY;
  }

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
  myBoolean.bLED4State = false;

  myBoolean.didReadConfig = false;

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
  bUpdate.attachPop(bUpdatePopCallback, &bUpdate);

  //Page3
  bHomeTemp.attachPop(bHomeTempPopCallback, &bHomeTemp);
  bPreheat.attachPop(bPreheatPopCallback, &bPreheat);

  //Page4
  bHomeTimer.attachPop(bHomeTimerPopCallback, &bHomeTimer);

  //Page5
  bLED1.attachPop(bLED1PopCallback, &bLED1);
  bLED2.attachPop(bLED2PopCallback, &bLED2);
  bLED3.attachPop(bLED3PopCallback, &bLED3);
  bLED4.attachPop(bLED4PopCallback, &bLED4);
  bHomeLED.attachPop(bHomeLEDPopCallback, &bHomeLED);

  //Page6
  bHomePID.attachPop(bHomePIDPopCallback, &bHomePID);
  bAutotune.attachPop(bAutotunePopCallback, &bAutotune);

  //Page7
  bReset.attachPop(bResetPopCallback, &bReset);

  //Page8
  bHomeCredits.attachPop(bHomeCreditsPopCallback, &bHomeCredits);

  //page9
  bEnter.attachPop(bEnterPopCallback, &bEnter);

  //declare and init pins

  //Disable the default square wave of the SQW pin.
  RTC.squareWave(SQWAVE_NONE);

  pinMode(LEDlight, OUTPUT);
  digitalWrite(LEDlight, 0);

  pinMode(reedSwitch, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(reedSwitch), furnaceDoor, RISING);

  pinMode(onOffButton, OUTPUT);
  digitalWrite(onOffButton, onOffState);

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
    // ToDo: disable reading preset from sd card
  } else{
    DEBUG_PRINTLN(F("card initialized."));
  }

  selETH();
  #ifdef USE_Static_IP
  DEBUG_PRINTLN(F("using static IP..."));
  if(Ethernet.begin(mac, ip, dnsServer, gateway, subnet == 0)) {
    ethernetAvailable = false;
    DEBUG_PRINTLN(F("Ethernet not available"));
  } else {
    ethernetAvailable = true;
    Udp.begin(localPort);
    DEBUG_PRINTLN(F("IP number assigned by DHCP is "));
    DEBUG_PRINTLN(Ethernet.localIP());
  }
  #else
    DEBUG_PRINTLN(F("using dynamic IP..."));
    if(Ethernet.begin(mac) == 0) {
      ethernetAvailable = false;
      DEBUG_PRINTLN(F("Ethernet not available"));
    } else {
      ethernetAvailable = true;
      Udp.begin(localPort);
      DEBUG_PRINTLN(F("IP number assigned by DHCP is "));
      DEBUG_PRINTLN(Ethernet.localIP());
    }
   #endif

  // Run timer2 interrupt every 15 ms
  TCCR2A = 0;
  TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20;

  //Timer2 Overflow Interrupt Enable
  TIMSK2 |= 1<<TOIE2;

   #ifdef USE_Blynk
    //init Blynk
    if(ethernetAvailable){
      Blynk.begin(auth);
    }
   #endif

  DEBUG_PRINTLN(F("setup ready"));

}

/************************************************
 Timer Interrupt Handler
************************************************/
SIGNAL(TIMER2_OVF_vect)
{
  if (uvFurnaceStateMachine.isInState(offState) == true)
  {
    digitalWrite(RelayPin, LOW);  // make sure relay is off
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
 doorChanged = !doorChanged;
 DEBUG_PRINTLN(F("Furnace Door changed"));
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
     digitalWrite(RelayPin, HIGH);
  }
  else
  {
     digitalWrite(RelayPin, LOW);
  }
}

void loop() {
  nexLoop(nex_listen_list);
  #ifdef USE_Blynk
    //all the Blynk magic happens here
    Blynk.run();
  #endif
  //this function reads the temperature of the MAX31855 Thermocouple Amplifier

  if(doorChanged == true){
    checkDoor();
  }
  doorChanged = false;

  readTemperature();
  readInternalTemperature();
  updateBlynk();
  if (alarmIsrWasCalled){
     if (RTC.alarm(ALARM_1)) {
        DEBUG_PRINTLN("Alarm_1");
        uvFurnaceStateMachine.transitionTo(offState);
     }
     if (RTC.alarm(ALARM_2)){
        DEBUG_PRINTLN("Alarm_2");
     }
     alarmIsrWasCalled = false;
  }

  //this function updates the FSM
  // the FSM is the heart of the UV furnace - all actions are defined by its states
  uvFurnaceStateMachine.update();
}

/*******************************************************************************
 * Function Name  : checkDoor
 * Description    : updates the value of target temperature of the uv furnace
 * Return         : none
 *******************************************************************************/
void checkDoor(){
  if(digitalRead(reedSwitch) == HIGH){
      digitalWrite(LEDlight, 255);
      controlLEDs(0,0,0,0);
      DEBUG_PRINTLN("door open");
  }else if(digitalRead(reedSwitch) == LOW){
      if(uvFurnaceStateMachine.isInState(runState)){
        controlLEDs(LED1_intensity, LED2_intensity, LED3_intensity, LED4_intensity);
      }
      digitalWrite(LEDlight, 0);
      DEBUG_PRINTLN("door closed");
  }
}

/*******************************************************************************
 * Function Name  : updateTemperature
 * Description    : updates the value of target temperature of the uv furnace
 * Return         : none
 *******************************************************************************/
void updateTemperature()
{
    if(averageTemperature != lastTemperature && newTemperature == true)  {
      dtostrf(averageTemperature, 5, 1, buffer);
      tTemp.setText(buffer);
      lastTemperature = averageTemperature;
    }
    newTemperature = false;
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
void readTemperature(){
   //time is up? no, then come back later
   if (MAX31855SampleInterval < MAX31855_SAMPLE_INTERVAL) {
    return;
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
   float b0, b1, b2, b3, b4, b5, b6, b7, b8, b9;
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
      return;
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

    uint8_t i;
    for (i=0; i< NUMBER_OF_SAMPLES; i++) {
        //store the sample in the next available 'slot' in the array of samples
        if ( temperatureSamples[i] == DUMMY) {
            temperatureSamples[i] = currentTemperature;
        break;
        }
    }

    //is the samples array full? if not, exit and get a new sample
    if ( temperatureSamples[NUMBER_OF_SAMPLES-1] == DUMMY) {
        return;
    }

    // average all the samples out
    averageTemperature = 0;

    for (i=0; i<NUMBER_OF_SAMPLES; i++) {
        averageTemperature += temperatureSamples[i];
    }
    averageTemperature /= NUMBER_OF_SAMPLES;
    averageTemperature = int((averageTemperature + 0.05) * 10);
    averageTemperature /= 10;

    //reset samples array to default so we fill it up again with new samples
    for (i=0; i<NUMBER_OF_SAMPLES; i++) {
        temperatureSamples[i] = DUMMY;
    }
    DEBUG_PRINTLN(averageTemperature);
    newTemperature = true;
}

/*******************************************************************************
 * Function Name  : readInternalTemperature
 * Description    : reads the temperature of the DS3231
 * Return         : 0
 *******************************************************************************/
void readInternalTemperature(){
  if(DS3231TempInterval < DS3231_TEMP_INTERVAL) {
    return;
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
void setDS3231Alarm(byte minutes, byte hours) {

  tmElements_t tm;
  RTC.read(tm);

  dayAlarm = 0;
  hourAlarm = 0;
  minuteAlarm = 0;
  secondAlarm = 0;

  if(tm.Minute + minutes >= 60){
    hours += 1;
    minuteAlarm = tm.Minute + minutes - 60;
  } else {
    minuteAlarm = tm.Minute + minutes;
  }

  if(hours + tm.Hour >= 24)
  {
    dayAlarm = tm.Day + 1;
    hourAlarm = hours + tm.Hour - 24;
  } else
    {
      dayAlarm = tm.Day;
      hourAlarm = hours + tm.Hour;
    }

  DEBUG_PRINTLN(F("Alarm"));
  DEBUG_PRINTLN(secondAlarm);
  DEBUG_PRINTLN(minuteAlarm);
  DEBUG_PRINTLN(hourAlarm);

  RTC.setAlarm(ALM1_MATCH_HOURS, secondAlarm, minuteAlarm, hourAlarm, 1);
  RTC.alarm(ALARM_1);
  RTC.alarmInterrupt(ALARM_1, true);

  return;
}

/*******************************************************************************
 * Function Name  : updateBlynk
 * Description    : updates the blynk app
 * Return         : 0
*******************************************************************************/
void updateBlynk(){
   if (BlynkInterval < BLYNK_INTERVAL) {
    return;
   }
   //DEBUG_PRINTLN(F("updating Blynk"));
   Blynk.virtualWrite(V0, averageTemperature);
   Blynk.virtualWrite(V1, Setpoint);
   Blynk.virtualWrite(V2, LED1_intensity);
   Blynk.virtualWrite(V3, LED2_intensity);
   Blynk.virtualWrite(V4, LED3_intensity);
   Blynk.virtualWrite(V6, LED4_intensity);
   if(uvFurnaceStateMachine.isInState(runState) || uvFurnaceStateMachine.isInState(preheatState)){
    Blynk.virtualWrite(V5, 1);
   }else if(uvFurnaceStateMachine.isInState(offState)){
    Blynk.virtualWrite(V5, 0);
   }
   Blynk.syncVirtual(V9);
   Blynk.syncVirtual(V10);
   if(uvFurnaceStateMachine.isInState(errorState)){
      Blynk.setProperty(V14, "color", BLYNK_RED);
      Blynk.virtualWrite(V14, BLYNK_RED);
      Blynk.virtualWrite(V14, 255);
   } else{
      Blynk.setProperty(V14, "color", BLYNK_GREEN);
      Blynk.virtualWrite(V14, BLYNK_GREEN);
      Blynk.virtualWrite(V14, 255);
   }
   BlynkInterval = 0;
}

/*******************************************************************************
 * Function Name  : SaveParameters
 * Description    : Save any parameter changes to EEPROM
 * Return         : 0
*******************************************************************************/
void SaveParameters()
{
   DEBUG_PRINTLN(F("Save any parameter changes to EEPROM"));
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
   DEBUG_PRINTLN(Kp);
   DEBUG_PRINTLN(Ki);
   DEBUG_PRINTLN(Kd);

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
   //DEBUG_PRINTLN(F("EEPROM_readDouble"));

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
    Input = averageTemperature;

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
 * Function Name  : controlLEDs
 * Description    : sets the intensity of the UV LEDs
 * Return         : 0
 *******************************************************************************/
void controlLEDs(byte intensity1, byte intensity2, byte intensity3, byte intensity4){
  if(myBoolean.bLED1State == true){
    cLED1.Set_background_crop_picc(1);
    analogWrite(LED1, intensity1);
    //DEBUG_PRINTLN(intensity1);
  }else {
    analogWrite(LED1, 0);
    cLED1.Set_background_crop_picc(2);
  }
  if(myBoolean.bLED2State == true){
    cLED2.Set_background_crop_picc(1);
    analogWrite(LED2, intensity2);
    //DEBUG_PRINTLN(intensity2);
  }else {
    analogWrite(LED2, 0);
    cLED2.Set_background_crop_picc(2);
  }
  if(myBoolean.bLED3State == true){
    cLED3.Set_background_crop_picc(1);
    analogWrite(LED3, intensity3);
    //DEBUG_PRINTLN(intensity3);
  }else {
    analogWrite(LED3, 0);
    cLED3.Set_background_crop_picc(2);
  }
  if(myBoolean.bLED4State == true){
    cLED4.Set_background_crop_picc(1);
    analogWrite(LED4, intensity4);
    //DEBUG_PRINTLN(intensity3);
  }else {
    analogWrite(LED4, 0);
    cLED4.Set_background_crop_picc(2);
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
  sChart.addValue(0, averageTemperature);
  sChart.addValue(1, Setpoint);
  GraphUpdateInterval = 0;
}

void sendToInfluxDB(){
  if(InfluxdbUpdateInterval < INFLUXDB_UPDATE_INTERVAL || ethernetAvailable == false){
    return;
  }
  selETH();
  int c = averageTemperature;
  int d = averageTemperature * 100;
  d = d % 100;

  sprintf(msg, "UV Tset=%d,T=%d.%d,LED1=%lu,LED2=%lu,LED3=%lu,LED4=%lu", int(Setpoint), c, d, LED1_intensity, LED2_intensity, LED3_intensity, LED4_intensity);
  DEBUG_PRINTLN(msg);
  Udp.beginPacket(INFLUXDB_HOST, INFLUXDB_PORT);
  Udp.write(msg);
  Udp.endPacket();
  InfluxdbUpdateInterval = 0;
}

/*******************************************************************************
 * Function Name  : sendNTPpacket
 * Description    : send an NTP request to the time server at the given address
 * Return         : timestamp
 *******************************************************************************/
  void sendNTPpacket(char* address){
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
      tmElements_t tm;
      RTC.read(tm);



      if(minuteAlarm < tm.Minute) {
        calcMinutes = 60 - (tm.Minute - minuteAlarm);
        calcHours = hourAlarm - tm.Hour - 1;
      } else {
        calcMinutes = minuteAlarm - tm.Minute;
        calcHours = hourAlarm - tm.Hour;
      }

      nhour_uv.setValue(calcHours);
      nmin_uv.setValue(calcMinutes);

      Blynk.virtualWrite(V12, calcMinutes);
      Blynk.virtualWrite(V13, calcHours);

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

      myBoolean.bLED3State = cfg.getBooleanValue();
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

      LED3_intensity = cfg.getIntValue();
      DEBUG_PRINT(F("Read LED3_intensity: "));
      DEBUG_PRINTLN(LED3_intensity);

    } else if (cfg.nameIs("LED4_intensity")) {

      LED4_intensity = cfg.getIntValue();
      DEBUG_PRINT(F("Read LED4_intensity: "));
      DEBUG_PRINTLN(LED4_intensity);

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

BLYNK_WRITE(V9){
   pushNotification = param.asInt();
   //DEBUG_PRINT(F("push notification: "));
   //DEBUG_PRINTLN(pushNotification);
}
BLYNK_WRITE(V10){
   emailNotification = param.asInt();
   //DEBUG_PRINT(F("Email notification: "));
   //DEBUG_PRINTLN(emailNotification);
}

BLYNK_WRITE(V11){
  twitterNotification = param.asInt();
}

void notifyUser(String message){
  if(pushNotification == 1){
     Blynk.notify(message);
  }
  if(emailNotification == 1){
     Blynk.email("UV furnace", message);
  }
  if(twitterNotification == 1){
     Blynk.tweet(message);
  }
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
  if(ethernetAvailable){
    selETH();
    sendNTPpacket(timeServer); // send an NTP packet to a time server

    // wait to see if a reply is available
    delay(1000);
    if ( Udp.parsePacket() ) {
      // We've received a packet, read the data from it
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, extract the two words:

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

      setTime(epoch);
      utc = now();
      local = myTZ.toLocal(utc, &tcr);
      RTC.set(local);                     //set the RTC from the system time
    }
    setSyncProvider(RTC.get);   // the function to get the time from the RTC
    if(timeStatus()!= timeSet){
        DEBUG_PRINTLN(F("Unable to sync with the RTC"));
    }else{
        DEBUG_PRINTLN(F("RTC has set the system time"));
    }
  }

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
      bPreSet1.Set_background_crop_picc(4);
  }
  if(myBoolean.preset2 == 1){
      bPreSet2.Set_background_crop_picc(4);
  }
  if(myBoolean.preset3 == 1){
      bPreSet3.Set_background_crop_picc(4);
  }
  if(myBoolean.preset4 == 1){
      bPreSet4.Set_background_crop_picc(4);
  }
  if(myBoolean.preset5 == 1){
      bPreSet5.Set_background_crop_picc(4);
  }
  if(myBoolean.preset6 == 1){
      bPreSet6.Set_background_crop_picc(4);
  }

  sendCommand("ref 0");
  selETH();
  Blynk.setProperty(V14, "color", "BLYNK_GREEN");
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

  if(myBoolean.bLED1State == true){
    bLED1.Set_background_crop_picc(13);
  }else{
    bLED1.Set_background_crop_picc(11);
  }
  if(myBoolean.bLED2State == true){
    bLED2.Set_background_crop_picc(13);
  }else{
    bLED2.Set_background_crop_picc(11);
  }
  if(myBoolean.bLED3State == true){
    bLED3.Set_background_crop_picc(13);
  }else{
    bLED3.Set_background_crop_picc(11);
  }
  if(myBoolean.bLED4State == true){
    bLED4.Set_background_crop_picc(13);
  }else{
    bLED4.Set_background_crop_picc(11);
  }
  sendCommand("ref 0");

  nLED1.setValue(LED1_intensity);
  nLED2.setValue(LED2_intensity);
  nLED3.setValue(LED3_intensity);
  nLED4.setValue(LED4_intensity);

  selETH();
  Blynk.setProperty(V14, "color", "BLYNK_GREEN");
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

  nTempSetup.setValue(int(Setpoint));
  if(myBoolean.preheat == true){
    bPreheat.Set_background_crop_picc(8);
  }else{
    bPreheat.Set_background_crop_picc(6);
  }

  sendCommand("ref 0");
}

void setTempUpdateFunction(){
  //DEBUG_PRINTLN(F("setTempUpdate"));

}
void setTempExitFunction(){
  DEBUG_PRINTLN(F("setTempExit"));
}

void setTimerEnterFunction(){
  DEBUG_PRINTLN(F("setTimerEnter"));
  page4.show();
  nOvenMinuteT.setValue(minutes_oven);
  nOvenHourT.setValue(hours_oven);
  nLEDsMinuteT.setValue(minutes_LED);
  nLEDsHourT.setValue(hours_LED);

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
  LoadParameters();
  //tP.setText("567");
  tP.setText(dtostrf(Kp, 4, 2, buffer));
  tI.setText(dtostrf(Ki, 4, 2, buffer));
  tD.setText(dtostrf(Kd, 4, 2, buffer));

  //nP.setValue(int(Kp));
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

   controlLEDs(LED1_intensity, LED2_intensity, LED3_intensity, LED4_intensity);

   //turn the PID on
   myPID.SetMode(AUTOMATIC);
   windowStartTime = millis();

   SaveParameters();
   myPID.SetTunings(Kp,Ki,Kd);

   selETH();
   Udp.begin(INFLUXDB_PORT);
   InfluxdbUpdateInterval = 0;
}

void runUpdateFunction(){
   //DEBUG_PRINTLN(F("runUpdate"));

   DoControl();
   updateGraph();
   updateTemperature();
   fadePowerLED();
   refreshCountdown();

   #ifdef USE_InfluxDB
       sendToInfluxDB();
   #endif
}

void runExitFunction(){
  DEBUG_PRINTLN(F("runExit"));

  minutes_oven = 0;
  hours_oven = 0;

  notifyUser("Curing finished");
}

void errorEnterFunction(){
  DEBUG_PRINTLN(F("errorEnter"));
  myPID.SetMode(MANUAL);
  //Turn off LEDs
  controlLEDs(0, 0, 0, 0);
  digitalWrite(RelayPin, LOW);  // make sure it is off
  RTC.alarm(ALARM_1);
  RTC.alarmInterrupt(ALARM_1, false);
  selETH();
  notifyUser("Error occured");
  Blynk.setProperty(V14, "color", "BLYNK_RED");
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
    nhour_uv.setValue(hours_oven);
    nmin_uv.setValue(minutes_oven);
    nSetpoint.setValue(int(Setpoint));
    sendCommand("ref 0");
    myPID.SetMode(MANUAL);
    controlLEDs(0, 0, 0, 0);
    digitalWrite(RelayPin, LOW);  // make sure it is off
    RTC.alarmInterrupt(ALARM_1, false);

    if(myBoolean.preheat == 1){
      cPreheat.Set_background_crop_picc(1);
    } else{
        cPreheat.Set_background_crop_picc(2);
    }
}

void offUpdateFunction(){
    //DEBUG_PRINTLN(F("offUpdate"));
    updateTemperature();
}

void offExitFunction(){

}

void preheatEnterFunction(){
   DEBUG_PRINTLN(F("preheatEnter"));

    //turn the PID on
   myPID.SetMode(AUTOMATIC);
   windowStartTime = millis();

   SaveParameters();
   myPID.SetTunings(Kp,Ki,Kd);

   tToast.setText("preheating");
   tToast.Set_background_crop_picc(1);

   selETH();
   Udp.begin(INFLUXDB_PORT);
   InfluxdbUpdateInterval = 0;
}

void preheatUpdateFunction(){
    //DEBUG_PRINTLN(F("preheatUpdate"));
   DoControl();
   updateGraph();
   updateTemperature();
   fadePowerLED();
   refreshCountdown();
   #ifdef USE_InfluxDB
       sendToInfluxDB();
   #endif

   if(averageTemperature >= (Setpoint * 0.975) && averageTemperature <= (Setpoint * 1.025)){
          uvFurnaceStateMachine.transitionTo(runState);

          notifyUser("Preheating done!");
   }
}

void preheatExitFunction(){
  tToast.setText("");
  tToast.Set_background_crop_picc(2);
}
