#include <Arduino.h>

/************************************************
  Debug
************************************************/
#define DebugStream    Serial
#define UV_FURNACE_DEBUG //uncomment to turn debugging off

#ifdef UV_FURNACE_DEBUG
// need to do some debugging...
  #define DEBUG_PRINT(...)    DebugStream.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...)    DebugStream.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

//#define USE_InfluxDB

#define USE_Blynk

//#define USE_Static_IP
uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
#ifdef USE_Static_IP
//Ethernet shield MAC address
uint8_t ip[] = { 10, 0, 0, 177 };
uint8_t dnsServer[] = {10, 0, 0, 1};
uint8_t gateway[] = {10, 0, 0, 1};
uint8_t subnet[] = {255, 255, 255, 0};
#endif
