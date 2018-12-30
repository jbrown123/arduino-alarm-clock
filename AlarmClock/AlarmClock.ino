#include <NTPClient.h>

// change next line to use with another board/shield
#include <ESP8266WiFi.h>

#include <WiFiUdp.h>

#include <TM1637Display.h>

#include "secret.h"

#include "pt-1.4/pt.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

#include <Timezone.h>    // https://github.com/JChristensen/Timezone Modified!

// For Webserver
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// OTA Updates — ESP8266 Arduino Core 2.4.0 documentation
// https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html
#include <ArduinoOTA.h>

// For WifiManager
#include <DNSServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>
// Available on the library manager (ArduinoJson)
// https://github.com/bblanchon/ArduinoJson

#include <WiFiClientSecure.h>

#include <NonBlockingRtttl.h>

class hiresTimer
{
protected:
	unsigned long startTime;
	
public:
	// sets the start time of the timer
	void start()
	{
		startTime = micros();
	}

	// returns true if the specified duration (in microseconds) has elapsed
	boolean delayMicros(unsigned long duration)
	{
		return(micros() - startTime >= duration);
	}

	// returns true if the specified duration (in milliseconds) has elapsed
	boolean delayMillis(unsigned long duration)
	{
		return(delayMicros(duration*1000));
	}

	// returns the elapsed time in microseconds
	unsigned long elapsedMicros()
	{
		return(micros() - startTime);
	}

	// returns the elapsed time in milliseconds
	unsigned long elapsedMillis()
	{
		return(elapsedMicros() / 1000);
	}
};

char *startupSound = "Star Trek:d=16,o=5,b=120:8f.,a#,4d#.6,8d6,a#.,g.,c.6,4f6";
char *alarmSound = "Ring Low High:d=16,o=5,b=355:b4,d,b4,d,b4,d,b4,d,d,f,d,f,d,f,d,f,f,a,f,a,f,a,f,a,2p,b5,d6,b5,d6,b5,d6,b5,d6,d6,f6,d6,f6,d6,f6,d6,f6,f6,a6,f6,a6,f6,a6,f6,a6,1p";
// 10 ticks:d=1,o=4,b=240:100c,4p,100c,4p,100c,4p,100c,4p,100c,4p,100c,4p,100c,4p,100c,4p,100c,4p,100c,4p


// here is all the alarm info
struct
{
	char alarmSound[2049];	// the alarm tune
	byte volume;	// 1-100% (really only works in 10% increments; 10-100)
	struct 
	{
		byte alarmHour;
		byte alarmMinute;
	} alarmDay[7];	// one for each day, 0=Sun ... 6=Sat
} alarmInfo;


// thread states
struct
{
	struct pt	pt;
	
	unsigned long lastUpdateTime = 0;
	boolean timeUpdateSuccess = false;
} timeUpdateState;

struct
{
	struct pt	pt;
	
	unsigned long lastScreenUpdate = 0;
	boolean dotsOn;
	boolean displayDisabled;
} displayUpdateState;

struct
{
	struct pt	pt;
} rtttlPlayerState;

struct
{
	struct pt	pt;

	int lastMinute;
	class hiresTimer	timer;
} alarmThreadState;

boolean globalAlarmState = true;

void displayTime(bool dotsVisible);
bool checkForAlarm();

// For storing configurations
#include "FS.h"
// see http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html
// The following diagram illustrates flash layout used in Arduino environment:
// |--------------|-------|---------------|--|--|--|--|--|
// ^              ^       ^               ^     ^
// Sketch    OTA update   File system   EEPROM  WiFi config (SDK)

// from wemos website
// wemos d1 mini using ESP8266MOD
/*
	Pin	Function						ESP-8266 Pin
	TX	TXD								TXD
	RX	RXD								RXD
	A0	Analog input, max 3.3V input	A0
	D0	IO								GPIO16
	D1	IO, SCL							GPIO5
	D2	IO, SDA							GPIO4
	D3	IO, 10k Pull-up					GPIO0
	D4	IO, 10k Pull-up, BUILTIN_LED	GPIO2
	D5	IO, SCK							GPIO14
	D6	IO, MISO						GPIO12
	D7	IO, MOSI						GPIO13
	D8	IO, 10k Pull-down, SS			GPIO15
	G	Ground							GND
	5V	5V								-
	3V3	3.3V							3.3V
	RST	Reset							RST
	All of the IO pins have interrupt/pwm/I2C/one-wire support except D0.
	All of the IO pins run at 3.3V.
*/

