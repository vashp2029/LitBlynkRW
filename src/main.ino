//BEFOREUPLOAD Generate new keys for Blynk auth (SetSpecific.h) once project is
//BEFOREUPLOAD complete since this is going to be a public repo

//BEFOREUPLOAD Comment out the following unless testing.
#define MYDEBUG
#define BLYNK_PRINT Serial

//BEFOREUPLOAD Uncomment the appropriate ESP and comment out the rest.
#define BED
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




////////////////////////////////////////////////////////////////////////////////
//CONFIG                                                                      //
////////////////////////////////////////////////////////////////////////////////
#define BLYNKAUTH		"48acb8cd05e645eebdef6d873f4e2262"

#define SWITCHPIN 		V0 //On/off switch
#define AUTOSWITCHPIN 	V1 //On/off for auto-turn-on LEDs at specified time
#define AUTOTIMEPIN 	V2 //Time to auto-turn-on LEDs
#define BRIGHTNESSPIN 	V3 //Brightness slider (range 0-255 in Blynk app)
#define MICPIN 			V4 //Mic sensitivity (range 0-255 in Blynk app)
#define SPEEDPIN 		V5 //Animation speed (range 0-255 in Blynk app)
#define EFFECTPIN 		V6 //Effect selection drop-down menu
#define RGBPIN 			V7 //ZeRGBa (set to "merge" in Blynk app)
#define ESPTIMEPIN 		V8 //Update the Blynk app with current ESP time
#define GROUPPIN		V9 //Select LED group to command individually

#define DATAPIN			D5
#define COLORORDER		RGB
#define CHIPSET			WS2812B
#define MAXVOLTAGE		5
#define MAXAMPS			900 //Units in milliamps

#define ON 				1
#define OFF 			0




////////////////////////////////////////////////////////////////////////////////
//GLOBAL VARIABLES                                                            //
////////////////////////////////////////////////////////////////////////////////
bool onOff; //True = LEDs on
bool autoOnOff; //True = LEDs will automatically turn on at specified time
bool autoOnOffTime; //True = Time set for LEDs to turn on automatically
bool effectChange; //True = Effect was changed since last loop
bool stopCurrentEffect; //True = Stop current effect to load new paramenters

uint8_t selectedEffect = 0; //Store currently selected effect from Blynk
uint8_t brightness = 0; //Range is 0-255
uint8_t micSensitivity = 0; //Range is 0-255
uint8_t animationSpeed = 0; //Range is 0-255
uint8_t currentRed = 0; //Range is 0-255
uint8_t currentGreen = 0; //Range is 0-255
uint8_t currentBlue = 0; //Range is 0-255

WidgetRTC clock;
WiFiManager wifiManager;
struct CRGB leds[NUMLEDS];

//When a command needs to be sent to only one set of LEDs, this variable is set
//to the corresponding LED group from "SetSpecific.h." For global commands to
//all the sets simultaneously, set "selectedLedGroup" equal to "LEDGROUP."
uint8_t selectedLedGroup = LEDGROUP;

//This will store strings to be easily called later if needed for something like
//the Blynk.setProperty function to populate a drop down menu in Blynk.
BlynkParamAllocated effectsList(512);
BlynkParamAllocated ledGroupsList(128);




