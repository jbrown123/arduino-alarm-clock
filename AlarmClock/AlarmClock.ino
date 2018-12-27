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

#include "pitches.h"

class hiresTimer
{
private:
	unsigned long startTime;
	
public:
	void start()
	{
		startTime = micros();
	}

	boolean delayMicros(unsigned long duration)
	{
		return(micros() - startTime >= duration);
	}

	boolean delayMillis(unsigned long duration)
	{
		return(delayMicros(duration*1000));
	}

	unsigned long elapsedMicros()
	{
		return(micros() - startTime);
	}

	unsigned long elapsedMillis()
	{
		return(elapsedMicros() / 1000);
	}
};

boolean globalAlarmState = true;

#define ENABLE_TRAFFIC 0
#if ENABLE_TRAFFIC
#include <GoogleMapsDirectionsApi.h>

bool enableTrafficAdjust = false;

//Free Google Maps Api only allows for 2500 "elements" a day, so carful you dont go over
unsigned long api_mtbs = 60000; //mean time between api requests
unsigned long api_due_time = 0;
bool firstTime = true;

String origin = "40.8359838,-73.8734402";
String destination = "40.8536064,-73.9667197";
String waypoints = ""; //You need to include the via: before your waypoint

//Optional
DirectionsInputOptions inputOptions;
#endif

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

int alarmHour = 0;
int alarmMinute = 0;
bool alarmActive = false;
bool alarmHandled = false;
bool buttonPressed = false;

void handleGetAlarm() {
  String alarmString = String(alarmHour) + ":" + String(alarmMinute);
  server.send(200, "text/plain", alarmString);
}

int trafficOffset = 0;

WiFiClientSecure client;

#if ENABLE_TRAFFIC
GoogleMapsDirectionsApi api(MAPS_API_KEY, client);
#endif

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

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }

  loadConfig();

  display.setBrightness(7);	// max brightness
  display.setSegments(SEG_BOOT);

  pinMode(ALARM, OUTPUT);
  digitalWrite(ALARM, LOW);

  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(SNOOZE_BUTTON, INPUT_PULLUP);

  attachInterrupt(BUTTON, interuptButton, RISING);

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

#if ENABLE_TRAFFIC
  //These are all optional (although departureTime needed for traffic)
  inputOptions.departureTime = "now"; //can also be a future timestamp
  inputOptions.trafficModel = "best_guess"; //Defaults to this anyways
  inputOptions.avoid = "ferries";
  inputOptions.units = "metric";
#endif

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
  File configFile = SPIFFS.open("/alarm.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  alarmHour = json["alarmHour"];
  alarmMinute = json["alarmMinute"];
  alarmActive = json["alarmActive"];
  return true;
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["alarmHour"] = alarmHour;
  json["alarmMinute"] = alarmMinute;
  json["alarmActive"] = alarmActive;

  File configFile = SPIFFS.open("/alarm.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

void handleSetAlarm() {

  Serial.println("Setting Alarm");
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "alarm") {
      String alarm = server.arg(i);
      int indexOfColon = alarm.indexOf(":");
      alarmHour = alarm.substring(0, indexOfColon).toInt();
      alarmMinute = alarm.substring(indexOfColon + 1).toInt();
      alarmActive = true;
      saveConfig();
      Serial.print("Setting Alarm to: ");
      Serial.print(alarmHour);
      Serial.print(":");
      Serial.print(alarmMinute);
    }
  }
  server.send(200, "text/html", "Set Alarm");
}

// notes in the melody:
/*
int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};
// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};
*/

int melody[] = {
  1, 2, 3, 4, 5
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  1, 1, 1, 1, 1
};

// https://www.reddit.com/r/arduino/comments/4l2pfe/now_arduinos_tone_function_has_8bit_volume_control/