// Module connection pins (Digital Pins)
#define CLK D6
#define DIO D5

#define BUILTIN_LED	D4

#define ALARM D1

#define BUTTON D2
#define SNOOZE_BUTTON D3

#define LDR A0

TM1637Display display(CLK, DIO);

const uint8_t LETTER_A = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
const uint8_t LETTER_B = SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
const uint8_t LETTER_C = SEG_A | SEG_D | SEG_E | SEG_F;
const uint8_t LETTER_D = SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
const uint8_t LETTER_E = SEG_A | SEG_D | SEG_E | SEG_F | SEG_G;
const uint8_t LETTER_F = SEG_A | SEG_E | SEG_F | SEG_G;

const uint8_t LETTER_O = SEG_C | SEG_D | SEG_E | SEG_G;
const uint8_t LETTER_N = SEG_C | SEG_E | SEG_G;
const uint8_t LETTER_T = SEG_D | SEG_E | SEG_F | SEG_G;

const uint8_t SEG_CONF[] = {
  LETTER_C,                                        // C
  LETTER_O,                                        // o
  LETTER_N,                                        // n
  LETTER_F                                         // F
};

const uint8_t SEG_BOOT[] = {
  LETTER_B,                                        // b
  LETTER_O,                                        // o
  LETTER_O,                                        // o
  LETTER_T                                         // t - kinda
};

ESP8266WebServer server(80);

const char *webpage =
#include "alarmWeb.h"
  ;

void handleRoot() {

  server.send(200, "text/html", webpage);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

boolean webActive = false;

void handleGetAlarm() {
  // single threaded
  if (webActive)
  {
    server.send(503, "text/html", "busy - single threaded");
  	return;
  }
  webActive = true;

  StaticJsonBuffer<2048> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json["alarmSound"] = alarmInfo.alarmSound;
  
  json["volume"] = alarmInfo.volume;

  JsonArray& dayArray = jsonBuffer.createArray();
  
  for (int i = 0; i < 7; i++)
  {
  	JsonObject& obj = dayArray.createNestedObject();
  	
  	obj["alarmHour"] = alarmInfo.alarmDay[i].alarmHour;
  	obj["alarmMinute"] = alarmInfo.alarmDay[i].alarmMinute;
  }

  json["alarmDay"] = dayArray;

  String alarmString;
  json.prettyPrintTo(alarmString);

  server.send(200, "text/plain", alarmString);

  webActive = false;
}

void handleSetAlarm() 
{
  // single threaded
  if (webActive)
  {
    server.send(503, "text/html", "busy - single threaded");
  	return;
  }
  webActive = true;

  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "alarm") {
      String alarm = server.arg(i);
 
      int indexOfColon = alarm.indexOf(":");
      
      byte alarmHour = constrain(alarm.substring(0, indexOfColon).toInt(), 0, 23);
      byte alarmMinute = constrain(alarm.substring(indexOfColon + 1).toInt(), 0, 59);
      byte alarmDay = constrain(alarm.substring(alarm.indexOf(";")+1).toInt(), 0, 6);

      alarmInfo.alarmDay[alarmDay].alarmHour = alarmHour;
      alarmInfo.alarmDay[alarmDay].alarmMinute = alarmMinute;
      
    }
    if (server.argName(i) == "alarmSound")
    {
    	if (server.arg(i).length() <= 2048)
    		strcpy(alarmInfo.alarmSound, server.arg(i).c_str());
    		
    	if (strlen(alarmInfo.alarmSound) < 10)
    		strcpy(alarmInfo.alarmSound, alarmSound);	// default sound
    }
    if (server.argName(i) == "volume")
   		alarmInfo.volume = constrain(server.arg(i).toInt(), 10, 100);
  }
  
  saveConfig();
  
  server.send(200, "text/html", "Alarm Set");
  
  webActive=false;
}