////////////////////////////////////////////////////////////////////////////////
//BLYNK INPUT FUNCTIONS                                                       //
////////////////////////////////////////////////////////////////////////////////
BLYNK_WRITE(SWITCHPIN){
	DEBUG_PRINTLN("Toggled 'SWITCHPIN' (V1).");


	//If this ESP is the one that is selected in the App (or all are selected),
	//toggle variable 'onOff' to match the state of this pin. Then, set the
	//value of 'stopCurrentEffect' to 'true' to kick the program out of the
	//currently running effect loop.
	if(selectedLedGroup == LEDGROUP){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		onOff = param.asInt();
		stopCurrentEffect = true;

		DEBUG_PRINT("Variable 'onOff' toggled to: ");
		DEBUG_PRINTLN(onOff);

		//If the LEDs are turned on manually, turn of the auto-turn-on function.
		if(onOff){
			Blynk.virtualWrite(AUTOSWITCHPIN, OFF);
			autoOnOff = false;

			DEBUG_PRINT("Variable 'autoOnOff' set to: ");
			DEBUG_PRINTLN(autoOnOff);
		}
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}


	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(AUTOTIMEPIN){}
BLYNK_WRITE(BRIGHTNESSPIN){}
BLYNK_WRITE(MICPIN){}
BLYNK_WRITE(SPEEDPIN){}
BLYNK_WRITE(EFFECTPIN){}
BLYNK_WRITE(RGBPIN){}
BLYNK_WRITE(ESPTIMEPIN){}
BLYNK_WRITE(GROUPPIN){}




////////////////////////////////////////////////////////////////////////////////
//SETUP FUNCTIONS                                                             //
////////////////////////////////////////////////////////////////////////////////
//BEFOREUPLOAD Make sure all the effects are listed in the function below or
//BEFOREUPLOAD they will not show up in the Blynk app.
void addEffectsToList(){
	DEBUG_PRINTLN("Populating 'effectsList' with effects.");

	effectsList.add("First effect");
	effectsList.add("Second effect");

	DEBUG_PRINTLN("Finished populating 'effectList'.");
}

//BEFOREUPLOAD Make sure all the LED groups are listed in the function below or
//BEFOREUPLOAD they will not show up in the Blynk app.
void addLedGroupsToList(){
	DEBUG_PRINTLN("Populating 'ledGroupList' with effects.");

	ledGroupsList.add("All");
	ledGroupsList.add("Bed");
	ledGroupsList.add("Couch");
	ledGroupsList.add("TV");
	ledGroupsList.add("TV Stand");
	ledGroupsList.add("Wall Desk");
	ledGroupsList.add("Glass Desk");

	DEBUG_PRINTLN("Finished populating 'ledGroupList'.");
}

void setupLeds(){
	DEBUG_PRINTLN("Setting up LEDs.");

	animationSpeed 			= 0;
	currentRed		= 255;
	currentGreen	= 255;
	currentBlue		= 255;
	onOff 			= true;

	FastLED.addLeds<CHIPSET, DATAPIN, COLORORDER>(leds, NUMLEDS);
	FastLED.setCorrection(TypicalLEDStrip);

	set_max_power_in_volts_and_milliamps(MAXVOLTAGE, MAXAMPS);

	DEBUG_PRINTLN("LED setup complete.");
}

void setupWiFi(){
	DEBUG_PRINT("Setting up the Wifi connection.");

	//If there are no recognized networks to connect to, this will create an
	//ad-hoc network with the name of the current LED group. You can connect
	//to this network to setup the WiFi further.
	wifiManager.autoConnect(SENSORNAME);

	while(WiFi.status() != WL_CONNECTED){
		DEBUG_PRINT(".");
		delay(500);
	}

	DEBUG_PRINT("Wifi setup is complete, the current IP Address is: ");
	DEBUG_PRINTLN(WiFi.localIP());
}

void setupBlynk(){
	DEBUG_PRINT("Setting up connection to Blynk servers...");

	//Make sure to use Blynk.config instead of Blynk.begin, since the Wifi
	//is being managed by WiFiManager.
	Blynk.config(BLYNKAUTH);

	while(!Blynk.connect()){
		DEBUG_PRINT(".");
		delay(500);
	}

	clock.begin();

	//Once connected to blynk, download the current settings (this is in case
	//there was an unexpected disconnect--the settings on the ESP will revert
	//back to where they were before the disconnect).
	DEBUG_PRINTLN("\nSuccessfully connected to Blynk, now syncing values.");

	Blynk.setProperty(GROUPPIN, "labels", ledGroupsList);
	Blynk.setProperty(EFFECTPIN, "labels", effectsList);

	DEBUG_PRINTLN("Sent group and effect list to Blynk. Check the drop-down menus.");

	Blynk.syncAll();

	DEBUG_PRINTLN("Finished syncing values.");

}



void setup(){
	DEBUG_BEGIN(115200);

	setupWiFi();

	//Only setup everything else if the WiFi connects.
	if(WiFi.status() == WL_CONNECTED){
		addLedGroupsToList();
		addEffectsToList();
		setupLeds();
		setupBlynk();
	}
}

void loop(){
	Blynk.run();
}