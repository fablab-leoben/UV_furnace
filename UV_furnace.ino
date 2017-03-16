/* UV furnace                                                          *
 *                                                                      *
 * Copyright 2017 Thomas Rockenbauer, Fablab Leoben,                    *
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
#include <Nextion.h> //Library for LCD touch screen
#include <SPI.h>
#include "SD.h"
#include <EEPROM.h> // So we can save and retrieve settings
#include <Adafruit_MAX31855.h>
#include <elapsedMillis.h>
#include <FiniteStateMachine.h>
#include <SDConfigFile.h>
#include "NexUpload.h"
#include <SoftReset.h>
#include <SimpleTimer.h>
#include <Timer5.h>
#include <Adafruit_NeoPixel.h>

#define APP_NAME "UV furnace"
const char VERSION[] = "0.2";

uint32_t oldhours = 0;
uint32_t oldmins = 0;

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

// FSM compatibility with Arduino IDE 1.6.9+
void dummy(){}

/*******************************************************************************
 initialize FSM states with proper enter, update and exit functions
*******************************************************************************/
State dummyState = State(dummy);
State initState = State( initEnterFunction, initUpdateFunction, initExitFunction );
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

#define NEOPIN 13
#define BRIGHTNESS 64 // set max brightness 0-255

// Pattern types supported:
enum  pattern { NONE, RAINBOW_CYCLE, THEATER_CHASE, COLOR_WIPE, SCANNER, FADE };
// Patern directions supported:
enum  direction { FORWARD, REVERSE };
Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, NEOPIN, NEO_GRB + NEO_KHZ800); // strip object

// Output relay
#define RelayPin 32

// Reed switch
#define reedSwitch 19
volatile boolean doorChanged;

// ON/OFF Button LED
#define powerButton 12

// Ethernet
#define W5500_CS  10

// SD card
#define SDCARD_CS  4

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

volatile unsigned long onTime = 0;

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
 time settings variables for heating and leds
************************************************/
uint32_t hours_oven = 0;
uint32_t minutes_oven = 0;

uint32_t hours_LED = 0;
uint32_t minutes_LED = 0;

/************************************************
 chart definitions for Nextion 4,3" display
************************************************/


/*******************************************************************************
 MAX31855 Thermocouple Amplifier
 Creating a thermocouple instance with software SPI on any three
 digital IO pins.
*******************************************************************************/
SimpleTimer TempTimer;

//Adafruit_MAX31855 thermocouple(CLK, CS, DO);
#define cs_MAX31855   47
Adafruit_MAX31855 thermocouple(cs_MAX31855);
float currentTemperature;
float lastTemperature;
bool newTemperature = false;

#define inputOffset 0

/*******************************************************************************
 Power LED
*******************************************************************************/
#define POWERLED_BLINK_INTERVAL   500
elapsedMillis POWERLEDBlinkInterval;
boolean powerState = HIGH;

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
#define BLYNK_INTERVAL   33333
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
File dataFile;
#define SD_CARD_SAMPLE_INTERVAL   1000
elapsedMillis sdCard;

/*******************************************************************************
 Display
*******************************************************************************/
char buffer[10] = {0};
char errorMessage[40] = {0};

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
     if(picNum == 1 && myBoolean.preheat == 0) {
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
    Blynk.virtualWrite(V1, Setpoint);
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
  DEBUG_PRINTLN(minutes_oven);
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

    Blynk.virtualWrite(V2, LED1_intensity);
    Blynk.virtualWrite(V3, LED2_intensity);
    Blynk.virtualWrite(V4, LED3_intensity);
    Blynk.virtualWrite(V5, LED4_intensity);

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
  delay(500);
  soft_restart();
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
  digitalWrite(W5500_CS, LOW);
  digitalWrite(cs_MAX31855, HIGH);
}

/*******************************************************************************
 * Function Name  : selSD
 * Description    : selects the SD card to make communication possible
 * Return         : 0
 *******************************************************************************/
void selSD() {
  digitalWrite(W5500_CS, HIGH);
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
  digitalWrite(W5500_CS, HIGH);
  digitalWrite(SDCARD_CS, HIGH);
  digitalWrite(cs_MAX31855, LOW);
}

SimpleTimer timer;

uint32_t CountdownRemain;
uint32_t OldCountdownRemain = 0;