WiFiClientSecure client;

// From World clock example in timezone library
// United Kingdom (London, Belfast)
TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        // British Summer Time
TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         // Standard Time
Timezone UK(BST, GMT);

TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    // UTC + 11 hours
TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    // UTC + 10 hours
Timezone ausET(aEDT, aEST);

// US Eastern Time Zone (New York, Detroit)
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  // Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   // Eastern Standard Time = UTC - 5 hours
Timezone usET(usEDT, usEST);

// US Mountain Time Zone (Denver, Salt Lake City)
TimeChangeRule usMDT = {"MDT", Second, Sun, Mar, 2, -360};  // Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usMST = {"MST", First, Sun, Nov, 2, -420};   // Eastern Standard Time = UTC - 5 hours
Timezone usMT(usMDT, usMST);

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  display.setSegments(SEG_CONF);
}

void setup() 
{
  Serial.begin(115200);

  // init all the thread variables
  PT_INIT(&timeUpdateState.pt);
  PT_INIT(&displayUpdateState.pt);
  PT_INIT(&rtttlPlayerState.pt);
  PT_INIT(&alarmThreadState.pt);


  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }

  loadConfig();

  display.setBrightness(7);	// max brightness
  display.setSegments(SEG_BOOT);

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);	// off
     
  pinMode(ALARM, OUTPUT);
  digitalWrite(ALARM, LOW);

  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(SNOOZE_BUTTON, INPUT_PULLUP);

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect("AlarmClock", "password");
/*
	Chip ID
	5372786
	Flash Chip ID
	1458270
	IDE Flash Size
	4194304 bytes
	Real Flash Size
	4194304 bytes
	Soft AP IP
	192.168.4.1
	Soft AP MAC
	62:01:94:51:FB:72
	Station MAC
	60:01:94:51:FB:72
 */
 
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");

  IPAddress ipAddress = WiFi.localIP();
  Serial.println(ipAddress);

  display.showNumberDec(ipAddress[3], false);

  delay(1000);

//  if (MDNS.begin("alarmclock")) {
//    Serial.println("MDNS Responder Started");
//  }

  server.on("/", handleRoot);
  server.on("/setAlarm", handleSetAlarm);
  server.on("/getAlarm", handleGetAlarm);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP Server Started");

  timeClient.begin();

  // enable over the air updates

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("alarmclock");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();	// start the OTA responder
  Serial.println("OTA server started");

  rtttl::begin(ALARM, startupSound);

/*
	for (int i = 0; i < 20; i++)
	{
		tone(ALARM, NOTE_C4);
		delay(10);
		noTone(ALARM);
		delay(240);
	}
	delay(500);
	
	unsigned long timeStart = millis();
	while (millis() - timeStart < 1500)
	{
		// generate a 440Hz tone
		for (unsigned long toneStart = micros(); micros() - toneStart < 1000000/(NOTE_C4*2);)
		{
			// 20% duty cycle at 50KHz
			unsigned long volStart = micros();
			digitalWrite(ALARM, 1);
			while (micros() - volStart < 10)
				;
	
			volStart = micros();
			digitalWrite(ALARM, 0);
			while (micros() - volStart < 40)
				;
		}
		for (unsigned long toneStart = micros(); micros() - toneStart < 1000000/(NOTE_C4*2);)
			;
	}
*/
}

bool loadConfig() {
  File configFile = SPIFFS.open("/alarm.dat", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");

  	strcpy(alarmInfo.alarmSound, alarmSound);	// default sound
    alarmInfo.volume = 100;	// limit to 10 - 100
    
    return false;
  }

  size_t size = configFile.size();
  if (size != sizeof(alarmInfo) ) {
    Serial.println("Config file size is incorrect");
    return false;
  }

  configFile.readBytes((char *) &alarmInfo, sizeof(alarmInfo));
  configFile.close();

  if (strlen(alarmInfo.alarmSound) < 10)
  	strcpy(alarmInfo.alarmSound, alarmSound);	// default sound

  return true;
}

