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

#define OFF 0x0
#define ON 0x1

#define SERIAL_BUFFER_SIZE 16
#define outDoorResetIntervalMinutes 60
#define lcdLEDDisplayIntervalSeconds 100
#define NO_HEAT_REQUIRED_TEMP_IN_C 22
#define NO_HEAT_REQUIRED_HI_LIMIT 25
#define NO_HEAT_REQUIRED_LO_LIMIT 18
#define DEGREES_C_TO_RAISE_H20 .7f
#define DEGREES_C_TO_RAISE_H20_HI_LIMIT 1.0f
#define DEGREES_C_TO_RAISE_H20_LO_LIMIT 0.5f

enum responseType { SET_RESPONSE, GET_RESPONSE };

enum responseType setOrGet;

Adafruit_RGBLCDShield lcd;

byte serialReceiveBuffer[SERIAL_BUFFER_SIZE];
int8_t serialReceiveBufferIndex = 0;
int bufferComplete = 0;
void (*serialBufferCallback)();
unsigned short crc16;
int led = 13;
int EN = 2;
unsigned long outDoorResetTimer =
    millis() - (outDoorResetIntervalMinutes * 60000L);
unsigned long ledDisplayTimer = millis();

const unsigned long dewPointUpdateInterval = 3605 * 1000L;
unsigned long lastDewPointUpdateTime = millis() - dewPointUpdateInterval;

#define DEFROST_STATUS 2136               // defrost is bit 5
#define DELTA_AMBIENT_COIL 2040          // Parameter 32
#define TEMP_TO_EXTEND_DEFROST_TIME 2039 // Parameter 31
#define COIL_TEMP_FOR_DEFROST_MODE 2038  // Parameter 30
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

unsigned short menuCodes[] = {ON_OFF,
                              DEFROST_STATUS,
                              HOT_WATER_SET_POINT,
                              WATER_TANK_TEMP,
                              Outlet_water_temperature,
                              Inlet_water_temperature,
                              AMBIENT_TEMP,
                              EXT_COIL_TEMP,
                              COOL_COIL_TEMP,
                              COIL_TEMP_FOR_DEFROST_MODE,
                              TEMP_TO_EXTEND_DEFROST_TIME,
                              DELTA_AMBIENT_COIL,
                              FAN_SPEED,
                              AC_VOLTS,
                              AC_AMPS,
                              NWS_DEW_POINT,
                              NO_HEAT_TEMP_SET,
                              C_UP_PER_C_DOWN,
                              COP_CRC};
char menu[][17] = {
    "On or Off   ",     "Defrost Status  ", "Temp Set    ",   "Temp Tank   ",
    "Temp H2O >  ",     "Temp H2O <   ",    "Temp Air     ",  "Temp Ext Coil",
    "Temp Cool Coil",   "Dew Point Set",    "Temp to Extend", "Ambient - Coil",
    "Fan Speed    ",    "AC Volts    ",     "AC Amps     ",   "NWS Dew Point",
    "Set No Heat Temp", "C up per C down",  "COP & CRC"};

const int NUM_MENU_ITEMS = sizeof(menuCodes) / sizeof(short);

short outsideTemp = 0;
short radiantTemperature = 0;
float COP = 0;
float degreesToRaiseH2O = .7;
int8_t noHeatRequiredTempInC;
#define SECRET_SSID "121PageBrookRoad"
#define SECRET_PASS ""
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] =
    SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;
char server[] = "api.weather.gov";
WiFiSSLClient client;
const float dewPointNotFetchedTemp = -40;
float dewpoint = dewPointNotFetchedTemp;
bool dewPointFetchedFromNWS = false;

void formatPrint(short number) {
  if (number < 10 && number > 0) {
    lcd.print(" ");
  }
  if (number < 100 && number > 0) {
    lcd.print(" ");
  }
  lcd.print(number);
}