void setup() {
  // Initialize LEDs:
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  controlLEDs(0, 0, 0, 0);
  // Initialize Relay Control:
  pinMode(RelayPin, OUTPUT);    // Output mode to drive relay
  digitalWrite(RelayPin, LOW);  // make sure it is off to start

  //declare and init pins
  pinMode(LEDlight, OUTPUT);
  digitalWrite(LEDlight, 0);

  pinMode(reedSwitch, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(reedSwitch), furnaceDoor, CHANGE);

  pinMode(powerButton, OUTPUT);
  digitalWrite(powerButton, powerState);

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

  // Initialize all the pixelStrips
  Ring1.begin();
  // Kick off a pattern
  Ring1.TheaterChase(Ring1.Color(255,255,0), Ring1.Color(0,0,50), 100);
}

void initDisplay()
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

// Define the function which will handle the notifications (interrupts)
ISR(timer5Event)
{
  // Reset Timer1 (resetTimer1 should be the first operation for better timer precision)
  resetTimer5();
  // For a smaller and faster code, the line above could safely be replaced with a call
  // to the function resetTimer1Unsafe() as, despite its name, it IS safe to call
  // that function in here (interrupts are disabled)

  // Make sure to do your work as fast as possible, since interrupts are automatically
  // disabled when this event happens (refer to interrupts() and noInterrupts() for
  // more information on that)
  CountdownRemain--;
}

/************************************************
 Reed Switch Interrupt Handler
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

  if(doorChanged == true){
    checkDoor();
    doorChanged = false;
  }

  TempTimer.run();
  updateBlynk();

  timer.run();

  //this function updates the FSM
  // the FSM is the heart of the UV furnace - all actions are defined by its states
  uvFurnaceStateMachine.update();

  // Update the rings.
  Ring1.Update();
}

void CountdownTimerFunction() {
  DEBUG_PRINTLN(CountdownRemain);
  CountdownShowFormatted(CountdownRemain);

  if (!CountdownRemain) { // check if CountdownRemain == 0/FALSE/LOW
    //timer.disable(CountdownTimer); // if 0 stop timer
    Blynk.virtualWrite(V5, LOW); // reset START/STOP button status
    Blynk.virtualWrite(V12, "Curing finished");
    uvFurnaceStateMachine.transitionTo(offState);
  } else {

  }
}

uint32_t days = 0;
uint32_t hours = 0;
uint32_t mins = 0;
uint32_t secs = 0;

void CountdownShowFormatted(uint32_t seconds) {
  secs = seconds; // set the seconds remaining
  mins = secs / 60; //convert seconds to minutes
  hours = mins / 60; //convert minutes to hours
  days = hours / 24; //convert hours to days
  secs = secs - (mins * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
  mins = mins - (hours * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours = hours - (days * 24); //subtract the coverted hours to days in order to display 23 hours max
  const String secs_o = ":";
  const String mins_o = ":";
  const String hours_o = ":";

  if (secs < 10) {
    secs_o = ":0";
  }
  if (mins < 10) {
    mins_o = ":0";
  }
  if (hours < 10) {
    hours_o = ":0";
  }
}

  //display
  strip.show();

  if(hours != oldhours){
    //DEBUG_PRINTLN(hours);
    nhour_uv.setValue(hours);
    oldhours = hours;
  }

  if(mins != oldmins){
    //DEBUG_PRINTLN(mins);
    nmin_uv.setValue(mins);
    Blynk.virtualWrite(V12, days + hours_o + hours + mins_o + mins);// + secs_o + secs);
    oldmins = mins;
  }
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
      //DEBUG_PRINTLN("door open");
  }else if(digitalRead(reedSwitch) == LOW){
      if(uvFurnaceStateMachine.isInState(runState)){
        controlLEDs(LED1_intensity, LED2_intensity, LED3_intensity, LED4_intensity);
      }
      digitalWrite(LEDlight, 0);
  }
}

/*******************************************************************************
 * Function Name  : updateTemperature
 * Description    : updates the value of target temperature of the uv furnace
 * Return         : none
 *******************************************************************************/