bool saveConfig() 
{
  File configFile = SPIFFS.open("/alarm.dat", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");	// doesn't actually print
    return false;
  }

  configFile.write((unsigned char *) &alarmInfo, sizeof(alarmInfo));
  configFile.close();
  
  return true;
}


void soundAlarm() 
{
	if (!globalAlarmState)
		return;

	if (!rtttl::isPlaying())
		rtttl::begin(ALARM, alarmInfo.alarmSound);
}

// current real-time state of the buttons
// left button (snooze) returns 2, right button returns 1
uint8_t ButtonState()
{
	return ((digitalRead(SNOOZE_BUTTON) == LOW ? 2 : 0) | (digitalRead(BUTTON) == LOW ? 1 : 0));
}

uint8_t ButtonChange()
{
	static uint8_t lastButtonState = 0;

	uint8_t curr = ButtonState();

	// which have changed anded with current = what's newly on
	uint8_t retval = (lastButtonState ^ curr) & curr;

	// remember the new value
	lastButtonState = curr;

	return(retval);
}

#define timeUpdateInterval (1000 * 60 * 1)
#define errorTimeUpdateInterval	(1000 * 30)

PT_THREAD(DisplayUpdateThread(void))
{
	struct pt *pt = &displayUpdateState.pt;
	unsigned long now = millis();
	
	PT_BEGIN(pt);

	while(1)
	{
		// screen update
		PT_WAIT_UNTIL(pt, !displayUpdateState.displayDisabled && 
			(displayUpdateState.lastScreenUpdate == 0 || now - displayUpdateState.lastScreenUpdate > 1000));
		
		// brightness doesn't change until the next display update
		display.setBrightness(analogRead(LDR)/256);	// 0 = dim, 7 = max bright
		
		// if we can't get NTP, then flash the time
		if (!timeUpdateState.timeUpdateSuccess)
		{
			if (displayUpdateState.dotsOn)
				displayTime(displayUpdateState.dotsOn);
			else
				display.clear();
		}
		else
			displayTime(displayUpdateState.dotsOn);
	
		displayUpdateState.dotsOn = !displayUpdateState.dotsOn;
		
		displayUpdateState.lastScreenUpdate = now;
	}
	
	PT_END(pt);
}

PT_THREAD(TimeUpdateThread(void))
{
	struct pt *pt = &timeUpdateState.pt;
	unsigned long now = millis();
	
	PT_BEGIN(pt);

	// get initial time
	timeUpdateState.timeUpdateSuccess = timeClient.update();
	timeUpdateState.lastUpdateTime = now;

	while(1)
	{
		// do updates infrequently unless there's an error
	  	PT_WAIT_UNTIL(pt, now - timeUpdateState.lastUpdateTime > timeUpdateInterval
	  		|| (!timeUpdateState.timeUpdateSuccess && now - timeUpdateState.lastUpdateTime > errorTimeUpdateInterval) );

	  	// no updates while playing
		PT_WAIT_UNTIL(pt, !rtttl::isPlaying());
		
	  	// NTP update
		timeUpdateState.timeUpdateSuccess = timeClient.update();
		timeUpdateState.lastUpdateTime = now;
	}
	
	PT_END(pt);
}

// https://www.reddit.com/r/arduino/comments/4l2pfe/now_arduinos_tone_function_has_8bit_volume_control/

PT_THREAD(RtttlPlayerThread())
{
	struct pt *pt = &rtttlPlayerState.pt;

	PT_BEGIN(pt);

	while(1)
	{
		if (rtttl::isPlaying())
			rtttl::play();	// keep playing stuff
		
		PT_YIELD(pt);
	}
	
	PT_END(pt);
}