void setup() {
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
  }
  Serial1.begin(2400, SERIAL_8E1);
  pinMode(led, OUTPUT);
  lcd.begin(16, 2);
  lcd.print("ModBus");
  lcd.setCursor(0, 1);
  lcd.print("MonoBloc");

  EEPROM.get(sizeof(degreesToRaiseH2O), noHeatRequiredTempInC);
  if (noHeatRequiredTempInC > NO_HEAT_REQUIRED_HI_LIMIT ||
      noHeatRequiredTempInC < NO_HEAT_REQUIRED_LO_LIMIT) {
    noHeatRequiredTempInC = NO_HEAT_REQUIRED_TEMP_IN_C;
    EEPROM.put(sizeof(degreesToRaiseH2O), noHeatRequiredTempInC);
  }
  EEPROM.get(0, degreesToRaiseH2O);
  if (isnan(degreesToRaiseH2O) ||
      degreesToRaiseH2O > DEGREES_C_TO_RAISE_H20_HI_LIMIT ||
      degreesToRaiseH2O < DEGREES_C_TO_RAISE_H20_LO_LIMIT) {
    degreesToRaiseH2O = DEGREES_C_TO_RAISE_H20;
    EEPROM.put(0, degreesToRaiseH2O);
  }

  digitalWrite(led, 1 - digitalRead(led));
}

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 500;
int loopCounter = 0;

int8_t menuIndex = 0;
short currentCode = 0;
bool whenYouAreReady = false;
bool doItNow = true;

void loop() {

  requestDewPointFromNWS(whenYouAreReady);
  handleMODBUSandButtons();
}

void requestDewPointFromNWS(bool whenToDoIt) {
  if ((millis() - lastDewPointUpdateTime > dewPointUpdateInterval) ||
      whenToDoIt) {
    lastDewPointUpdateTime = millis();
    send_http_request();
  }
  read_http_response();
  if (dewPointFetchedFromNWS) {
    setMonoBlocTemperature(COIL_TEMP_FOR_DEFROST_MODE, dewpoint);
    dewPointFetchedFromNWS = false;
  }
}

