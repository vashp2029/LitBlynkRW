//BEFOREUPLOAD Generate new keys for Blynk auth (SetSpecific.h) once project is
//BEFOREUPLOAD complete since this is going to be a public repo

//BEFOREUPLOAD Comment out the following unless testing.
#define MYDEBUG
#define BLYNK_PRINT Serial

//BEFOREUPLOAD Uncomment the appropriate ESP and comment out the rest.
#define TESTESP
//#define BED
//#define COUCH
//#define TV
//#define TVSTAND
//#define WALLDESK
//#define GLASSDESK

#define FASTLED_INTERRUPT_RETRY_COUNT 0

////////////////////////////////////////////////////////////////////////////////
//INCLUDES                                                                    //
////////////////////////////////////////////////////////////////////////////////
#include "ESP8266WiFi.h"
#include "ESP8266mDNS.h"
#include "WiFiManager.h"
#include "BlynkSimpleEsp8266.h"
#include "TimeLib.h"
#include "WidgetRTC.h"
#include "FastLED.h"
#include "Debug.h"
#include "SetSpecific.h"

void setup(){}
void loop(){}