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
#include "Dusk2Dawn.h"
#include "WidgetRTC.h"
#include "FastLED.h"
#include "Debug.h"
#include "SetSpecific.h"




////////////////////////////////////////////////////////////////////////////////
//CONFIG                                                                      //
////////////////////////////////////////////////////////////////////////////////
#define BLYNKAUTH		"48acb8cd05e645eebdef6d873f4e2262"

#define GROUPPIN		V0 			//Select LED group to command individually
#define SWITCHPIN 		V1 			//On/off switch
#define AUTOSWITCHPIN 	V2 			//On/off for auto-turn-on LEDs at specified time
#define BRIGHTNESSPIN 	V3 			//Brightness slider (range 0-255 in Blynk app)
#define MICPIN 			V4 			//Mic sensitivity (range 0-255 in Blynk app)
#define SPEEDPIN 		V5 			//Animation speed (range 0-255 in Blynk app)
#define EFFECTPIN 		V6 			//Effect selection drop-down menu
#define RGBPIN 			V7 			//ZeRGBa (set to "merge" in Blynk app)
#define ESPTIMEPIN 		V8 			//Update the Blynk app with current ESP time
#define AUTOTIMEPIN 	V9 			//Time to auto-turn-on LEDs
#define PINCOUNT		10 			//No. of pins for custom syncAll (to prevent crashing)

#define DATAPIN			D5
#define COLORORDER		GRB
#define CHIPSET			WS2812B
#define MAXVOLTAGE		5
#define MAXAMPS			1200 		//Units in milliamps

#define ON 				1
#define OFF 			0

#define LATITUDE		34.051490 	//For sunrise/sunset functions
#define LONGITUDE		-84.071300 	//For sunrise/sunset functions
#define TIMEZONE		-5 			//For sunrise/sunset functions




////////////////////////////////////////////////////////////////////////////////
//GLOBAL VARIABLES                                                            //
////////////////////////////////////////////////////////////////////////////////
bool onOff; 						//True = LEDs on
bool autoOnOff; 					//True = LEDs will automatically turn on at specified time
bool effectChange; 					//True = Effect was changed since last loop
bool stopCurrentEffect; 			//True = Stop current effect to load new paramenters
bool globalLedSelection; 			//True = all led groups selected
bool dst;							//True = Daylight savings time is in effect

uint8_t selectedEffect = 0; 		//Store currently selected effect from Blynk
uint8_t selectedLedGroup = LEDGROUP;//Tells all ESPs which group should respond to command
uint8_t brightness = 0; 			//Range is 0-255
uint8_t micSensitivity = 0; 		//Range is 0-255
uint8_t animationSpeed = 0; 		//Range is 0-255
uint8_t currentRed = 0; 			//Range is 0-255
uint8_t currentGreen = 0; 			//Range is 0-255
uint8_t currentBlue = 0; 			//Range is 0-255

unsigned long currentTimeInEpoch; 	//Current time in EPOCH
unsigned long midnightInEpoch; 		//EPOCH time at prior midnight
unsigned long sunriseInEpoch; 		//Today's sunrise time in EPOCH
unsigned long sunsetInEpoch; 		//Today's sunset time in EPOCH
unsigned long commandTimeInEpoch; 	//Time a command is received in EPOCH
unsigned long autoStartTimeInEpoch; //Time to auto turn on LEDs in EPOCH
unsigned long autoStopTimeInEpoch; 	//Time to auto turn off LEDs in EPOCH

WidgetRTC clock;
BlynkTimer updateTime;
Dusk2Dawn atlantaSun(LATITUDE, LONGITUDE, TIMEZONE);
WiFiManager wifiManager;
struct CRGB leds[NUMLEDS];

//This will store strings to be easily called later if needed for something like
//the Blynk.setProperty function to populate a drop down menu in Blynk.
BlynkParamAllocated effectsList(512);
BlynkParamAllocated ledGroupsList(128);