void handleMODBUSandButtons() {

  while (Serial1.available() > 0) {
    if (serialReceiveBufferIndex < SERIAL_BUFFER_SIZE) {
      serialReceiveBuffer[serialReceiveBufferIndex] = Serial1.read();
      serialReceiveBufferIndex++;
    } else {
      serialReceiveBufferIndex = 0;
      serialReceiveBuffer[serialReceiveBufferIndex] = Serial1.read();
    }
  }

  if (bufferComplete && serialReceiveBufferIndex >= bufferComplete) {
    bufferComplete = 0;
    (*serialBufferCallback)();
    return;
  }

  if (millis() > ((outDoorResetIntervalMinutes * 60000) + outDoorResetTimer)) {
    requestDataFromMonoBus(AMBIENT_TEMP, &setRadiantFloorTemperatureCallback);
    outDoorResetTimer = millis();
    return;
  }

  if (millis() > ledDisplayTimer + (lcdLEDDisplayIntervalSeconds * 1000)) {
    lcd.setBacklight(OFF);
    ledDisplayTimer = millis();
    return;
  }

  uint8_t buttons = lcd.readButtons();

  if (buttons) {
    ledDisplayTimer = millis();
    if (millis() > debounceDelay + lastDebounceTime) {
      lcd.setBacklight(ON);
      lastDebounceTime = millis();
      if (buttons & BUTTON_UP) {
        menuIndex--;
        if (menuIndex < 0) {
          menuIndex += NUM_MENU_ITEMS;
        }
      } else if (buttons & BUTTON_DOWN) {
        menuIndex++;
        menuIndex = menuIndex % NUM_MENU_ITEMS;
      } else if (buttons & BUTTON_LEFT) {
        if (menuCodes[menuIndex] == NO_HEAT_TEMP_SET) {
          noHeatRequiredTempInC--;
          if (noHeatRequiredTempInC < NO_HEAT_REQUIRED_LO_LIMIT) {
            noHeatRequiredTempInC = NO_HEAT_REQUIRED_LO_LIMIT;
          }
          lcd.setCursor(0, 1);
          lcd.print(noHeatRequiredTempInC);
        } else if (menuCodes[menuIndex] == C_UP_PER_C_DOWN) {
          degreesToRaiseH2O -= 0.1f;
          if (degreesToRaiseH2O < DEGREES_C_TO_RAISE_H20_LO_LIMIT) {
            degreesToRaiseH2O = DEGREES_C_TO_RAISE_H20_LO_LIMIT;
          }
          lcd.setCursor(0, 1);
          lcd.print(degreesToRaiseH2O);
        }
        return;
      } else if (buttons & BUTTON_RIGHT) {
        if (menuCodes[menuIndex] == NO_HEAT_TEMP_SET) {
          noHeatRequiredTempInC++;
          if (noHeatRequiredTempInC > NO_HEAT_REQUIRED_HI_LIMIT) {
            noHeatRequiredTempInC = NO_HEAT_REQUIRED_HI_LIMIT;
          }
          lcd.setCursor(0, 1);
          lcd.print(noHeatRequiredTempInC);
        } else if (menuCodes[menuIndex] == C_UP_PER_C_DOWN) {
          degreesToRaiseH2O += 0.1f;
          if (degreesToRaiseH2O > DEGREES_C_TO_RAISE_H20_HI_LIMIT) {
            degreesToRaiseH2O = DEGREES_C_TO_RAISE_H20_HI_LIMIT;
          }
          lcd.setCursor(0, 1);
          lcd.print(degreesToRaiseH2O);
        }
        return;
      } else if (buttons & BUTTON_SELECT) {
        if (menuCodes[menuIndex] == NO_HEAT_TEMP_SET) {
          EEPROM.put(sizeof(degreesToRaiseH2O), noHeatRequiredTempInC);
          lcd.setCursor(0, 1);
          lcd.print(noHeatRequiredTempInC);
          lcd.print(" saved");
        } else if (menuCodes[menuIndex] == C_UP_PER_C_DOWN) {
          EEPROM.put(0, degreesToRaiseH2O);
          lcd.setCursor(0, 1);
          lcd.print(degreesToRaiseH2O);
          lcd.print(" saved");
        } else if (menuCodes[menuIndex] == COIL_TEMP_FOR_DEFROST_MODE) {
          if (dewpoint > dewPointNotFetchedTemp) {
            setMonoBlocTemperature(COIL_TEMP_FOR_DEFROST_MODE, dewpoint);
          }
        } else if (menuCodes[menuIndex] == TEMP_TO_EXTEND_DEFROST_TIME) {
          if (dewpoint > dewPointNotFetchedTemp) {
            setMonoBlocTemperature(TEMP_TO_EXTEND_DEFROST_TIME, dewpoint);
          }
        }

        return;
      }

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(menu[menuIndex]);
      if (menuCodes[menuIndex] >= ON_OFF) {
        requestDataFromMonoBus(menuCodes[menuIndex],
                               &displayMonoBusGetResponse);
      } else if (menuCodes[menuIndex] == NWS_DEW_POINT) {
        requestDewPointFromNWS(doItNow);
        lcd.setCursor(0, 1);
        formatPrint(dewpoint);
        lcd.print("C ");
        lcd.print((dewpoint * 2.2) + 32);
        lcd.print("F");
      } else if (menuCodes[menuIndex] == COP_CRC) {
        lcd.setCursor(0, 1);
        lcd.print(calcCOP(radiantTemperature, outsideTemp));
        lcd.print(" & ");
        lcd.print(crc16, HEX);
      } else if (menuCodes[menuIndex] == NO_HEAT_TEMP_SET) {
        lcd.setCursor(0, 1);
        lcd.print(noHeatRequiredTempInC);
      } else if (menuCodes[menuIndex] == C_UP_PER_C_DOWN) {
        lcd.setCursor(0, 1);
        lcd.print(degreesToRaiseH2O);
      }
    }
  }
}

