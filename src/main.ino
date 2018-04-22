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

#define DCOFFSET		300			//Offset the waveform above or below the zero line
#define NOISE 			85			//Ambient noise in the room
#define AMPLIFY			5			//Amplify sounds by a factor of 'x' in case casing makes things too quiet
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
BlynkParamAllocated soundEffectsList(128);
BlynkParamAllocated ledGroupsList(64);




////////////////////////////////////////////////////////////////////////////////
//EFFECT-SPECIFIC GLOBAL VARIABLES                                            //
////////////////////////////////////////////////////////////////////////////////
// MIC READING /////////////////////////////////////////////////////////////////
int sampleArray[SOUNDSAMPLES];			//An array to store previously read mic values

bool peakOccurred 		= false;		//True = a local peak ocurred

uint8_t overshootLeds 	= NUMLEDS + 2; 	//Allow effect to overshoot the LED strip
uint8_t sampleNumber 	= 0;			//Location in sampleArray to iterate over

uint16_t currentSample 	= 0; 			//Most current value read from the mic
uint16_t previousSample = 0;			//Previous value read from the mic
uint16_t dampSample 	= 0;			//Dampened value for currentSample to prevent twitchy look
uint16_t minSoundLevel 	= 0;			//Minimum of the values stored in sampleArray
uint16_t maxSoundLevel 	= 0;			//Maximum of the values stored in sampleArray
uint16_t dampMin 		= 0;			//Dampened value of minSoundLevel to prevent twitchy look
uint16_t dampMax 		= 0;			//Dampened value of maxSoundLevel to prevent twitchy look
uint16_t arraySum		= 0;			//Keep a sum of the values stored in sampleArray
uint16_t arrayAverage	= 0;			//Keep an average of the values stored in sampleArray

// EFFECTS /////////////////////////////////////////////////////////////////////
#define qsubd(x, b) ((x>b)?b:0)			//Digital unsigned subtraction macro
#define qsuba(x, b) ((x>b)?x-b:0)		//Analog unsigned subtracton macro

int thistime = 20;						//FIXIT comment here explinaing purpose

uint8_t maxChanges = 24;				//Maximum blending steps per iteration (less is smoother)
uint8_t timeval = 20;					//Time between calls of the effect function
uint8_t currentHue = 0;					//Used when a hue needs to be rotated over time
uint8_t thisIndex = 0;						//FIXIT comment here explaining purpose

int16_t xdist;							//Random number for noise generator
int16_t ydist;							//Random number for noise generator
uint16_t xscale = 30;					//FIXIT comment here explaining purpose
uint16_t yscale = 30;					//FIXIT comment here explaining pirpose

