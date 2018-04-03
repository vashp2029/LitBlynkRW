//Debug utilities from here:
//https://forum.arduino.cc/index.php?topic=64555.0

#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H

#ifdef MYDEBUG
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DEBUG_BEGIN(...) Serial.begin(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
  #define DEBUG_BEGIN(...)
#endif

#endif