void soundAlarm() {
  if (!globalAlarmState)
  	return;
  	
  for (int thisNote = 0; thisNote < (sizeof(melody) / sizeof(melody[0])); thisNote++) {

    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    if (melody[thisNote] > 0)
	    tone(ALARM, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(ALARM);
  }
}

bool dotsOn = false;

#if ENABLE_TRAFFIC
void checkGoogleMaps() {
  Serial.println("Getting traffic for " + origin + " to " + destination);
  DirectionsResponse response = api.directionsApi(origin, destination, inputOptions);
  if (response.duration_value == 0) {
    delay(100);
    response = api.directionsApi(origin, destination, inputOptions);
  }
  Serial.println("Response:");
  Serial.print("Trafic from ");
  Serial.print(response.start_address);
  Serial.print(" to ");
  Serial.println(response.end_address);

  Serial.print("Duration in Traffic text: ");
  Serial.println(response.durationTraffic_text);
  Serial.print("Duration in Traffic in Seconds: ");
  Serial.println(response.durationTraffic_value);

  Serial.print("Normal duration text: ");
  Serial.println(response.duration_text);
  Serial.print("Normal duration in Seconds: ");
  Serial.println(response.duration_value);

  Serial.print("Distance text: ");
  Serial.println(response.distance_text);
  Serial.print("Distance in meters: ");
  Serial.println(response.distance_value);

  trafficOffset = (response.durationTraffic_value - response.duration_value) / 60 ;

  Serial.print("Traffic Offset: ");
  Serial.println(trafficOffset);
}
#endif

#define timeUpdateInterval (1000 * 60 * 1)
#define errorTimeUpdateInterval	(1000 * 30)

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

#define buttonLongPressInterval	1900

void loop() 
{
  ArduinoOTA.handle();
	
  unsigned long now = millis();
  static unsigned long lastUpdateTime = 0;
  static boolean timeUpdateSuccess = false;
  static unsigned long lastScreenUpdate = 0;
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
  	}
  } else
  	buttonDownStartTime = 0;

  if(ButtonState() == 3){
    int sensorValue = analogRead(LDR);
    display.setBrightness(sensorValue/256);	// 0 = dim, 7 = max bright
    display.showNumberDec(sensorValue, false);
    lastScreenUpdate = 0;
  } else if ( ButtonState() == 2 ) {
    IPAddress ipAddress = WiFi.localIP();
    display.showNumberDec(ipAddress[3], false);
    lastScreenUpdate = 0;
  } else {
  	// NTP update
  	if (lastUpdateTime == 0 
  		|| now - lastUpdateTime > timeUpdateInterval
  		|| (!timeUpdateSuccess && now - lastUpdateTime > errorTimeUpdateInterval) )
  	{
      timeUpdateSuccess = timeClient.update();
      lastUpdateTime = now;
  	}

	// screen update
  	if (lastScreenUpdate == 0 || now - lastScreenUpdate > 1000)
  	{
      // brightness doesn't change until the next display update
      display.setBrightness(analogRead(LDR)/256);	// 0 = dim, 7 = max bright

      // if we can't get NTP, then flash the time
      if (!timeUpdateSuccess)
      {
      	if (dotsOn)
      		displayTime(dotsOn);
      	else
      		display.clear();
      }
      else
      	displayTime(dotsOn);
      dotsOn = !dotsOn;
      
      lastScreenUpdate = now;
    }

    // alarm check
      checkForAlarm();
      if (buttonPressed) {
        alarmHandled = true;
        buttonPressed = false;
      }
  }

#if ENABLE_TRAFFIC
  if (enableTrafficAdjust)
  {
    if ((now > api_due_time))  {
      inputOptions.waypoints = waypoints;
      checkGoogleMaps();
      api_due_time = now + api_mtbs;
    }
  }
#endif

  server.handleClient();
}

int timeHour;
int timeMinutes;

int lastEffectiveAlarm = 0;

//bool checkForAlarm()
//{
//  int effectiveAlarmMinute = alarmMinute;
//  int effectiveAlarmHour = alarmHour;
//  int actualAlarmMinutesFromMidnight = (alarmHour * 60) + alarmMinute;
//  int effectiveAlarmMinutesFromMidnight = actualAlarmMinutesFromMidnight;
//  if (trafficOffset != 0)
//  {
//    effectiveAlarmMinutesFromMidnight -= trafficOffset;
//
//    if (effectiveAlarmHour > 1439)
//    {
//      effectiveAlarmMinutesFromMidnight = effectiveAlarmMinutesFromMidnight % 1440;
//    }
//
//    if (effectiveAlarmHour < 0)
//    {
//      effectiveAlarmMinutesFromMidnight = (1440 + effectiveAlarmMinutesFromMidnight) % 1440;
//    }
//  }
//
//  int minutesSinceMidnight = (hour * 60) + minutes;
//
//  if (alarmActive) {
//    if (minutesSinceMidnight >= effectiveAlarmMinutesFromMidnight) {
//      if (minutesSinceMidnight <= actualAlarmMinutesFromMidnight + 30) {
//        if (!alarmHandled)
//        {
//          soundAlarm();
//        }
//      }
//    }
//
//  } else if (minutesSinceMidnight = 0) {
//    alarmHandled = false;
//  }
//
//  lastEffectiveAlarm = effectiveAlarmMinutesFromMidnight;
//}

bool checkForAlarm()
{
  if (alarmActive && timeHour == alarmHour && timeMinutes == alarmMinute) {
    if (!alarmHandled)
    {
      soundAlarm();
    }
  } else {
    alarmHandled = false;
  }
}

void interuptButton()
{
  // Serial.println("interuptButton");
  buttonPressed = true;
  return;
}

void displayTime(bool dotsVisible) {

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
  	
  if (hr < 10) {
    data[0] = 0;	// display.encodeDigit(0);
    data[1] = display.encodeDigit(hr);
  } else {
    data[0] = display.encodeDigit(hr / 10);
    data[1] = display.encodeDigit(hr % 10);
  }

  // use left column as AM/PM flag
  data[0] |= pmFlag ? SEG_E : SEG_F;

  if (dotsVisible) {
    // Turn on double dots
    data[1] = data[1] | B10000000;
    
	// use left dash as alarm disable flag
	data[0] |= globalAlarmState ? 0 : SEG_G;
  }

  if (timeMinutes < 10) {
    data[2] = display.encodeDigit(0);
    data[3] = display.encodeDigit(timeMinutes);
  } else {
    data[2] = display.encodeDigit(timeMinutes / 10);
    data[3] = display.encodeDigit(timeMinutes % 10);
  }
  
  display.setSegments(data);
}