CRGBPalette16 currentPalette;			//Used in blending functions
CRGBPalette16 targetPalette;			//Used in blending functions
TBlendType currentBlending;				//Used in blending functions




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
	effectsList.add("Sun");
	effectsList.add("Beatwave");
	effectsList.add("Blendwave");
	effectsList.add("Dotbeat");
	effectsList.add("Confetti");
	effectsList.add("Mirrored Fire");
	effectsList.add("Juggle");
	effectsList.add("Lightning");
	effectsList.add("Plasma");
	effectsList.add("Rainbow Beat");
	effectsList.add("Rainbow March");
	effectsList.add("Sinelon");

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

	//Sound-reactive Effects
	soundEffectsList.add("Bracelet");
	soundEffectsList.add("Random Noise");
	soundEffectsList.add("Juggle");
	soundEffectsList.add("Matrix");
	soundEffectsList.add("Fire");
	soundEffectsList.add("Sine Wave");
	soundEffectsList.add("Pixel");
	soundEffectsList.add("Plasma");
	soundEffectsList.add("Rainbow Bit");
	soundEffectsList.add("Rainbow Gradient");
	soundEffectsList.add("Ripple");

	DEBUG_PRINTLN("Finished populating 'effectList'.");

	DEBUG_PRINTLN("Populating 'ledGroupList' with LED Groups.");

	//LED Groups
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
				case 1:		//sunriseSunset();		break;
				case 2:		beatWave(); 			break;
				case 3:		blendWave(); 			break;
				case 4:		confetti();				break;
				case 5:		dotBeat();				break;
				case 6:		mirroredFire();			break;
				case 7:		juggle();				break;
				case 8:		lightning(); 			break;
				case 9:		plasma();				break;
				case 10:	rainbowBeat();			break;
				case 11:	rainbowMarch();			break;
				case 12:	sinelon();				break;
				default:	ws2812fxImplementer();	break;
			}
		}

		//If a sound-reactive effect is selected, run soundmems to read microphone
		//before going into the effect function to react. This portion is just lifted from
		//Atuline's soundmems_demo repo.
		else if(selectedSoundEffect != 0){
			soundmems();

			EVERY_N_MILLISECONDS(20){
				maxChanges = 24;
				nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);
			}

			EVERY_N_MILLIS_I(thisTimer, timeval){
				thisTimer.setPeriod(timeval);
				switch(selectedSoundEffect){
					case 1:		soundBracelet();		break;
					case 2:		soundFillNoise();		break;
					case 3:		soundJuggle();			break;
					case 4:		soundMatrix();			break;
					case 5:		soundFire();			break;
					case 6:		soundSineWave();		break;
					case 7:		soundPixel();			break;
					case 8:		soundPlasma();			break;
					case 9:		soundRainbowBit();		break;
					case 10:	soundRainbowGradient();	break;
					case 11:	soundRipple();			break;
				}
			}

			EVERY_N_SECONDS(5){
				uint8_t baseclr = random8();
				targetPalette = CRGBPalette16(	CHSV(baseclr, 255, random8(128,255)),CHSV(baseclr+128, 255, random8(128,255)),
												CHSV(baseclr + random8(16), 192,
												random8(128,255)), CHSV(baseclr + random8(16), 255, random8(128,255)));
			}

			fastLedImplementer();

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
	//FIXIT since a lot of the functions don't have controls for the brightness internally,
	//FIXIT maybe test adding FastLed.setBrightness(); here and see if it works properly.
	//FIXIT For example, soundSineWave has an internal brightness value called 'thisbright'
	//FIXIT so add setBrightness() here and see if that internal brightness still scales
	//FIXIT within the global brightness here.
	FastLED.setBrightness(brightness);
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
			case 13:	ws2812fx.setMode(FX_MODE_STATIC);					break;
			case 14: 	ws2812fx.setMode(FX_MODE_BLINK);					break;
			case 15: 	ws2812fx.setMode(FX_MODE_COLOR_WIPE_RANDOM);		break;
			case 16: 	ws2812fx.setMode(FX_MODE_RAINBOW);					break;
			case 17: 	ws2812fx.setMode(FX_MODE_RAINBOW_CYCLE);			break;
			case 18: 	ws2812fx.setMode(FX_MODE_SCAN);						break;
			case 19: 	ws2812fx.setMode(FX_MODE_DUAL_SCAN);				break;
			case 20: 	ws2812fx.setMode(FX_MODE_FADE);						break;
			case 21: 	ws2812fx.setMode(FX_MODE_CHASE_COLOR);				break;
			case 22: 	ws2812fx.setMode(FX_MODE_CHASE_RANDOM); 			break;
			case 23: 	ws2812fx.setMode(FX_MODE_CHASE_RAINBOW);			break;
			case 24: 	ws2812fx.setMode(FX_MODE_CHASE_BLACKOUT_RAINBOW); 	break;
			case 25: 	ws2812fx.setMode(FX_MODE_RUNNING_LIGHTS);			break;
			case 26: 	ws2812fx.setMode(FX_MODE_RUNNING_COLOR); 			break;
			case 27: 	ws2812fx.setMode(FX_MODE_LARSON_SCANNER); 			break;
			case 28: 	ws2812fx.setMode(FX_MODE_COMET); 					break;
			case 29: 	ws2812fx.setMode(FX_MODE_FIREWORKS_RANDOM); 		break;
			case 30: 	ws2812fx.setMode(FX_MODE_MERRY_CHRISTMAS); 			break;
			case 31: 	ws2812fx.setMode(FX_MODE_HALLOWEEN); 				break;
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
void soundmems(){
	
	//Read current mic value and append it to the end of the array. Iterate
	//sampleNumber to prep for adding next value to the next position in the array.
	currentSample = AMPLIFY * abs(abs(analogRead(MICPIN) - DCOFFSET) - NOISE);
	
	if(currentSample < micSensitivity) currentSample = 0;
	
	sampleArray[sampleNumber] = currentSample;
	sampleNumber++;

	//Initiate a new variable 'sum' on each iteration of soundmems and use it to
	//calculate the sum of the values in the array of values.
	for(uint8_t h = 0; h < SOUNDSAMPLES - 1; h++){
		uint16_t sum;
		sum = sum + sampleArray[h];

		arraySum = sum;
		arrayAverage = sum/SOUNDSAMPLES;
	}
	
	//To get a dampened value, multiply dampSample by 7 and add the current sample
	//to make it seem as though you have 8 samples, then devide by 8.
	dampSample = ((dampSample * 7) + currentSample)/8;
	
	//If the number of iterations since starting from the beginning of the array
	//is greater than the size of the array, start iteratiing from 0 again.
	if(sampleNumber > (SOUNDSAMPLES - 1)) sampleNumber = 0;
	
	//Set the minimum and maximum values we've heard so far to the value at position
	//0 in the array. This is just to initiate min and max levels at some value.
	minSoundLevel = maxSoundLevel = sampleArray[0];
	
	//Iterated over the entire array and set the lowest value found as minSoundLevel,
	//and the highest value as maxSoundLevel.
	for(uint8_t i = 1; i < SOUNDSAMPLES; i++){
		if(sampleArray[i] < minSoundLevel) minSoundLevel = sampleArray[i];
		else if(sampleArray[i] > maxSoundLevel) maxSoundLevel = sampleArray[i];
	}
	
	//If the difference between minSoundLevel and maxSoundLevel is smaller than the
	//length of the LEDs (or however much you want to overshoot them), then set the max
	//to be equal to the min plus length of LEDs (or however much you want to overshoot).
	if((maxSoundLevel - minSoundLevel) < overshootLeds) maxSoundLevel = minSoundLevel + overshootLeds;
	
	//Dampening for the minimum and maximum values. Multiply the current averages by
	//SOUNDSAMPLES minus 1, then add the current minSoundLevel. Basically, pretend you
	//took 64 samples for min/max so that even if the min/max changes dramatically
	//for a second, it shouldn't effect the lights much. Then, divide by 64 to get the average.
	dampMin = (dampMin * (SOUNDSAMPLES - 1) + minSoundLevel)/SOUNDSAMPLES;
	dampMax = (dampMax * (SOUNDSAMPLES - 1) + maxSoundLevel)/SOUNDSAMPLES;
	
	if(currentSample > (dampSample + micSensitivity) && (currentSample < previousSample)){
		peakOccurred = true;
	}
	
	previousSample = currentSample;

	//Uncomment if you need raw values of mic readings printed to serial.
	DEBUG_PRINTLN(String("Current Sample: ") + currentSample + String(", Dampened: ") + dampSample);
}




