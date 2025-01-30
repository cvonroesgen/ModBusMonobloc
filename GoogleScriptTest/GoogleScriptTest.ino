#include "WiFiSSLClient.h"
#include <Adafruit_RGBLCDShield.h>
#include <EEPROM.h>

#include "WiFiS3.h"
#include <Arduino_JSON.h>
// to do https://api.weather.gov/stations/KBED/observations/latest
// properties.dewpoint.value
// properties.dewpoint.qualityControl
//  EXT_COIL_TEMP	03	External coil temperature	Unicode/double
//  byte COOL_COIL_TEMP	03	Cooling coil temperature	Unicode/double
//  byte
// Google Script to log data
// https://script.google.com/macros/s/AKfycby5aqlUA9junaFrhdakvVVYDYJccGsa0mIGeRLHS6_5a36PLq5-rx2unjfTYQVAjZEV/exec

#define OFF 0x0
#define ON 0x1

#define SERIAL_BUFFER_SIZE 16
#define outDoorResetIntervalMinutes 60
#define loggingIntervalMinutes 1
#define lcdLEDDisplayIntervalMinutes 3
#define NO_HEAT_REQUIRED_TEMP_IN_C 22
#define NO_HEAT_REQUIRED_HI_LIMIT 25
#define NO_HEAT_REQUIRED_LO_LIMIT 18
#define DEGREES_C_TO_RAISE_H20 .7f
#define DEGREES_C_TO_RAISE_H20_HI_LIMIT 1.0f
#define DEGREES_C_TO_RAISE_H20_LO_LIMIT 0.5f

enum responseType { SET_RESPONSE,
                    GET_RESPONSE };

enum responseType setOrGet;

Adafruit_RGBLCDShield lcd;
const long millisecondsInMinute = 60000L;
byte serialReceiveBuffer[SERIAL_BUFFER_SIZE];
int8_t serialReceiveBufferIndex = 0;
int bufferComplete = 0;
unsigned short crc16;
int led = 13;
int EN = 2;
unsigned long outDoorResetTimer =
  millis() - (outDoorResetIntervalMinutes * millisecondsInMinute);
unsigned long loggingTimer =
  millis() - (loggingIntervalMinutes * millisecondsInMinute);
unsigned long ledDisplayTimer = millis();

const unsigned long dewPointUpdateInterval =
  outDoorResetIntervalMinutes * millisecondsInMinute;
unsigned long lastDewPointUpdateTime = millis() - dewPointUpdateInterval;
unsigned long debugTimer = millis();

#define DEFROST_STATUS 2136               // defrost is bit 5
#define DELTA_AMBIENT_COIL 2040           // Parameter 32
#define TEMP_TO_EXTEND_DEFROST_TIME 2039  // Parameter 31
#define COIL_TEMP_FOR_DEFROST_MODE 2038   // Parameter 30
#define AMBIENT_TEMP 2110
#define ON_OFF 2000
#define HOT_WATER_SET_POINT 2004
#define WATER_TANK_TEMP 2100
#define Outlet_water_temperature 2102
#define Inlet_water_temperature 2103
#define EXT_COIL_TEMP 2107
#define COOL_COIL_TEMP 2108
#define FAN_SPEED 2119
#define AC_VOLTS 2120
#define AC_AMPS 2121
#define NWS_DEW_POINT 199
#define NO_HEAT_TEMP_SET 200
#define C_UP_PER_C_DOWN 201
#define COP_CRC 202

struct menuCode {
  unsigned short code;
  char label[17];
  float value;
};

