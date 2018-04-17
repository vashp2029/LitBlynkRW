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
#include "WS2812FX.h"
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
#define SENSITIVITYPIN 	V4 			//Mic sensitivity (range 0-255 in Blynk app)
#define SPEEDPIN 		V5 			//Animation speed (range 0-255 in Blynk app)
#define EFFECTPIN 		V6 			//Effect selection drop-down menu
#define SOUNDEFFECTPIN	V7 			//Sound effect selection drop-down menu
#define RGBPIN 			V8 			//ZeRGBa (set to "merge" in Blynk app)
#define ESPTIMEPIN 		V9 			//Update the Blynk app with current ESP time
#define AUTOTIMEPIN 	V10			//Time to auto-turn-on LEDs
#define PINCOUNT		11 			//No. of pins for custom syncAll (to prevent crashing)

#define MICPIN			A0
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

#define DCOFFSET		512			//Offset the waveform above or below the zero line
#define SOUNDSAMPLES	64			//Number of sound samples to collect for analysis (more samples = smoother)




////////////////////////////////////////////////////////////////////////////////
//GLOBAL VARIABLES                                                            //
////////////////////////////////////////////////////////////////////////////////
bool onOff 					= false; 	//True = LEDs on
bool autoOnOff				= false;	//True = LEDs will automatically turn on at specified time
bool stopCurrentEffect 		= false;	//True = Stop current effect to load new paramenters
bool firstRun 				= true;		//True = This is the first run of a newly selected effect
bool globalLedSelection 	= true; 	//True = All led groups selected
bool dst 					= true;		//True = Daylight savings time is in effect

uint8_t selectedEffect 		= 0; 		//Store currently selected effect from Blynk
uint8_t selectedSoundEffect = 0;		//Store currently selected effect from Blynk
uint8_t selectedLedGroup 	= LEDGROUP;	//Tells all ESPs which group should respond to command
uint8_t brightness 			= 0; 		//Range is 0-255
uint8_t micSensitivity 		= 0; 		//Range is 0-255
uint8_t animationSpeed 		= 0; 		//Range is 0-255
uint8_t currentRed 			= 0; 		//Range is 0-255
uint8_t currentGreen 		= 0; 		//Range is 0-255
uint8_t currentBlue 		= 0; 		//Range is 0-255

unsigned long currentMillis;			//Time elapsed since program began
unsigned long currentTimeInEpoch; 		//Current time in EPOCH
unsigned long midnightInEpoch; 			//EPOCH time at prior midnight
unsigned long sunriseInEpoch; 			//Today's sunrise time in EPOCH
unsigned long sunsetInEpoch; 			//Today's sunset time in EPOCH
unsigned long commandTimeInEpoch; 		//Time a command is received in EPOCH
unsigned long autoStartTimeInEpoch; 	//Time to auto turn on LEDs in EPOCH
unsigned long autoStopTimeInEpoch; 		//Time to auto turn off LEDs in EPOCH

WidgetRTC clock;
BlynkTimer updateTime;
Dusk2Dawn atlantaSun(LATITUDE, LONGITUDE, TIMEZONE);
WiFiManager wifiManager;
struct CRGB leds[NUMLEDS];
CRGB currentRGB(currentRed, currentGreen, currentBlue);
WS2812FX ws2812fx = WS2812FX(NUMLEDS, DATAPIN, NEO_GRB + NEO_KHZ800);

//This will store strings to be easily called later if needed for something like
//the Blynk.setProperty function to populate a drop down menu in Blynk.
BlynkParamAllocated effectsList(512);
BlynkParamAllocated soundEffectsList(512);
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
		firstRun = true;

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
		firstRun = true;

		DEBUG_PRINT("Variable 'brightness' set to: ");
		DEBUG_PRINTLN(brightness);
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(SENSITIVITYPIN){
	DEBUG_PRINT("Moved 'SENSITIVITYPIN' slider (V4) to: ");
	DEBUG_PRINTLN(param.asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		//Inverse the value of micSensitivity so sliding the bar up decreases the threshold
		//required to activate the mic.
		micSensitivity = 255 - param.asInt();
		firstRun = true;

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
		firstRun = true;

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
		firstRun = true;

		Blynk.virtualWrite(SOUNDEFFECTPIN, 0);
		selectedSoundEffect = 0;

		DEBUG_PRINT("Variable 'selectedEffect' set to: ");
		DEBUG_PRINTLN(selectedEffect);

		DEBUG_PRINT("Variable 'selectedSoundEffect' set to: ");
		DEBUG_PRINTLN(selectedSoundEffect);

		DEBUG_PRINT("Variable 'stopCurrentEffect' set to: ");
		DEBUG_PRINTLN(stopCurrentEffect);
	}

	else{
		DEBUG_PRINTLN("Not accepting command: this group is not selected.");
	}
}