////////////////////////////////////////////////////////////////////////////////
//NORMAL FASTLED EFFECT FUNCTIONS                                             //
////////////////////////////////////////////////////////////////////////////////
// LEDS OFF ////////////////////////////////////////////////////////////////////
void ledsOff(){
	FastLED.clear();
	fastLedImplementer();
}

// BEATWAVE ////////////////////////////////////////////////////////////////////
void beatWave(){}

// BLENDWAVE ///////////////////////////////////////////////////////////////////
void blendWave(){}

// CONFETTI ////////////////////////////////////////////////////////////////////
void confetti(){}

// DOT BEAT ////////////////////////////////////////////////////////////////////
void dotBeat(){}

// MIRRORED FIRE ///////////////////////////////////////////////////////////////
void mirroredFire(){}

// JUGGLE //////////////////////////////////////////////////////////////////////
void juggle(){}

// LIGHTNING ///////////////////////////////////////////////////////////////////
void lightning(){}

// PLASMA //////////////////////////////////////////////////////////////////////
void plasma(){}

// RAINBOW BEAT ////////////////////////////////////////////////////////////////
void rainbowBeat(){}

// RAINBOW MARCH ///////////////////////////////////////////////////////////////
void rainbowMarch(){}

// SINELON /////////////////////////////////////////////////////////////////////
void sinelon(){}