menuCode menuCodes[] = { { ON_OFF, "On or Off   ", 0 },
                         { DEFROST_STATUS, "Defrost Status  ", -30 },
                         { HOT_WATER_SET_POINT, "Temp Set    ", -20 },
                         { WATER_TANK_TEMP, "Temp Tank   ", -50 },
                         { Outlet_water_temperature, "Temp H2O >  ", -60 },
                         { Inlet_water_temperature, "Temp H2O <   ", -70 },
                         { AMBIENT_TEMP, "Temp Air     ", -80 },
                         { EXT_COIL_TEMP, "Temp Ext Coil", -90 },
                         { COOL_COIL_TEMP, "Temp Cool Coil", 0 },
                         { COIL_TEMP_FOR_DEFROST_MODE, "Dew Point Set", 0 },
                         { TEMP_TO_EXTEND_DEFROST_TIME, "Temp to Extend", 0 },
                         { DELTA_AMBIENT_COIL, "Ambient - Coil", 0 },
                         { FAN_SPEED, "Fan Speed    ", -20 },
                         { AC_VOLTS, "AC Volts    ", 0 },
                         { AC_AMPS, "AC Amps     ", -10 },
                         { NWS_DEW_POINT, "NWS Dew Point", -40 },
                         { NO_HEAT_TEMP_SET, "Set No Heat Temp", 0 },
                         { C_UP_PER_C_DOWN, "C up per C down", 0 },
                         { COP_CRC, "COP & CRC", 0 } };

const int NUM_MENU_ITEMS = sizeof(menuCodes) / sizeof(menuCode);

char httpData[6000];
short outsideTemp = 0;
short radiantTemperature = 0;
float COP = 0;
float degreesToRaiseH2O = .7;
int8_t noHeatRequiredTempInC;
#define SECRET_SSID "121PageBrookRoad"
#define SECRET_PASS "1254019243"
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] =
  SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;
const char NWSserver[] = "api.weather.gov";
const char googleServer[] = "script.google.com";
WiFiSSLClient client;
const float dewPointNotFetchedTemp = -40;
float dewpoint = dewPointNotFetchedTemp;
bool dewPointFetchedFromNWS = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 500;
int loopCounter = 0;

int8_t menuIndex = 0;
short currentCode = 0;
const bool whenYouAreReady = false;
const bool doItNow = true;



#define TIMEOUT 5000  // Timeout in milliseconds
int taskInsertionPointer = 0;
int taskExecutingPointer = 0;
enum statuses { ACTIVE,
                READY,
                COMPLETED };
#define MAX_TASKS 20

struct Task {
  void (*initFunction)(short);
  void (*callbackFunction)();
  unsigned long startTime;
  statuses status;
  short code;
};

Task taskQueue[MAX_TASKS];

bool addTask(void (*initFunction)(short), void (*callbackFunction)(),
             short code) {
  if (taskQueue[taskInsertionPointer].status == COMPLETED) {
    taskQueue[taskInsertionPointer].initFunction = initFunction;
    taskQueue[taskInsertionPointer].callbackFunction = callbackFunction;
    taskQueue[taskInsertionPointer].startTime = millis();
    taskQueue[taskInsertionPointer].status = READY;
    taskQueue[taskInsertionPointer].code = code;
    taskInsertionPointer++;
    taskInsertionPointer = taskInsertionPointer % MAX_TASKS;
    return true;
  } else {
    return false;
  }
}