////////////////////////////////////////////////////////////////////////////////
//TIME FUNCTIONS                                                              //
////////////////////////////////////////////////////////////////////////////////
void getMyTime(){
	uint8_t myHour 		= hour();
	uint8_t myMinute 	= minute();
	uint8_t mySecond 	= second();

	unsigned long currentTime = (myHour * 3600) + (myMinute * 60) + (mySecond);

	currentTimeInEpoch = now();

	midnightInEpoch = currentTimeInEpoch - currentTime;

	DEBUG_PRINTLN(String("The current time in EPOCH is: ") + currentTimeInEpoch);
	DEBUG_PRINTLN(String("Midnight in EPOCH occured at : ") + midnightInEpoch);
}

void getSunTime(){
	getMyTime();

	uint16_t myYear 	= year();
	uint8_t myMonth 	= month();
	uint8_t myDay	 	= day();

	if(myMonth >= 3 && myMonth <= 10){
		dst = true;
	}

	else{
		dst = false;
	}

	sunriseInEpoch = (atlantaSun.sunrise(myYear, myMonth, myDay, dst) * 60) + midnightInEpoch;
	sunsetInEpoch = (atlantaSun.sunset(myYear, myMonth, myDay, dst) * 60) + midnightInEpoch;

	DEBUG_PRINTLN(String("Sunrise today will occur at (in EPOCH): ") + sunriseInEpoch);
	DEBUG_PRINTLN(String("Sunset today will occur at (in EPOCH): ") + sunsetInEpoch);
}




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

		//When the auto on/off switch is flipped, pull the time for auto start and 
		//auto stop from the 'AUTOTIMEPIN' (Time Input Widget). 
		Blynk.syncVirtual(AUTOTIMEPIN);
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(AUTOTIMEPIN){
	DEBUG_PRINTLN("Set new start/stop time for automatic operation.");

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		getSunTime();

		//Only set the start times and stop times if the auto on/off switch is turned on.
		if(autoOnOff){
			DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

			//The time this command was received in EPOCH.
			commandTimeInEpoch = now();

			DEBUG_PRINT("The command to turn LEDs on/off automatically was recieved at (EPOCH time): ");
			DEBUG_PRINTLN(commandTimeInEpoch);

			//If a start time and stop time are defined, store the time value (in seconds from)
			//midnight in the variables 'autoStartTimeInEpoch' and 'autoStopTimeInEpoch'.
			TimeInputParam t(param);

			if(t.hasStartTime() || t.hasStopTime()){

				//The start and stop times in EPOCH.
				autoStartTimeInEpoch = midnightInEpoch + (t.getStartHour() * 3600) + (t.getStartMinute() * 60);
				autoStopTimeInEpoch = midnightInEpoch + (t.getStopHour() * 3600) + (t.getStopMinute() * 60);

				DEBUG_PRINT("The LEDs are set to automatically turn on at EPOCH time: ");
				DEBUG_PRINTLN(autoStartTimeInEpoch);

				DEBUG_PRINT("The LEDs are set to automatically turn off at EPOCH time: ");
				DEBUG_PRINTLN(autoStopTimeInEpoch);
			}

			//If sunrise/sunset are selected in the Blynk time input widget, figure out when today's
			//sunrise/senset occur, and set autoStartTimeInEpoch and autoStopTimeInEpoch accordingly.
			if(t.isStartSunrise()){
				getSunTime();
				autoStartTimeInEpoch = sunriseInEpoch;

				DEBUG_PRINT("The start time is set to 'sunrise' which will occur at (EPOCH time): ");
				DEBUG_PRINTLN(autoStartTimeInEpoch);
			}

			if(t.isStopSunrise()){
				getSunTime();
				autoStopTimeInEpoch = sunriseInEpoch;

				DEBUG_PRINT("The stop time is set to 'sunrise' which will occur at (EPOCH time): ");
				DEBUG_PRINTLN(autoStopTimeInEpoch);
			}

			if(t.isStartSunset()){
				getSunTime();
				autoStartTimeInEpoch = sunsetInEpoch;

				DEBUG_PRINT("The start time is set to 'sunset' which will occur at (EPOCH time): ");
				DEBUG_PRINTLN(autoStartTimeInEpoch);
			}

			if(t.isStopSunset()){
				getSunTime();
				autoStopTimeInEpoch = sunsetInEpoch;

				DEBUG_PRINT("The stop time is set to 'sunset' which will occur at (EPOCH time): ");
				DEBUG_PRINTLN(autoStopTimeInEpoch);
			}
		}
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(BRIGHTNESSPIN){
	DEBUG_PRINT("Moved 'BRIGHTNESSPIN' slider (V3) to: ");
	DEBUG_PRINTLN(param.asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		brightness = map(param.asInt(), 0, 255, 0, 100);

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

		DEBUG_PRINT("Since a specific group was selected, globalLedSelection was set to: ");
		DEBUG_PRINTLN(globalLedSelection);

		DEBUG_PRINT("The new LED group selection is: ");
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

	effectsList.add("Sunrise/Sunset");
	effectsList.add("Solid Color");

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

	//Every 10 seconds, update the time by running the 'getMyTime' function.
	updateTime.setInterval(10000L, getMyTime);

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




////////////////////////////////////////////////////////////////////////////////
//MAIN PROGRAM                                                                //
////////////////////////////////////////////////////////////////////////////////
void setup(){
	DEBUG_BEGIN(115200);

	setupWiFi();

	//Only setup everything else if the WiFi connects.
	if(WiFi.status() == WL_CONNECTED){
		addLedGroupsToList();
		addEffectsToList();
		setupLeds();
		setupBlynk();
		getSunTime();
	}
}


void loop(){
	Blynk.run();
	updateTime.run();

	stopCurrentEffect = false;

	//If the lights are meant to be turned on/off automatically, run this loop.
	if(autoOnOff){

		//If the auto on/off switch was flipped before the LEDs are meant to be on,
		//start polling for time. Then, when the current time is
		//in between the auto start time and auto stop time, flip the manual
		//'SWITCH' to the 'on' position to turn on LEDs. When the current time passes
		//the auto stop time, flip the 'SWITCH' to 'off'.
		if(commandTimeInEpoch < autoStartTimeInEpoch){
			if(currentTimeInEpoch >= autoStartTimeInEpoch && currentTimeInEpoch <= autoStopTimeInEpoch){
				Blynk.virtualWrite(SWITCHPIN, ON);
			}
			else{
				Blynk.virtualWrite(SWITCHPIN, OFF);
			}
		}

		//If the auto on/off switch was flipped after the LEDs were already meant
		//to be on, then when the current time passes the auto stop time, turn off
		//the LEDs.
		else{
			if(currentTimeInEpoch > autoStopTimeInEpoch){
				Blynk.virtualWrite(SWITCHPIN, OFF);
			}
		}
	}

	//If the lights are turned on, run this loop.
	//BEFOREUPLOAD Make sure all the functions are implemented in this
	//BEFOREUPLOAD switch statement.
	if(onOff){
		switch(selectedEffect){
			case 1:
				sunriseSunset();
				break;
			case 2:
				solidColor();
				break;
		}
	}

	else if(!onOff){
		ledsOff();
	}
}




////////////////////////////////////////////////////////////////////////////////
//EFFECT FUNCTIONS                                                            //
////////////////////////////////////////////////////////////////////////////////
//This function must be called in ALL effects for the program to keep running.
//This is what will populate the LEDs with the given effect so don't use
//FastLED.show() in the effect functions themselves. This is also going to keep
//the time updated and keep Blynk running to accept commands.
void implementer(){
	FastLED.show();
	Blynk.run();
	updateTime.run();

	//If a command is received which requires a change of effect, kill the current
	//effect.
	if(stopCurrentEffect){
		FastLED.clear();
		return;
	}
}

void ledsOff(){
	FastLED.clear();
	implementer();
}

void solidColor(){
	FastLED.setBrightness(brightness);

	CRGB rgbval(currentRed, currentGreen, currentBlue);
	fill_solid(leds, NUMLEDS, rgbval);
	
	implementer();
}

void sunriseSunset(){
	implementer();
}