////////////////////////////////////////////////////////////////////////////////
//FASTLED SOUND REACTIVE EFFECT FUNCTIONS                                     //
////////////////////////////////////////////////////////////////////////////////
//FIXIT Need to alter this to work with the rest of Atuline's framwork
// SOUND BRACELET //////////////////////////////////////////////////////////////
void soundBracelet(){
	if(firstRun == true){
		firstRun = false;

		xscale = 0;					//Length of the portion of strip to light up
		yscale = 0;					//Location of the peak dot
	}

	uint16_t peakCount 		= 0;	//Keep a count of frames since peak was drawn
	uint16_t peakFallRate 	= 10;	//Number of frames until peak starts falling

	soundmems();
	
	//Scale the difference between the dampened current sample and the dampened minimum
	//to the difference between the dampened maximum and dampened minimum.
	xscale = overshootLeds * (dampSample - dampMin)/(long)(dampMax - dampMin);
	
	
	//Compensate for xscale being less than 0 (e.g. less than 0 LEDs) or more than
	//overshootLeds (e.g. more than 152 LEDs if NUMLEDS is 150). 
	if(xscale < 0L) xscale = 0;
	else if (xscale > overshootLeds) xscale = overshootLeds;
	
	//If the xscale is greater than where the peak last occured, this will be the new
	//peak.
	if(xscale > yscale) yscale = xscale;
	
	//Fill the number of LEDs equal to xscale with colors, and anything over that with
	//black.
	for (uint8_t i=0; i<NUMLEDS; i++){
		if(i >= xscale) leds[i].setRGB(0, 0, 0);
		else leds[i] = CHSV(map(i, 0, NUMLEDS - 1, 0, 150), 255, 255);
	}
	
	//Draw the peak dot.
	if(yscale > 0 && yscale <= NUMLEDS - 1){
		leds[yscale] = CHSV(map(yscale, 0, NUMLEDS - 1, 0, 150), 255, 255);
	}
	
	//Drop the peak dot every few frames.
	if(++peakCount >= peakFallRate){
		if(yscale > 0) yscale--;
		peakCount = 0;
	}
	
	fastLedImplementer();
}

// SOUND FILL NOISE ////////////////////////////////////////////////////////////
//FIXIT Works but crappy effect.
void soundFillNoise(){
	//Reset effect global variables to what is needed for this effect in case
	//they've been used by other effect functions.
	if(firstRun == true){
		firstRun = false;

		xdist = 0;
		ydist = 0;
		maxChanges = 24;
		timeval = 40;
		xscale = 30;
		yscale = 30;
	}

	if(arrayAverage > NUMLEDS) arrayAverage = NUMLEDS;

	for(int i = (NUMLEDS - arrayAverage/2)/2; i < (NUMLEDS + arrayAverage/2)/2; i++){
		uint8_t thisIndex = inoise8(i * arrayAverage + xdist, ydist + i * arrayAverage);
		leds[i] = ColorFromPalette(currentPalette, thisIndex, arrayAverage, LINEARBLEND);
	}

	xdist = ydist + beatsin8(5, 0, 3);
	ydist = ydist + beatsin8(4, 0, 3);

	glitter(arrayAverage/2);

	waveFromMiddle();

	fadeToBlackBy(leds + NUMLEDS/2 - 1, 2, 128);
}

