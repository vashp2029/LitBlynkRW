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
#define PINCOUNT		10 //No. of pins for custom syncAll (to prevent crashing)

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
bool globalLedSelection; //True = all led groups selected

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
//all the sets simultaneously, set 'globalLedSelection' to 'true'.
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
	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
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

BLYNK_WRITE(AUTOSWITCHPIN){
	DEBUG_PRINTLN("Toggled 'AUTOSWITCHPIN' (V2).");

	//If this LED group is selected, toggle the 'autoOnOff' variable to match the state
	//of this pin. Then stop the current effect and turn off the LEDs by setting 'onOff'
	//to false.
	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		autoOnOff = param.asInt();
		stopCurrentEffect = true;

		DEBUG_PRINT("Variable autoOnOff set to: ");
		DEBUG_PRINTLN(autoOnOff);

		//If LEDs are set to be turned on automatically, turn them off for now.
		if(autoOnOff){
			Blynk.virtualWrite(SWITCHPIN, OFF);
			onOff = false;

			DEBUG_PRINT("Variable 'onOff' set to: ");
			DEBUG_PRINTLN(onOff);
		}
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

//FIXIT I want this to automatically turn on the sunrise/sunset effect when the
//FIXIT specified time hits, but can't do that until I write the loop function
//FIXIT so come back and write this code. Keep in mind that once the time is set
//FIXIT the Blynk app doesn't need to be running for this to trigger at the time.

BLYNK_WRITE(AUTOTIMEPIN){
	autoOnOffTime = param.asInt();
}

BLYNK_WRITE(BRIGHTNESSPIN){
	DEBUG_PRINT("Moved 'BRIGHTNESSPIN' slider (V3) to: ");
	DEBUG_PRINTLN(param.asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		brightness = param.asInt();

		DEBUG_PRINT("Variable 'brightness' set to: ");
		DEBUG_PRINTLN(brightness);
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(MICPIN){
	DEBUG_PRINT("Moved 'MICPIN' slider (V4) to: ");
	DEBUG_PRINTLN(param.asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		micSensitivity = param.asInt();

		DEBUG_PRINT("Variable 'micSensitivity' set to: ");
		DEBUG_PRINTLN(micSensitivity);
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(SPEEDPIN){
	DEBUG_PRINT("Moved 'SPEEDPIN' slider (V5) to: ");
	DEBUG_PRINTLN(param.asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		animationSpeed = param.asInt();

		DEBUG_PRINT("Variable 'animationSpeed' set to: ");
		DEBUG_PRINTLN(animationSpeed);
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(EFFECTPIN){
	DEBUG_PRINT("Selected new effect from drop-down menu (V6), effect number: ");
	DEBUG_PRINTLN(param.asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		selectedEffect = param.asInt();
		stopCurrentEffect = true;

		DEBUG_PRINT("Variable 'selectedEffect' set to: ");
		DEBUG_PRINTLN(selectedEffect);

		DEBUG_PRINT("Variable 'stopCurrentEffect' set to: ");
		DEBUG_PRINTLN(stopCurrentEffect);
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(RGBPIN){
	DEBUG_PRINTLN("Changed RGB values on 'RGBPIN' (V7) to: ");
	DEBUG_PRINT("Red: ");
	DEBUG_PRINT(param[0].asInt());
	DEBUG_PRINT(", Green: ");
	DEBUG_PRINT(param[1].asInt());
	DEBUG_PRINT(", Blue: ");
	DEBUG_PRINTLN(param[2].asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		currentRed = param[0].asInt();
		currentGreen = param[1].asInt();
		currentBlue = param[2].asInt();

		stopCurrentEffect = true;

		DEBUG_PRINT("Variable 'stopCurrentEffect' set to: ");
		DEBUG_PRINTLN(stopCurrentEffect);

		DEBUG_PRINTLN("Set variables 'currentRed', 'currentGreen', and 'currentBlue' to: ");
		DEBUG_PRINT("Red: ");
		DEBUG_PRINT(currentRed);
		DEBUG_PRINT(", Green: ");
		DEBUG_PRINT(currentGreen);
		DEBUG_PRINT(", Blue: ");
		DEBUG_PRINTLN(currentBlue);

	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_READ(ESPTIMEPIN){
	//If this group is selected (and global selection is off), update the Blynk app
	//with the current time. In the Blynk app, set this widget to pull an update every
	//'x' seconds rather than 'push'. 
	if(selectedLedGroup == LEDGROUP && globalLedSelection != true){
		DEBUG_PRINTLN("Sending 'currentEspTime' to Blynk since this group or all groups are selected.");
		
		String timeString = String(hour()) + ":" + String(minute()) + ":" +  String(second());
		Blynk.virtualWrite(ESPTIMEPIN, timeString);
	}

	else{
		DEBUG_PRINTLN("Not sending 'currentEspTime' to Blynk since this group is not selected.");
	}
}

BLYNK_WRITE(GROUPPIN){
	//When a new group is selected, let all the ESPs know what the new selection is.
	//Update the variable 'selectedLedGroup' with the new group selection. If the new
	//selection is "1" (which is reserved as the value for "all groups"), set the bool
	//'globalLedSelection' equal to 'true'.
	DEBUG_PRINT("Selected new LED group from drop-down menu (V9), group number: ");
	DEBUG_PRINTLN(param.asInt());

	selectedLedGroup = param.asInt();

	DEBUG_PRINT("Updated variable 'selectedLedGroup; to group number: ");
	DEBUG_PRINTLN(selectedLedGroup);

	if(selectedLedGroup == 1){
		globalLedSelection = true;

		DEBUG_PRINT("Selected all groups, so setting variable 'globalLedSelction' to 'true': ");
		DEBUG_PRINTLN(globalLedSelection);
	}

	else{
		globalLedSelection = false;

		DEBUG_PRINT("Since a specific group was selected, globalLedSelection was set to 'false':");
		DEBUG_PRINTLN(globalLedSelection);

		DEBUG_PRINT("The new LED group selection is the same as before, which was: ");
		DEBUG_PRINTLN(selectedLedGroup);
	}
}




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
	DEBUG_PRINTLN("Syncing values of all pins from Blynk. This will take a few seconds.");

	//If you use a vanilla Blynk.syncAll() function here, the ESP may crash
	//because of the flood of values. This function iterates from 0 to the
	//number of pins as defined in the 'config' section, syncing one pin
	//every 10 milliseconds.
	for(uint8_t i = 0; i <= PINCOUNT; i++){
		Blynk.syncVirtual(i);
		delay(10);
		Blynk.run();

		DEBUG_PRINT("Obtain value for pin V");
		DEBUG_PRINTLN(i);
	}

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