unsigned short CRC16(byte *nData, unsigned short wLength) {
  static unsigned short wCRCTable[] = {
      0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241, 0XC601,
      0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440, 0XCC01, 0X0CC0,
      0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40, 0X0A00, 0XCAC1, 0XCB81,
      0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841, 0XD801, 0X18C0, 0X1980, 0XD941,
      0X1B00, 0XDBC1, 0XDA81, 0X1A40, 0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01,
      0X1DC0, 0X1C80, 0XDC41, 0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0,
      0X1680, 0XD641, 0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081,
      0X1040, 0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
      0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441, 0X3C00,
      0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41, 0XFA01, 0X3AC0,
      0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840, 0X2800, 0XE8C1, 0XE981,
      0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41, 0XEE01, 0X2EC0, 0X2F80, 0XEF41,
      0X2D00, 0XEDC1, 0XEC81, 0X2C40, 0XE401, 0X24C0, 0X2580, 0XE541, 0X2700,
      0XE7C1, 0XE681, 0X2640, 0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0,
      0X2080, 0XE041, 0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281,
      0X6240, 0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
      0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41, 0XAA01,
      0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840, 0X7800, 0XB8C1,
      0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41, 0XBE01, 0X7EC0, 0X7F80,
      0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40, 0XB401, 0X74C0, 0X7580, 0XB541,
      0X7700, 0XB7C1, 0XB681, 0X7640, 0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101,
      0X71C0, 0X7080, 0XB041, 0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0,
      0X5280, 0X9241, 0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481,
      0X5440, 0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
      0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841, 0X8801,
      0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40, 0X4E00, 0X8EC1,
      0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41, 0X4400, 0X84C1, 0X8581,
      0X4540, 0X8701, 0X47C0, 0X4680, 0X8641, 0X8201, 0X42C0, 0X4380, 0X8341,
      0X4100, 0X81C1, 0X8081, 0X4040};

  byte nTemp;
  unsigned short wCRCWord = 0xFFFF;

  while (wLength--) {
    nTemp = *nData++ ^ wCRCWord;
    wCRCWord >>= 8;
    wCRCWord ^= wCRCTable[nTemp];
  }
  return wCRCWord;
}

/* -------------------------------------------------------------------------- */
void send_http_request() {

  if (client.connect(server, 443)) {

    // Make a HTTP request:
    client.println("GET /stations/KBED/observations/latest HTTP/1.1");
    client.println("Host: api.weather.gov");
    client.println("user-agent: (vonroesgen.com, claude@vonroesgen.com)");
    client.println("Connection: close");
    client.println();
  }
}
/* -------------------------------------------------------------------------- */
void read_http_response() {
  /* --------------------------------------------------------------------------
   */
  uint32_t data_num = 0;

  char jsonData[6000];
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
    jsonData[data_num++] = c;
  }

  if (data_num == 0) {
    return;
  } else {
    jsonData[data_num] = 0;
  }
  client.stop();
  JSONVar apiJSON = JSON.parse(jsonData);
  if (JSON.typeof(apiJSON) == "undefined") {
    return;
  }

  if (apiJSON.hasOwnProperty("properties")) {
    if ((String)apiJSON["properties"]["dewpoint"]["qualityControl"] == "V") {
      dewpoint = (double)apiJSON["properties"]["dewpoint"]["value"];
      dewPointFetchedFromNWS = true;
    }
  }
}
short convertUnSignedByteToSigned(unsigned short uByte) {
  if (uByte < 128) {
    return uByte;
  } else {
    return uByte - 256;
  }
}

void requestDataFromMonoBus(short code, void (*rs485Callback)()) {
  byte byts[8] = {1, 3, (byte)(code >> 8), (byte)(code % 256), 0, 1, 0, 0};
  crc16 = CRC16(byts, 6);
  byts[6] = crc16 % 256;
  byts[7] = crc16 >> 8;
  serialBufferCallback = rs485Callback;
  serialReceiveBufferIndex = 0;
  bufferComplete = 7;
  Serial1.write(byts, 8);
  Serial1.flush();
}

void displaySerialReceiveBuffer() {
  lcd.clear();
  lcd.setCursor(0, 0);
  for (int i = 0; i < SERIAL_BUFFER_SIZE; i++) {
    if (i == 8) {
      lcd.setCursor(0, 1);
    }
    if (serialReceiveBuffer[i] < 16) {
      lcd.print("0");
    }
    lcd.print(serialReceiveBuffer[i], HEX);
  }
}

void setMonoBlocTemperature(short code, short temperature) {
  currentCode = code;
  byte byts[8] = {1, 6, (byte)(code >> 8), (byte)(code % 256), 0, 1, 0, 0};
  byts[4] = temperature >> 8;
  byts[5] = temperature % 256;
  crc16 = CRC16(byts, 6);
  byts[6] = crc16 % 256;
  byts[7] = crc16 >> 8;
  serialReceiveBufferIndex = 0;
  bufferComplete = 8;
  serialBufferCallback = &parseMonoBusSetResponse;
  Serial1.write(byts, 8);
  Serial1.flush();
}