void updateTemperature()
{
    if(currentTemperature != lastTemperature && newTemperature == true)  {
      dtostrf(currentTemperature, 5, 1, buffer);
      tTemp.setText(buffer);
      Blynk.virtualWrite(V0, currentTemperature);
      Blynk.virtualWrite(V1, Setpoint);

      if(uvFurnaceStateMachine.isInState(runState) || uvFurnaceStateMachine.isInState(preheatState))
      {
        Blynk.virtualWrite(V20, currentTemperature);
        Blynk.virtualWrite(V21, Setpoint);
      }
      lastTemperature = currentTemperature;
      newTemperature = false;
    }
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
   selMAX31855();

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

      currentTemperature *= 10;
      currentTemperature = (int)currentTemperature;
      currentTemperature /= 10;

    DEBUG_PRINTLN(currentTemperature, 1);
    newTemperature = true;
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

   Blynk.syncVirtual(V9, V10, V11);

      //Blynk.setProperty(V14, "color", BLYNK_GREEN);
      //Blynk.virtualWrite(V14, BLYNK_GREEN);
      //Blynk.virtualWrite(V14, 255);
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
   for (uint16_t i = 0; i < sizeof(value); i++)
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
  sChart.addValue(0, currentTemperature);
  sChart.addValue(1, Setpoint);

  GraphUpdateInterval = 0;
}

void sendToInfluxDB(){
  if(InfluxdbUpdateInterval < INFLUXDB_UPDATE_INTERVAL || ethernetAvailable == false){
    return;
  }
  selETH();
  int c = currentTemperature;
  int d = currentTemperature * 100;
  d = d % 100;

  sprintf(msg, "UV Tset=%d,T=%d.%d,LED1=%lu,LED2=%lu,LED3=%lu,LED4=%lu", int(Setpoint), c, d, LED1_intensity, LED2_intensity, LED3_intensity, LED4_intensity);
  DEBUG_PRINTLN(msg);
  Udp.beginPacket(INFLUXDB_HOST, INFLUXDB_PORT);
  Udp.write(msg);
  Udp.endPacket();
  InfluxdbUpdateInterval = 0;
}

/*******************************************************************************
 * Function Name  : blinkPowerLED
 * Description    : blinks the LED of the Power button
 * Return         : 0
 *******************************************************************************/