// SOUND JUGGLE ////////////////////////////////////////////////////////////////
void soundJuggle(){
	if(firstRun == true){
		firstRun = false;

		currentHue = 0;
		thistime = 20;
		timeval = 20;
	}

	currentHue = currentHue + 4;

	fadeToBlackBy(leds, NUMLEDS, 12);

	leds[beatsin16(thistime, 0, NUMLEDS - 1, 0, 0)] 	+= ColorFromPalette( currentPalette, currentHue, arrayAverage, currentBlending);
	leds[beatsin16(thistime - 3, 0, NUMLEDS - 1, 0, 0)] += ColorFromPalette( currentPalette, currentHue, arrayAverage, currentBlending);

	EVERY_N_MILLISECONDS(250){
		thistime = arrayAverage/2;
	}

	glitter(arrayAverage/2);

	waveFromMiddle();
}

// SOUND MATRIX ////////////////////////////////////////////////////////////////
void soundMatrix(){
	if(firstRun == true){
		firstRun = false;

		currentHue = 0;
		timeval = 40;
	}

	leds[0] = ColorFromPalette(currentPalette, currentHue++, arrayAverage, LINEARBLEND);

	for(int i = NUMLEDS - 1; i > 0; i--) leds[i] = leds[i-1];

	glitter(arrayAverage/2);
}

// SOUND FIRE //////////////////////////////////////////////////////////////////
void soundFire(){
	if(firstRun == true){
		firstRun = false;

		xscale = 20;
		yscale = 3;
		thisIndex = 0;
		timeval = 0;
	}

	currentPalette = CRGBPalette16(	CHSV(0,255,2), CHSV(0,255,4), CHSV(0,255,8), CHSV(0, 255, 8),
									CHSV(0, 255, 16), CRGB::Red, CRGB::Red, CRGB::Red,                                   
									CRGB::DarkOrange,CRGB::DarkOrange, CRGB::Orange, CRGB::Orange,
									CRGB::Yellow, CRGB::Orange, CRGB::Yellow, CRGB::Yellow);

	for(int i = 0; i < NUMLEDS; i++){
		thisIndex = inoise8(i * xscale, currentMillis * yscale * NUMLEDS/255);
		thisIndex = (255 - i * 256/NUMLEDS) * thisIndex/128;
		leds[i] = ColorFromPalette(currentPalette, thisIndex, arrayAverage, NOBLEND);
	}

	glitter(arrayAverage/2);
}

// SOUND SINE WAVE /////////////////////////////////////////////////////////////
void soundSineWave(){
	if(firstRun == true){
		firstRun = false;

	}
}

// SOUND PIXEL /////////////////////////////////////////////////////////////////
void soundPixel(){
	if(firstRun == true){
		firstRun = false;

		xscale = 0;
		timeval = 0;
	}

	xscale = (xscale + 1) % (NUMLEDS - 1);

	CRGB newcolor = ColorFromPalette(currentPalette, previousSample, currentSample, currentBlending);
	nblend(leds[xscale], newcolor, 192);

	glitter(arrayAverage/2);
}

// SOUND PLASMA ////////////////////////////////////////////////////////////////
void soundPlasma(){
	if(firstRun == true){
		firstRun = false;

		xscale = 0; 	//Atuline thisphase
		yscale = 0; 	//Atuline thatphase

		currentHue = 0; 		//Atuline thisbright
		thisIndex = 0; 	//Atuline colorIndex

		timeval = 20;
	}

	xscale += beatsin8(6, -4, 4);
	yscale += beatsin8(7, -4, 4);

	for(int k = 0; k < NUMLEDS; k++){
		currentHue = cubicwave8((k * 8) + xscale)/2;
		currentHue += cos8((k * 10) + yscale)/2;
		thisIndex = currentHue;
		currentHue = qsuba(currentHue, 255 - arrayAverage);

		leds[k] = ColorFromPalette(currentPalette, thisIndex, currentHue, currentBlending);
	}

	glitter(arrayAverage/2);
}