BLYNK_WRITE(SOUNDEFFECTPIN){
	DEBUG_PRINT("Selected new effect from drop-down menu (V6), effect number: ");
	DEBUG_PRINTLN(param.asInt());

	if(selectedLedGroup == LEDGROUP || globalLedSelection == true){
		DEBUG_PRINTLN("Accepting command: all groups selected or this group selected.");

		selectedSoundEffect = param.asInt();
		stopCurrentEffect = true;
		firstRun = true;

		Blynk.virtualWrite(EFFECTPIN, 0);
		selectedEffect = 0;

		DEBUG_PRINT("Variable 'selectedSoundEffect' set to: ");
		DEBUG_PRINTLN(selectedSoundEffect);

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

		currentRGB = (currentRed, currentGreen, currentBlue);

		firstRun = true;

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
//BEFOREUPLOAD Make sure all effects and groups are listed below.
void populateLists(){
	DEBUG_PRINTLN("Populating 'effectsList' with effects.");

	//My Effects
	effectsList.add("Sunrise/Sunset");
	effectsList.add("Thisothereffect");

	//WS2812FX Effects
	effectsList.add("Solid Color");
	effectsList.add("Blink");
	effectsList.add("Color Wipe Random");
	effectsList.add("Rainbow");
	effectsList.add("Rainbow Cycle");
	effectsList.add("Scan");
	effectsList.add("Dual Scan");
	effectsList.add("Fade");
	effectsList.add("Chase Color");
	effectsList.add("Chase Random");
	effectsList.add("Chase Rainbow");
	effectsList.add("Chase Blackout Rainbow");
	effectsList.add("Running Lights");
	effectsList.add("Running Color");
	effectsList.add("Larson Scanner");
	effectsList.add("Comet");
	effectsList.add("Fireworks");
	effectsList.add("Christmas");
	effectsList.add("Halloween");

	DEBUG_PRINTLN("Finished populating 'effectList'.");



	DEBUG_PRINTLN("Populating 'effectsList' with effects.");

	soundEffectsList.add("Testing");
	soundEffectsList.add("No Effect");

	DEBUG_PRINTLN("Finished populating 'effectList'.");



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

	//WS2812FX
	ws2812fx.init();

	//FastLED
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

void blynkSlowSync(){
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
	Blynk.setProperty(SOUNDEFFECTPIN, "labels", soundEffectsList);

	DEBUG_PRINTLN("Sent group and effect list to Blynk. Check the drop-down menus.");
	DEBUG_PRINTLN("Syncing values of all pins from Blynk. This will take a few seconds.");

	blynkSlowSync();

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
		populateLists();
		setupLeds();
		setupBlynk();
		getSunTime();
	}
}


void loop(){
	Blynk.run();
	updateTime.run();
	currentMillis = millis();

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
		if(selectedEffect != 0){
			//This will call the function for the selected effect. If the
			//selected effect does not have a case statement below, it means that the
			//effect is a WS2812FX effect and it will, by default, call the ws2812fxImplementer
			//function where there is another switch statement to choose effects from the
			//WS2812FX library.
			switch(selectedEffect){
				case 1:
					//sunriseSunset());
					break;
				case 2:
					//someEffect();
					break;
				default:
					ws2812fxImplementer();
					break;
			}
		}

		//If a sound-reactive effect is selected, run soundmems to read microphone
		//before going into the effect function to react.
		else if(selectedSoundEffect != 0){
			soundmems();

			switch(selectedSoundEffect){
				case 1:
					//someEffect();
					break;
				case 2:
					//someEffect();
					break;
			}
		}
	}

	else if(!onOff){
		ledsOff();
	}
}




////////////////////////////////////////////////////////////////////////////////
//EFFECT SUPPORT FUNCTIONS                                                    //
////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR MY EFFECTS ///////////////////////////////////////////////
//This function must be called in ALL effects for the program to keep running.
//This is what will populate the LEDs with the given effect so don't use
//FastLED.show() in the effect functions themselves. This is also going to keep
//the time updated and keep Blynk running to accept commands.
void fastLedImplementer(){
	FastLED.show();

	//If a command is received which requires a change of effect, kill the current
	//effect.
	if(stopCurrentEffect != false){
		stopCurrentEffect = false;
		FastLED.clear();
		return;
	}
}

// IMPLEMENTATION FOR WS2812FX EFFECTS /////////////////////////////////////////
void ws2812fxImplementer(){
	if(firstRun == true){
		firstRun = false;

		ws2812fx.setBrightness(brightness);
		ws2812fx.setColor(currentRed, currentGreen, currentBlue);
		ws2812fx.setSpeed(map(animationSpeed, 0, 255, 65535, 10));

		//BEFOREUPLOAD The cases should start from where the switch statement in the main
		//BEFOREUPLOAD loop left off for 'selectedEffect'. For example, if you have case
		//BEFOREUPLOAD 1 and case 2 defined in the main loop as FastLED functions, start
		//BEFOREUPLOAD these at case 3.
		switch(selectedEffect){
			case 3:		ws2812fx.setMode(FX_MODE_STATIC);					break;
			case 4: 	ws2812fx.setMode(FX_MODE_BLINK);					break;
			case 5: 	ws2812fx.setMode(FX_MODE_COLOR_WIPE_RANDOM);		break;
			case 6: 	ws2812fx.setMode(FX_MODE_RAINBOW);					break;
			case 7: 	ws2812fx.setMode(FX_MODE_RAINBOW_CYCLE);			break;
			case 8: 	ws2812fx.setMode(FX_MODE_SCAN);						break;
			case 9: 	ws2812fx.setMode(FX_MODE_DUAL_SCAN);				break;
			case 10: 	ws2812fx.setMode(FX_MODE_FADE);						break;
			case 11: 	ws2812fx.setMode(FX_MODE_CHASE_COLOR);				break;
			case 12: 	ws2812fx.setMode(FX_MODE_CHASE_RANDOM); 			break;
			case 13: 	ws2812fx.setMode(FX_MODE_CHASE_RAINBOW);			break;
			case 14: 	ws2812fx.setMode(FX_MODE_CHASE_BLACKOUT_RAINBOW); 	break;
			case 15: 	ws2812fx.setMode(FX_MODE_RUNNING_LIGHTS);			break;
			case 16: 	ws2812fx.setMode(FX_MODE_RUNNING_COLOR); 			break;
			case 17: 	ws2812fx.setMode(FX_MODE_LARSON_SCANNER); 			break;
			case 18: 	ws2812fx.setMode(FX_MODE_COMET); 					break;
			case 19: 	ws2812fx.setMode(FX_MODE_FIREWORKS_RANDOM); 		break;
			case 20: 	ws2812fx.setMode(FX_MODE_MERRY_CHRISTMAS); 			break;
			case 21: 	ws2812fx.setMode(FX_MODE_HALLOWEEN); 				break;
			default:	return;
		}

		ws2812fx.start();
	}

	if(stopCurrentEffect != false){
		stopCurrentEffect = false;
		return;
	}

	ws2812fx.service();
}

// MIC READING /////////////////////////////////////////////////////////////////
bool samplePeak = false;
int16_t currentSample = 0;
int16_t previousSample = 0;
int16_t sampleArray[SOUNDSAMPLES];
uint16_t sampleSum = 0;
uint16_t sampleAverage = 0;
uint16_t sampleCount = 0;

void soundmems(){
	//Read the current mic value and store it to currentSample.
	currentSample = abs(analogRead(MICPIN) - DCOFFSET);

	//If the current sample of sound is below the threshold required by the sensitivity
	//setting, just set it to 0 and pretend there was no sound.
	if(currentSample < micSensitivity) currentSample = 0;

	//Add the currentSample to the sum of samples and subtract the oldest sample from the array.
	//This will keep a summation of the samples in the array (subtracting the oldest sample
	//which will be removed in the next bit).
	sampleSum = sampleSum + currentSample - sampleArray[sampleCount];
	sampleAverage = sampleSum/SOUNDSAMPLES;

	//Remove the oldest sample from the array and replace it with the new one.
	sampleArray[sampleCount] = currentSample;

	//This will iterate sampleCount up by 1 until it gets to SOUNDSAMPLES, then start over.
	sampleCount = (sampleCount + 1) % SOUNDSAMPLES;

	//If the current sample is larger than the average of the samples and less than the last
	//sample, a local peak has occurred, so set samplePeak to 1.
	if(currentSample > (sampleAverage + micSensitivity) && (currentSample < previousSample)){
		samplePeak = true;
	}

	previousSample = currentSample;

	//Uncomment if you need to see raw mic values.
	//DEBUG_PRINTLN(String("The currentSample is: ") + currentSample + String(" and samplePeak is: ") + samplePeak);
}




////////////////////////////////////////////////////////////////////////////////
//EFFECT FUNCTIONS                                                            //
////////////////////////////////////////////////////////////////////////////////
// LEDS OFF ////////////////////////////////////////////////////////////////////
void ledsOff(){
	FastLED.clear();
	fastLedImplementer();
}