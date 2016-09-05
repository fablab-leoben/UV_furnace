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

#define InfluxDB

#define USE_Blynk

  