// SOUND RAINBOW BIT ///////////////////////////////////////////////////////////
void soundRainbowBit(){
	if(firstRun == true){
		firstRun = false;

		timeval = 10;
		currentHue = 0;
	}

	currentHue = beatsin8(17, 0, 255);

	if(peakOccurred == 1){
		fill_rainbow(leds + random8(0, NUMLEDS/2), random8(0, NUMLEDS/2), currentHue, 8);
	}

	fadeToBlackBy(leds, NUMLEDS, 40);

	glitter(arrayAverage/2);
}

// SOUND RAINBOW GRADIENT //////////////////////////////////////////////////////
void soundRainbowGradient(){
	if(firstRun == true){
		firstRun = false;

		thisIndex = 0;
		currentPalette = PartyColors_p;
	}

	uint8_t beatA = beatsin8(17, 0, 255);
	uint8_t beatB = beatsin8(13, 0, 255);
	uint8_t beatC = beatsin8(11, 0, 255);

	for(int i = 0; i< NUMLEDS; i++){
		thisIndex = (beatA + beatB + beatC)/3 * i * 4/NUMLEDS;
		leds[i] = ColorFromPalette(currentPalette, thisIndex, arrayAverage, currentBlending);
	}

	glitter(arrayAverage);
}

// SOUND RIPPLE ////////////////////////////////////////////////////////////////
void soundRipple(){
	if(firstRun == true){
		firstRun = false;

		maxChanges = 24;	//Atuline maxsteps
		currentHue = 0; 	//Atuline colour
		xscale = 0;			//Atuline center
		thistime = -1;		//Atuline step
		timeval = 20;
	}

	if(peakOccurred == 1) thistime = -1;

	fadeToBlackBy(leds, NUMLEDS, 64);

	switch(thistime){
		case -1:
			xscale = random(NUMLEDS);
			currentHue = (previousSample) % 255;
			thistime = 0;
			break;
		case 0:
			leds[xscale] += ColorFromPalette(currentPalette, currentHue, 255, currentBlending);
			thistime++;
			break;
		case 24:
			thistime = -1;
			break;
		default:
			leds[(xscale + thistime + NUMLEDS) % NUMLEDS] += ColorFromPalette(currentPalette, currentHue, 255/thistime * 2, currentBlending);
			leds[(xscale - thistime + NUMLEDS) % NUMLEDS] += ColorFromPalette(currentPalette, currentHue, 255/thistime * 2, currentBlending);
			thistime++;
			break;
	}

	glitter(arrayAverage/2);
}




////////////////////////////////////////////////////////////////////////////////
//EFFECT MODIFIERS                                                            //
////////////////////////////////////////////////////////////////////////////////
// GLITTER /////////////////////////////////////////////////////////////////////
void glitter(fract8 chanceOfGlitter){
	if(random8() < chanceOfGlitter){
		leds[random16(NUMLEDS)] += CRGB::White;
	}
}

void waveFromMiddle(){
	leds[NUMLEDS/2] = ColorFromPalette(currentPalette, currentSample, currentSample * 2, currentBlending);

	//Shift all pixels past the middle pixel towards the ending pixel.
	for(uint8_t i = NUMLEDS - 1; i > NUMLEDS/2; i--){
		leds[i] = leds[i - 1];
	}

	//Shift all pixels before the middle pixel towards the starting pixel.
	for(uint8_t i = 0; i < NUMLEDS/2; i++){
		leds[i] = leds[i + 1];
	}
}