void parseMonoBusSetResponse() {

  // Set responses come back with the CRC at byte locations 6 and 7 (zero based)
  if (!checkCRC(SET_RESPONSE)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Set Code:");
    lcd.print(currentCode);
    lcd.setCursor(0, 1);
    lcd.print("Reset CRC failed");
  }
  return;
}

short calcRadiantFloorTemperature(short outsideTemperature) {
  return noHeatRequiredTempInC +
         ((noHeatRequiredTempInC - outsideTemperature) * degreesToRaiseH2O);
}

bool checkCRC(enum responseType setOrGet) {
  if (setOrGet == SET_RESPONSE) {
    // Set responses come back with the CRC at byte locations 6 and 7 (zero
    // based)
    crc16 = CRC16(serialReceiveBuffer, 6);
    if (serialReceiveBuffer[6] == (crc16 % 256) &&
        serialReceiveBuffer[7] == (crc16 >> 8)) {
      return true;
    }
  } else {
    // Get responses come back with the CRC at byte locations 5 and 6 (zero
    // based)
    crc16 = CRC16(serialReceiveBuffer, 5);
    if (serialReceiveBuffer[5] == (crc16 % 256) &&
        serialReceiveBuffer[6] == (crc16 >> 8)) {
      return true;
    }
  }
  return false;
}

void displayMonoBusGetResponse() {
  short data;
  // Get responses come back with the CRC at byte locations 5 and 6 (zero based)
  if (checkCRC(GET_RESPONSE)) {
    if (menuCodes[menuIndex] == AC_VOLTS) {
      data = serialReceiveBuffer[4];
    } else if (menuCodes[menuIndex] == DEFROST_STATUS) {
      data = (data >> 5) & 1;
    } else {
      data = convertUnSignedByteToSigned(serialReceiveBuffer[4]);
    }
    lcd.setCursor(0, 1);
    formatPrint(data);
    if (menuCodes[menuIndex] >= HOT_WATER_SET_POINT &&
        menuCodes[menuIndex] <= AMBIENT_TEMP) {
      lcd.print("C ");
      lcd.print((data * 2.2) + 32);
      lcd.print("F");
    }
  } else {
    displaySerialReceiveBuffer();
    lcd.setCursor(0, 1);
    lcd.print("crc ");
    lcd.print(crc16, HEX);
  }
}

void setRadiantFloorTemperatureCallback() {
  // Get responses come back with the CRC at byte locations 5 and 6 (zero based)
  if (checkCRC(GET_RESPONSE)) {
    outsideTemp = convertUnSignedByteToSigned(serialReceiveBuffer[4]);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Get outside temp");
    lcd.setCursor(0, 1);
    lcd.print("Fail crc");
    lcd.print(crc16, HEX);
    return;
  }
  if (outsideTemp > -23 && outsideTemp < 25) {
    radiantTemperature = calcRadiantFloorTemperature(outsideTemp);
    setMonoBlocTemperature(HOT_WATER_SET_POINT, radiantTemperature);
  }
  return;
}

float interpolateCOP(int8_t HiWaterTemp, int8_t HiAirTemp, int8_t waterTemp,
                     int8_t airTemp, int8_t deltaWater, int8_t deltaAir,
                     float copHiWaterHiAir, float copHiWaterLowAir,
                     float copLoWaterHiAir, float copLoWaterLowAir) {
  float hiAirMidWaterCOP =
      copLoWaterLowAir + ((copLoWaterHiAir - copHiWaterHiAir) *
                          (waterTemp - HiWaterTemp) / deltaWater);
  float loWaterMidAirCOP =
      copLoWaterLowAir +
      ((copLoWaterHiAir - copLoWaterLowAir) * (airTemp - HiAirTemp) / deltaAir);
  return (hiAirMidWaterCOP + loWaterMidAirCOP) / 2;
}