void executeTask() {
  if (taskQueue[taskExecutingPointer].status == READY) {
    taskQueue[taskExecutingPointer].startTime = millis();
    taskQueue[taskExecutingPointer].status = ACTIVE;
    taskQueue[taskExecutingPointer].initFunction(
      taskQueue[taskExecutingPointer].code);
  } else if (taskQueue[taskExecutingPointer].status == COMPLETED) {
    taskExecutingPointer++;
    taskExecutingPointer = taskExecutingPointer % MAX_TASKS;
  } else if (taskQueue[taskExecutingPointer].status == ACTIVE) {
    if (millis() - taskQueue[taskExecutingPointer].startTime > TIMEOUT) {
      taskQueue[taskExecutingPointer].status = COMPLETED;
      taskExecutingPointer++;
      taskExecutingPointer = taskExecutingPointer % MAX_TASKS;
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Hello");
  for (int i = 0; i < MAX_TASKS; i++) {
    taskQueue[i].status = COMPLETED;
  }
  // attempt to connect to WiFi network:
  Serial.println("attempt to connect to WiFi network");
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
  }
}

void loop() {
  executeTask();
  putTasksOnQueue();
  handleHTTPResponse();
}

void putTasksOnQueue() {

  logData();
}

float getSavedData(short code) {
  for (int i = 0; i < NUM_MENU_ITEMS; i++) {
    if (menuCodes[i].code == code) {
      return menuCodes[i].value;
    }
  }
  return 0;
}

void logData() {

  if (millis() - loggingTimer > (loggingIntervalMinutes * millisecondsInMinute)) {
    Serial.println("loop");
    addTask(&sendDatoToGoogleSheets, &readGoogleScriptResponse, 0);
    loggingTimer = millis();
  }
}

void sendDatoToGoogleSheets(short code) {
  Serial.println("sendDatoToGoogleSheets");
  if (client.connected()) {
    Serial.println("stopping client");
    client.stop();  // Disconnect from the current server
  }
  if (client.connect(googleServer, 443)) {
    // Make a HTTP request:

    char postbuffer[256];
    int offset = 0;

    // Assemble the string

    offset += sprintf(postbuffer + offset, "{\"%s\":%.2f,", "OutsideTemp", outsideTemp);
    offset += sprintf(postbuffer + offset, "\"%s\":%.2f,", "Dewpoint", dewpoint);
    offset += sprintf(postbuffer + offset, "\"%s\":%.2f,", "Defrost", getSavedData(DEFROST_STATUS));
    offset += sprintf(postbuffer + offset, "\"%s\":%.2f,", "OutletWaterTemp", getSavedData(Outlet_water_temperature));
    offset += sprintf(postbuffer + offset, "\"%s\":%.2f,", "InletWaterTemp", getSavedData(Inlet_water_temperature));
    offset += sprintf(postbuffer + offset, "\"%s\":%.2f,", "ExteriorCoilTemp", getSavedData(EXT_COIL_TEMP));
    offset += sprintf(postbuffer + offset, "\"%s\":%.2f,", "FanSpeed", getSavedData(FAN_SPEED));
    offset += sprintf(postbuffer + offset, "\"%s\":%.2f}", "ACAmps", getSavedData(AC_AMPS));
    client.println("POST /macros/s/AKfycbyn5LfGotefyp8gp0_AP1Z9V2bO565uR3qoGofKCa6OCWXk2uuRFka8ZZXS-oxmwvvs/exec HTTP/1.1");
    client.print("Host: ");
    client.println(googleServer);
    client.println("user-agent: (vonroesgen.com, claude@vonroesgen.com)");
    client.print("Content-length: ");
    client.println(strlen(postbuffer));
    client.println("Connection: close");
    client.println();
    client.println(postbuffer);
    Serial.println("Sent data to Google Sheets");
    Serial.print(postbuffer);
  }
}




void readGoogleScriptResponse() {
  /* ---------------------------------------------------------------------- */

  taskQueue[taskExecutingPointer].status = COMPLETED;
}

/* --------------------------------------------------------------------------
 */
void handleHTTPResponse() {
  /* --------------------------------------------------------------------------
   */
  uint32_t data_num = 0;
  bool inHeader = true;
  while (client.available() && data_num < 6000) {
    while (inHeader) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        inHeader = false;
      }
    }
    /* actual data reception */
    char c = client.read();
    httpData[data_num++] = c;
  }

  if (data_num == 0) {
    return;
  } else {
    httpData[data_num] = 0;
  }
  client.stop();
  Serial.println("Received data to Google Sheets");
  Serial.print(httpData);
  taskQueue[taskExecutingPointer].callbackFunction();
}

short convertUnSignedByteToSigned(unsigned short uByte) {
  if (uByte < 128) {
    return uByte;
  } else {
    return uByte - 256;
  }
}

short calcRadiantFloorTemperature(short outsideTemperature) {
  return noHeatRequiredTempInC + ((noHeatRequiredTempInC - outsideTemperature) * degreesToRaiseH2O);
}
