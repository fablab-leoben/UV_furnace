//Library for DS3231 clock
#include <DS3232RTC.h>

//Library for LCD touch screen
#include <Nextion.h>

#include <Time.h>

#include <Ethernet.h>

#include <SPI.h>

#include <SD.h>

// So we can save and retrieve settings
#include <EEPROM.h>

#include <Adafruit_MAX31855.h>

#include <elapsedMillis.h>

#include <FiniteStateMachine.h>

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
String VERSION = "Version 0.01";


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

//initialize state machine, start in state: Idle
FSM uvFurnaceStateMachine = FSM(initState);

//milliseconds for the init cycle, so everything gets stabilized
#define INIT_TIMEOUT 5000
elapsedMillis initTimer;

// ************************************************
// Pin definitions
// ************************************************

// LEDs
byte b0Pin = 13;
byte b1Pin = 12;
byte b2Pin = 11;

// ************************************************
// LED variables
// ************************************************
bool b0State = false;
bool b1State = false;
bool b2State = false;

byte LED1_intensity = 0;
byte LED2_intensity = 0;
byte LED3_intensity = 0;

/************************************************
 PID Variables and constants
************************************************/

//Define Variables we'll be connecting to
double Setpoint;
double Input;
double Output;

// pid tuning parameters
double Kp;
double Ki;
double Kd;

// EEPROM addresses for persisted data
const int SpAddress = 0;
const int KpAddress = 8;
const int KiAddress = 16;
const int KdAddress = 24;


/************************************************
 Timer Variables
************************************************/

byte hourAlarm = 0;
byte minuteAlarm = 0;
byte secondAlarm = 0;


/************************************************
 time settings variables for heating and leds
************************************************/
char temporaer[10] = {0};
byte hours_oven = 0;
byte minutes_oven = 0;

byte hours_led = 0;
byte minutes_led = 0;

// ************************************************
// chart definitions for Nextion 4,3" display
// ************************************************
#define inputOffset 0
double targetTemp = 0;

/*******************************************************************************
 MAX31855 Thermocouple Amplifier
 Creating a thermocouple instance with software SPI on any three
 digital IO pins.
*******************************************************************************/
#define DO   3
#define CS   4
#define CLK  5
#define MAX31855_SAMPLE_INTERVAL   1000    // Sample room temperature every 5 seconds
Adafruit_MAX31855 thermocouple(CLK, CS, DO);
elapsedMillis MAX31855SampleInterval;
float currentTemperature;

/*******************************************************************************
 SD-Card
*******************************************************************************/
const int chipSelect = 53;
File dataFile;

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

char buffer[50] = {0};

NexTouch *nex_listen_list[] = 
{
    &bSettings, &bOnOff,
    
    &bPreSet1, &bPreSet2, &bPreSet3, &bPreSet4, &bPreSet5, &bPreSet6, &bTempSetup, &bTimerSetup, &bLEDSetup, &bPIDSetup, &bHomeSet,
    
    &bTempPlus1, &bTempPlus10, &bTempMinus1, &bTempMinus10, &bHomeTemp, &bPreheat,
    
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
  Serial.println(F("transition to settings")); 
  uvFurnaceStateMachine.transitionTo(settingsState);
}

void bOnOffPopCallback(void *ptr)
{
    uint32_t picNum = 0;
    bOnOff.getPic(&picNum);
     if(picNum == 4) {
      picNum = 5;

      createLogFile();
      writeHeader();
      
      //opState = RUN;
      
      //set alarm
      setDS3231Alarm(minutes_oven, hours_oven);

    } else {
      picNum = 4;

      //opState = OFF;
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
    uvFurnaceStateMachine.transitionTo(idleState);    
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

void bPreheatPopCallback(void *ptr)
{
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

  nexInit();

  #ifdef DEBUG
    Serial.begin(9600);
  #endif
  //Serial2.begin(9600);

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
 
  //declare and init pins
  
  //Disable the default square wave of the SQW pin.
  RTC.squareWave(SQWAVE_NONE);

  DEBUG_PRINTLN(F("setup"));

}

void loop() {
  nexLoop(nex_listen_list);
  
  //this function reads the temperature of the MAX31855 Thermocouple Amplifier
  readTemperature();

  updateTargetTemp();
  
  //this function updates the FSM
  // the FSM is the heart of the UV furnace - all actions are defined by its states
  uvFurnaceStateMachine.update();
}


/*******************************************************************************
 * Function Name  : updateTargetTemp
 * Description    : updates the value of target temperature of the uv furnace
 * Return         : none
 *******************************************************************************/
void updateTargetTemp()
{
    //memset(buffer, 0, sizeof(buffer));
    //itoa(int(currentTemperature), buffer, 10);
    //tTemp.setText(buffer);
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

// ************************************************
// Save any parameter changes to EEPROM
// ************************************************
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

// ************************************************
// Load parameters from EEPROM
// ************************************************
void LoadParameters()
{
   DEBUG_PRINTLN("Load parameters from EEPROM");

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
   DEBUG_PRINTLN(F("EEPROM_writeDouble"));
   
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
}
void initUpdateFunction(){
  DEBUG_PRINTLN("initUpdate");
  //time is up?
  if (initTimer > INIT_TIMEOUT) {
    uvFurnaceStateMachine.transitionTo(idleState);
  }
}
void initExitFunction(){
  DEBUG_PRINTLN(F("initExit"));
  DEBUG_PRINTLN(F("Initialization done"));
}

void idleEnterFunction(){
  DEBUG_PRINTLN(F("idleEnter"));
  page1.show();
  sendCommand("ref 0");
}
void idleUpdateFunction(){
  DEBUG_PRINTLN(F("idleUpdate"));
}
void idleExitFunction(){
  DEBUG_PRINTLN(F("idleExit"));
}

void settingsEnterFunction(){
  DEBUG_PRINTLN(F("settingsEnter"));
  page2.show();
  sendCommand("ref 0");
}
void settingsUpdateFunction(){
  DEBUG_PRINTLN(F("settingsUpdate"));
}
void settingsExitFunction(){
  DEBUG_PRINTLN(F("settingsExit"));
}

void setLEDsEnterFunction(){
  DEBUG_PRINTLN(F("setLEDsEnter"));
}
void setLEDsUpdateFunction(){
  DEBUG_PRINTLN(F("setLEDsUpdate"));
}
void setLEDsExitFunction(){
  DEBUG_PRINTLN(F("setLEDsExit"));
}

void setTempEnterFunction(){
  DEBUG_PRINTLN(F("setTempEnter"));
}
void setTempUpdateFunction(){
  DEBUG_PRINTLN(F("setTempUpdate"));
}
void setTempExitFunction(){
  DEBUG_PRINTLN(F("setTempExit"));
}

void setTimerEnterFunction(){
  DEBUG_PRINTLN(F("setTimerEnter"));
}
void setTimerUpdateFunction(){
  DEBUG_PRINTLN(F("setTimerUpdate"));
}
void setTimerExitFunction(){
  DEBUG_PRINTLN(F("setTimerExit"));
}

void setPIDEnterFunction(){
  DEBUG_PRINTLN(F("setPIDEnter"));
}
void setPIDUpdateFunction(){
  DEBUG_PRINTLN(F("setPIDUpdate"));
}
void setPIDExitFunction(){
  //DEBUG_PRINTLN(F("setPIDExit"));
}
