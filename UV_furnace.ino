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
#define DEBUG

#ifdef DEBUG
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

#define APP_NAME "UV furnace"
String VERSION = "Version 0.01";

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

//compatibility with Arduino IDE 1.6.9
void dummy(){}

/*******************************************************************************
 initialize FSM states with proper enter, update and exit functions
*******************************************************************************/
State dummyState = State(dummy);
State initState = State( initEnterFunction, initUpdateFunction, initExitFunction );
State idleState = State( idleEnterFunction, idleUpdateFunction, idleExitFunction );
State settingsState = State( settingsEnterFunction, settingsUpdateFunction, settingsExitFunction );


//initialize state machine, start in state: Idle
FSM uvFurnaceStateMachine = FSM(initState);

//milliseconds for the init cycle, so everything gets stabilized
#define INIT_TIMEOUT 10000
elapsedMillis initTimer;




/*******************************************************************************
 IO mapping
*******************************************************************************/

void setup() {

  #ifdef DEBUG
    Serial.begin(115200);
  #endif
  Serial2.begin(9600);
  
  //declare and init pins
  
  // put your setup code here, to run once:

  DEBUG_PRINTLN(F("setup"));

}

void loop() {
  //this function reads the temperature of the MAX31855 Thermocouple Amplifier
  readTemperature();


  //this function updates the FSM
  // the FSM is the heart of the UV furnace - all actions are defined by its states
  uvFurnaceStateMachine.update();

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
     DEBUG_PRINTLN(F("readTemperature"));

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

     DEBUG_PRINTLN(F(currentTemperature));
     DEBUG_PRINTLN(F(" °C"));
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
}
void idleUpdateFunction(){
  DEBUG_PRINTLN(F("idleUpdate"));
}
void idleExitFunction(){
  DEBUG_PRINTLN(F("idleExit"));
}

void settingsEnterFunction(){
  DEBUG_PRINTLN(F("settingsEnter"));
}
void settingsUpdateFunction(){
  DEBUG_PRINTLN(F("settingsUpdate"));
}
void settingsExitFunction(){
  DEBUG_PRINTLN(F("settingsExit"));
}
 