void blinkPowerLED(){
  if (POWERLEDBlinkInterval > POWERLED_BLINK_INTERVAL){
    !powerState;
    digitalWrite(powerButton, powerState);
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
   analogWrite(powerButton, fadeValue);           // sets the value (range from 0 to 255)
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

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
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

  initDisplay();

  //start the timer of this cycle
  initTimer = 0;
  page0.show();
  tVersion.setText(VERSION);

  // Initialize the PID and related variables
  LoadParameters();
  myPID.SetTunings(Kp,Ki,Kd);
  myPID.SetSampleTime(1000);
  myPID.SetOutputLimits(0, WindowSize);

  //Initializing Chip Select pin for MAX31855
  pinMode(cs_MAX31855, OUTPUT);

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
    uvFurnaceStateMachine.transitionTo(errorState);

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

   Blynk.virtualWrite(V5, 0);
   Blynk.virtualWrite(V12, "0:00:00");

  DEBUG_PRINTLN(F("setup ready"));

  TempTimer.setInterval(1000, readTemperature);

  DEBUG_PRINTLN(F("starting Timer"));
  startTimer5(1000000L);
  pauseTimer5();
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
   CountdownRemain = hours_oven * 60 * 60 + minutes_oven * 60;
   DEBUG_PRINTLN(CountdownRemain);

   controlLEDs(LED1_intensity, LED2_intensity, LED3_intensity, LED4_intensity);

   //turn the PID on
   myPID.SetMode(AUTOMATIC);
   windowStartTime = millis();
   SaveParameters();
   myPID.SetTunings(Kp,Ki,Kd);

   resumeTimer5();

   //selETH();
   //Udp.begin(INFLUXDB_PORT);
   //InfluxdbUpdateInterval = 0;

   Blynk.virtualWrite(V5, 1);
}

void runUpdateFunction(){
   //DEBUG_PRINTLN(F("runUpdate"));
   DoControl();
   updateGraph();
   updateTemperature();
   fadePowerLED();

   if(OldCountdownRemain != CountdownRemain){
     CountdownTimerFunction();
     OldCountdownRemain = CountdownRemain;
   }

   #ifdef USE_InfluxDB
       sendToInfluxDB();
   #endif
}

void runExitFunction(){
  DEBUG_PRINTLN(F("runExit"));
  pauseTimer5();

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
  selETH();
  notifyUser("Error occured");

  Blynk.setProperty(V14, "color", BLYNK_RED);
  Blynk.virtualWrite(V14, BLYNK_RED);
  Blynk.virtualWrite(V14, 255);
}

void errorUpdateFunction(){
  DEBUG_PRINTLN(F("errorUpdate"));
  blinkPowerLED();
}

void errorExitFunction(){
  //DEBUG_PRINTLN(F("errorExit"));
}

void offEnterFunction(){
    pauseTimer5();

    DEBUG_PRINTLN(F("offEnter"));
    page1.show();
    nhour_uv.setValue(hours_oven);
    nmin_uv.setValue(minutes_oven);
    nSetpoint.setValue(int(Setpoint));
    myPID.SetMode(MANUAL);
    controlLEDs(0, 0, 0, 0);
    digitalWrite(RelayPin, LOW);  // make sure it is off

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

   Blynk.virtualWrite(V5, 1);
}

void preheatUpdateFunction(){
    //DEBUG_PRINTLN(F("preheatUpdate"));
   DoControl();
   updateGraph();
   updateTemperature();
   fadePowerLED();
   #ifdef USE_InfluxDB
       sendToInfluxDB();
   #endif

   if(currentTemperature >= (Setpoint * 0.975) && currentTemperature <= (Setpoint * 1.025)){
          uvFurnaceStateMachine.transitionTo(runState);
          notifyUser("Preheating done!");
   }
}

void preheatExitFunction(){
  tToast.setText("");
  tToast.Set_background_crop_picc(2);
}


// NeoPattern Class - derived from the Adafruit_NeoPixel class
class NeoPatterns : public Adafruit_NeoPixel
{
    public:

    // Member Variables:
    pattern  ActivePattern;  // which pattern is running
    direction Direction;     // direction to run the pattern

    unsigned long Interval;   // milliseconds between updates
    unsigned long lastUpdate; // last update of position

    uint32_t Color1, Color2;  // What colors are in use
    uint16_t TotalSteps;  // total number of steps in the pattern
    uint16_t Index;  // current step within the pattern

    void (*OnComplete)();  // Callback on completion of pattern

    // Constructor - calls base-class constructor to initialize strip
    NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t type, void (*callback)())
    :Adafruit_NeoPixel(pixels, pin, type)
    {
        OnComplete = callback;
    }

    // Update the pattern
    void Update()
    {
        if((millis() - lastUpdate) > Interval) // time to update
        {
            lastUpdate = millis();
            switch(ActivePattern)
            {
                case RAINBOW_CYCLE:
                    RainbowCycleUpdate();
                    break;
                case THEATER_CHASE:
                    TheaterChaseUpdate();
                    break;
                case COLOR_WIPE:
                    ColorWipeUpdate();
                    break;
                case SCANNER:
                    ScannerUpdate();
                    break;
                case FADE:
                    FadeUpdate();
                    break;
                default:
                    break;
            }
        }
    }

    // Increment the Index and reset at the end
    void Increment()
    {
        if (Direction == FORWARD)
        {
           Index++;
           if (Index >= TotalSteps)
            {
                Index = 0;
                if (OnComplete != NULL)
                {
                    OnComplete(); // call the comlpetion callback
                }
            }
        }
        else // Direction == REVERSE
        {
            --Index;
            if (Index <= 0)
            {
                Index = TotalSteps-1;
                if (OnComplete != NULL)
                {
                    OnComplete(); // call the comlpetion callback
                }
            }
        }
    }

    // Reverse pattern direction
    void Reverse()
    {
        if (Direction == FORWARD)
        {
            Direction = REVERSE;
            Index = TotalSteps-1;
        }
        else
        {
            Direction = FORWARD;
            Index = 0;
        }
    }

    // Initialize for a RainbowCycle
    void RainbowCycle(uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = RAINBOW_CYCLE;
        Interval = interval;
        TotalSteps = 255;
        Index = 0;
        Direction = dir;
    }

    // Update the Rainbow Cycle Pattern
    void RainbowCycleUpdate()
    {
        for(int i=0; i< numPixels(); i++)
        {
            setPixelColor(i, Wheel(((i * 256 / numPixels()) + Index) & 255));
        }
        show();
        Increment();
    }

    // Initialize for a Theater Chase
    void TheaterChase(uint32_t color1, uint32_t color2, uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = THEATER_CHASE;
        Interval = interval;
        TotalSteps = numPixels();
        Color1 = color1;
        Color2 = color2;
        Index = 0;
        Direction = dir;
   }

    // Update the Theater Chase Pattern
    void TheaterChaseUpdate()
    {
        for(int i=0; i< numPixels(); i++)
        {
            if ((i + Index) % 3 == 0)
            {
                setPixelColor(i, Color1);
            }
            else
            {
                setPixelColor(i, Color2);
            }
        }
        show();
        Increment();
    }

    // Initialize for a ColorWipe
    void ColorWipe(uint32_t color, uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = COLOR_WIPE;
        Interval = interval;
        TotalSteps = numPixels();
        Color1 = color;
        Index = 0;
        Direction = dir;
    }

    // Update the Color Wipe Pattern
    void ColorWipeUpdate()
    {
        setPixelColor(Index, Color1);
        show();
        Increment();
    }

    // Initialize for a SCANNNER
    void Scanner(uint32_t color1, uint8_t interval)
    {
        ActivePattern = SCANNER;
        Interval = interval;
        TotalSteps = (numPixels() - 1) * 2;
        Color1 = color1;
        Index = 0;
    }

    // Update the Scanner Pattern
    void ScannerUpdate()
    {
        for (int i = 0; i < numPixels(); i++)
        {
            if (i == Index)  // Scan Pixel to the right
            {
                 setPixelColor(i, Color1);
            }
            else if (i == TotalSteps - Index) // Scan Pixel to the left
            {
                 setPixelColor(i, Color1);
            }
            else // Fading tail
            {
                 setPixelColor(i, DimColor(getPixelColor(i)));
            }
        }
        show();
        Increment();
    }

    // Initialize for a Fade
    void Fade(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = FADE;
        Interval = interval;
        TotalSteps = steps;
        Color1 = color1;
        Color2 = color2;
        Index = 0;
        Direction = dir;
    }

    // Update the Fade Pattern
    void FadeUpdate()
    {
        // Calculate linear interpolation between Color1 and Color2
        // Optimise order of operations to minimize truncation error
        uint8_t red = ((Red(Color1) * (TotalSteps - Index)) + (Red(Color2) * Index)) / TotalSteps;
        uint8_t green = ((Green(Color1) * (TotalSteps - Index)) + (Green(Color2) * Index)) / TotalSteps;
        uint8_t blue = ((Blue(Color1) * (TotalSteps - Index)) + (Blue(Color2) * Index)) / TotalSteps;

        ColorSet(Color(red, green, blue));
        show();
        Increment();
    }

    // Calculate 50% dimmed version of a color (used by ScannerUpdate)
    uint32_t DimColor(uint32_t color)
    {
        // Shift R, G and B components one bit to the right
        uint32_t dimColor = Color(Red(color) >> 1, Green(color) >> 1, Blue(color) >> 1);
        return dimColor;
    }

    // Set all pixels to a color (synchronously)
    void ColorSet(uint32_t color)
    {
        for (int i = 0; i < numPixels(); i++)
        {
            setPixelColor(i, color);
        }
        show();
    }

    // Returns the Red component of a 32-bit color
    uint8_t Red(uint32_t color)
    {
        return (color >> 16) & 0xFF;
    }

    // Returns the Green component of a 32-bit color
    uint8_t Green(uint32_t color)
    {
        return (color >> 8) & 0xFF;
    }

    // Returns the Blue component of a 32-bit color
    uint8_t Blue(uint32_t color)
    {
        return color & 0xFF;
    }

    // Input a value 0 to 255 to get a color value.
    // The colours are a transition r - g - b - back to r.
    uint32_t Wheel(byte WheelPos)
    {
        WheelPos = 255 - WheelPos;
        if(WheelPos < 85)
        {
            return Color(255 - WheelPos * 3, 0, WheelPos * 3);
        }
        else if(WheelPos < 170)
        {
            WheelPos -= 85;
            return Color(0, WheelPos * 3, 255 - WheelPos * 3);
        }
        else
        {
            WheelPos -= 170;
            return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
        }
    }
};

void Ring1Complete();

// Define some NeoPatterns for the two rings and the stick
//  as well as some completion routines
NeoPatterns Ring1(24, 5, NEO_GRB + NEO_KHZ800, &Ring1Complete);