int8_t airTemps[] = {15, 7, 2, -7, -12, -15, -20, -25};
int8_t waterTemps[] = {50, 45, 41, 35, 30, 20};
float waterCOPs[7][9] = {{4.25, 3.5, 2.81, 2.65, 2.41, 2.09, 1.86, 1.32, 1.23},
                         {4.75, 3.68, 2.99, 2.84, 2.65, 2.21, 2.01, 1.57, 1.47},
                         {5.1, 3.92, 3.05, 2.96, 2.87, 2.48, 2.22, 1.73, 1.59},
                         {5.51, 4.15, 3.65, 3.28, 2.94, 2.63, 2.32, 2.01, 1.65},
                         {5.92, 4.76, 4.2, 3.63, 3.2, 2.95, 2.60, 2.17, 1.78},
                         {6.93, 5.43, 4.85, 4.3, 3.7, 3.31, 2.81, 2.48, 2.03},
                         {7.85, 6.30, 5.45, 4.81, 4.27, 3.72, 3.12, 2.79, 2.3}};

float calcCOP(int8_t waterTemp, int8_t airTemp) {
  if (waterTemp >= waterTemps[0]) {
    return cop(waterTemps[0], waterTemp, airTemp, waterTemps[0] - waterTemps[1],
               waterCOPs[0], waterCOPs[1]);
  }
  if (waterTemp >= waterTemps[1]) {
    return cop(waterTemps[1], waterTemp, airTemp, waterTemps[1] - waterTemps[2],
               waterCOPs[1], waterCOPs[2]);
  }
  if (waterTemp >= waterTemps[2]) {
    return cop(waterTemps[2], waterTemp, airTemp, waterTemps[2] - waterTemps[3],
               waterCOPs[2], waterCOPs[3]);
  }
  if (waterTemp >= waterTemps[3]) {
    return cop(waterTemps[3], waterTemp, airTemp, waterTemps[3] - waterTemps[4],
               waterCOPs[3], waterCOPs[4]);
  }
  if (waterTemp >= waterTemps[4]) {
    return cop(waterTemps[4], waterTemp, airTemp, waterTemps[4] - waterTemps[5],
               waterCOPs[4], waterCOPs[5]);
  }
  return 0;
}

float cop(int8_t highWaterTemp, int8_t waterTemp, int8_t airTemp,
          int8_t deltaWater, float *hiWaterCOPs, float *loWaterCOPs) {
  if (airTemp >= airTemps[0]) {
    return interpolateCOP(highWaterTemp, airTemps[0], waterTemp, airTemp,
                          deltaWater, airTemps[0] - airTemps[1], hiWaterCOPs[0],
                          hiWaterCOPs[1], loWaterCOPs[0], loWaterCOPs[1]);
  } else if (airTemp >= airTemps[1]) {
    return interpolateCOP(highWaterTemp, airTemps[1], waterTemp, airTemp,
                          deltaWater, airTemps[1] - airTemps[2], hiWaterCOPs[1],
                          hiWaterCOPs[2], loWaterCOPs[1], loWaterCOPs[2]);
  } else if (airTemp >= airTemps[2]) {
    return interpolateCOP(highWaterTemp, airTemps[2], waterTemp, airTemp,
                          deltaWater, airTemps[2] - airTemps[3], hiWaterCOPs[2],
                          hiWaterCOPs[3], loWaterCOPs[2], loWaterCOPs[3]);
  } else if (airTemp >= airTemps[3]) {
    return interpolateCOP(highWaterTemp, airTemps[3], waterTemp, airTemp,
                          deltaWater, airTemps[3] - airTemps[4], hiWaterCOPs[3],
                          hiWaterCOPs[4], loWaterCOPs[3], loWaterCOPs[4]);
  } else if (airTemp >= airTemps[4]) {
    return interpolateCOP(highWaterTemp, airTemps[4], waterTemp, airTemp,
                          deltaWater, airTemps[4] - airTemps[5], hiWaterCOPs[4],
                          hiWaterCOPs[5], loWaterCOPs[4], loWaterCOPs[5]);
  } else if (airTemp >= airTemps[5]) {
    return interpolateCOP(highWaterTemp, airTemps[5], waterTemp, airTemp,
                          deltaWater, airTemps[5] - airTemps[6], hiWaterCOPs[5],
                          hiWaterCOPs[6], loWaterCOPs[5], loWaterCOPs[6]);
  } else if (airTemp >= airTemps[6]) {
    return interpolateCOP(highWaterTemp, airTemps[6], waterTemp, airTemp,
                          deltaWater, airTemps[6] - airTemps[7], hiWaterCOPs[6],
                          hiWaterCOPs[7], loWaterCOPs[6], loWaterCOPs[7]);
  }
  return 0;
}