PT_THREAD(AlarmThread())
{
	struct pt *pt = &alarmThreadState.pt;

	int timeHour;
	int timeMinutes;
	int timeDay;
	
	boolean alarmCheck = false;	// once each minute this is set to true;
	
	unsigned long epoch = usMT.toLocal(timeClient.getEpochTime());
	
	timeHour = (epoch % 86400L) / 3600;
	timeMinutes = (epoch % 3600) / 60;
	timeDay = (((epoch  / 86400L) + 4 ) % 7); //0 is Sunday
	
	// check at each minute change for alarm activation
	if (timeMinutes != alarmThreadState.lastMinute)
		alarmCheck = true;
		
	alarmThreadState.lastMinute = timeMinutes;

	byte buttons = ButtonState();
	
	PT_BEGIN(pt);

	while(1)
	{
	    // alarm check
	    PT_WAIT_UNTIL(pt, globalAlarmState && alarmCheck 
	    	&& timeHour == alarmInfo.alarmDay[timeDay].alarmHour 
	    	&& timeMinutes == alarmInfo.alarmDay[timeDay].alarmMinute);

		while (buttons == 0)
		{
			// keep ringing the alarm until user presses a button
			soundAlarm();

			// wait until user presses a button OR we finish playing
			PT_WAIT_UNTIL(pt, buttons != 0 || !rtttl::isPlaying());
		}
		
		rtttl::stop();	// stop the alarm
	}
	
	PT_END(pt);
}

#define buttonLongPressInterval	1900

void loop() 
{
	AlarmThread();	// should come before time update
	RtttlPlayerThread();
	TimeUpdateThread();
	DisplayUpdateThread();
	
	ArduinoOTA.handle();
	server.handleClient();

	unsigned long now = millis();
	static unsigned long buttonDownStartTime = 0;

	if ( ButtonState() == 1 ) {
		if (buttonDownStartTime == 0)
			buttonDownStartTime = now;
		else if (now - buttonDownStartTime > buttonLongPressInterval)
		{
			// Serial.print("button down time "); Serial.println(now - buttonDownStartTime);
			
			// toggle global alarm state
			globalAlarmState = !globalAlarmState;
			// reset timer
			buttonDownStartTime = 0;

			soundAlarm();	// play a sample of the alarm (if enabled)
		}
	} else
		buttonDownStartTime = 0;

	if(ButtonState() == 3)
	{
		displayUpdateState.displayDisabled = true;
		
		int sensorValue = analogRead(LDR);
		display.setBrightness(sensorValue/256);	// 0 = dim, 7 = max bright
		display.showNumberDec(sensorValue, false);
		displayUpdateState.lastScreenUpdate = 0;
	} else if ( ButtonState() == 2 ) 
	{
		displayUpdateState.displayDisabled = true;

		IPAddress ipAddress = WiFi.localIP();
		display.showNumberDec(ipAddress[3], false);
		displayUpdateState.lastScreenUpdate = 0;
	} else
		displayUpdateState.displayDisabled = false;
}

void displayTime(bool dotsVisible) 
{
	int timeHour;
	int timeMinutes;
	
	unsigned long epoch = usMT.toLocal(timeClient.getEpochTime());
	
	timeHour = (epoch % 86400L) / 3600;
	timeMinutes = (epoch % 3600) / 60;
	
	uint8_t data[4];
	
	uint8_t hr = timeHour;
	uint8_t pmFlag = 0;
	
	if (hr > 11)
		pmFlag = 1;
	
	if (hr == 0)
		hr = 12;
	else if (hr > 12)
		hr -= 12;
	
	if (hr < 10) 
	{
		data[0] = 0;	// display.encodeDigit(0);
		data[1] = display.encodeDigit(hr);
	} 
	else 
	{
		data[0] = display.encodeDigit(hr / 10);
		data[1] = display.encodeDigit(hr % 10);
	}
	
	// use left column as AM/PM flag
	data[0] |= pmFlag ? SEG_E : SEG_F;
	
	if (dotsVisible) 
	{
		// Turn on double dots
		data[1] = data[1] | B10000000;
		
		// use left dash as alarm disable flag
		data[0] |= globalAlarmState ? 0 : SEG_G;
	}
	
	if (timeMinutes < 10) 
	{
		data[2] = display.encodeDigit(0);
		data[3] = display.encodeDigit(timeMinutes);
	} 
	else
	{
		data[2] = display.encodeDigit(timeMinutes / 10);
		data[3] = display.encodeDigit(timeMinutes % 10);
	}
	
	display.setSegments(data);
}

