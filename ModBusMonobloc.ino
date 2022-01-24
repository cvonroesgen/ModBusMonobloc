#include <Adafruit_RGBLCDShield.h>

// These #defines make it easy to set the backlight color
#define OFF 0x0
#define ON 0x1
#define NUM_MENU_ITEMS 8
#define SERIAL_BUFFER_SIZE 16
#define outDoorResetIntervalMinutes 5
#define lcdLEDDisplayIntervalSeconds 10
Adafruit_RGBLCDShield lcd;

byte serialReceiveBuffer[SERIAL_BUFFER_SIZE];
int8_t serialReceiveBufferIndex = 0;
int bufferComplete = 0;
short (*serialBufferCallback)();
unsigned short crc16;
int led = 13;
int EN = 2;
unsigned long outDoorResetTimer = millis();
unsigned long ledDisplayTimer = millis();

unsigned short  menuCodes[NUM_MENU_ITEMS] = {2000, 2004, 2100, 2102, 2103, 2110, 2120, 2121};
char menu[NUM_MENU_ITEMS][14] = {
  "On or Off   ",
  "Temp Set    ", 
  "Temp Tank   ", 
  "Temp H2O >  ", 
  "Temp H2O <   ",
  "Temp Air     ", 
  "AC Volts    ", 
  "AC Amps     "
  };
short outsideTemp;
short radiantTemperature;
float COP = 0;
  


void formatPrint(short number)
{
  if (number < 10 && number > 0)
  {
    lcd.print(" ");
  }
if (number < 100 && number > 0)
  {
    lcd.print(" ");
  }  
  lcd.print(number);
}

void setup() {
  Serial.begin(2400, SERIAL_8E1);
  pinMode(led,OUTPUT);
  lcd.begin(16, 2);
  lcd.print("ModBus");
  lcd.setCursor(0,1);
  lcd.print("MonoBloc");

digitalWrite(led,1-digitalRead(led));
}
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 1000;
int loopCounter = 0;

int8_t menuIndex = 0;
 
void loop() {
  

  while(Serial.available() > 0) {
    if(serialReceiveBufferIndex < SERIAL_BUFFER_SIZE)
    {
      serialReceiveBuffer[serialReceiveBufferIndex] = Serial.read();
      serialReceiveBufferIndex++;
    }
  else
    {
     serialReceiveBufferIndex = 0;
     serialReceiveBuffer[serialReceiveBufferIndex] = Serial.read(); 
    }
  }

if(bufferComplete && serialReceiveBufferIndex >= bufferComplete)
  {
    bufferComplete = 0;
  (*serialBufferCallback)();
  return;
  }

if(millis() - outDoorResetTimer > outDoorResetIntervalMinutes * 60000)
  {
  setRadiantFloorTemperature();
  delay(500);
  outDoorResetTimer = millis();
  return;
  }

  if(millis() - ledDisplayTimer > lcdLEDDisplayIntervalSeconds * 1000)
  {
  lcd.setBacklight(OFF);
  ledDisplayTimer = millis();
  return;
  }

  uint8_t buttons = lcd.readButtons();

  if (buttons) {
    ledDisplayTimer = millis();
    if(millis() - lastDebounceTime > debounceDelay)
        {
        lcd.setBacklight(ON);
        lastDebounceTime = millis();
        if (buttons & BUTTON_UP) {
          menuIndex--;
        }
        else if (buttons & BUTTON_DOWN) {
          menuIndex++;
        }
        else if (buttons & BUTTON_LEFT) {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("COP: ");
          lcd.print(COP);
          lcd.setCursor(0,1);
          lcd.print("crc:");
          lcd.print(crc16, HEX);
          return;
        }
        else if (buttons & BUTTON_RIGHT) {
          displaySerialReceiveBuffer();          
          return;
        }
        else if (buttons & BUTTON_SELECT) {
          return;          
        }
        if(menuIndex < 0)
          {
          menuIndex += NUM_MENU_ITEMS;
          }
        menuIndex = menuIndex % NUM_MENU_ITEMS;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(menu[menuIndex]);
        requestDataForDisplayFromMonoBus(menuCodes[menuIndex]);        
      }
  }
}

 unsigned short CRC16 (byte *nData,  unsigned short wLength)
{
static  unsigned short wCRCTable[] = {
0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };

byte nTemp;
 unsigned short wCRCWord = 0xFFFF;

   while (wLength--)
   {
      nTemp = *nData++ ^ wCRCWord;
      wCRCWord >>= 8;
      wCRCWord ^= wCRCTable[nTemp];
   }
   return wCRCWord;

}


short convertUnSignedByteToSigned(unsigned short uByte)
{
  if (uByte < 128)
    {
    return uByte;
    } 
  else
    {
    return uByte - 256;
    }  
}        


void requestDataForDisplayFromMonoBus(short code)
{
  byte byts[8] = {1, 3, (byte) (code >> 8), (byte)(code % 256), 0, 1, 0, 0};
  crc16 = CRC16(byts, 6);
  byts[6] = crc16 % 256;
  byts[7] = crc16 >> 8;
  serialBufferCallback = &displayMonoBusGetResponse;
  serialReceiveBufferIndex = 0;
  bufferComplete = 7;
  
  Serial.write(byts, 8);
  Serial.flush();
  delay(100);
}

void requestDataFromMonoBus(short code)
{
  byte byts[8] = {1, 3, (byte) (code >> 8), (byte)(code % 256), 0, 1, 0, 0};
  crc16 = CRC16(byts, 6);
  byts[6] = crc16 % 256;
  byts[7] = crc16 >> 8;
  serialBufferCallback = &setRadiantFloorTemperatureCallback;
  serialReceiveBufferIndex = 0;
  bufferComplete = 7;
  Serial.write(byts, 8);
  Serial.flush();
  delay(100);
}


short displayMonoBusGetResponse()
{  
  crc16 = CRC16(serialReceiveBuffer, 5);
  short data;
  if(serialReceiveBuffer[5] == (crc16 % 256) && serialReceiveBuffer[6] == (crc16 >> 8))
    {
      if(menuCodes[menuIndex] == 2120)
        {
          data = serialReceiveBuffer[4];
        }
      else
        {
          data = convertUnSignedByteToSigned(serialReceiveBuffer[4]);
        }
    lcd.setCursor(0,1);
    formatPrint(data);
    if(menuCodes[menuIndex] >= 2004 && menuCodes[menuIndex] <= 2110)
      {
      lcd.print("C ");
      lcd.print((data * 2.2) + 32);
      lcd.print("F");

      }
    }
  else
    {
      displaySerialReceiveBuffer();
      lcd.setCursor(0,1);
      lcd.print("crc ");
      lcd.print(crc16, HEX);
    }  
}


void displaySerialReceiveBuffer()
{
  lcd.clear();
  lcd.setCursor(0,0);
  for(int i = 0; i < SERIAL_BUFFER_SIZE; i++)
    {
      if(i == 8)
      {
        lcd.setCursor(0,1);
      }
      if(serialReceiveBuffer[i] < 16)
        {
          lcd.print("0");
        }
      lcd.print(serialReceiveBuffer[i], HEX);
    }
}

void setMonoBlocTemperature(short temperature)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Reset to: ");
  lcd.print(temperature);
  short code = 2004;
  byte byts[8] = {1, 6, (byte) (code >> 8), (byte)(code % 256), 0, 1, 0, 0};
  byts[4] = temperature >> 8;
  byts[5] = temperature % 256;
  crc16 = CRC16(byts, 6);
  byts[6] = crc16 % 256;
  byts[7] = crc16 >> 8;
  serialReceiveBufferIndex = 0;
  bufferComplete = 8;
  serialBufferCallback = &parseMonoBusSetResponse;
  Serial.write(byts, 8);
  Serial.flush();
  delay(50);
}

short parseMonoBusSetResponse()
{
  crc16 = CRC16(serialReceiveBuffer, 6);
  if(serialReceiveBuffer[6] == (crc16 % 256) && serialReceiveBuffer[7] == (crc16 >> 8))
    {
      lcd.setCursor(0,1);
      lcd.print("Reset CRC success");
      return crc16;
    }
lcd.setCursor(0,1);
lcd.print("Reset CRC failed");
return crc16;
}

short calcRadiantFloorTemperature(short outsideTemperature)
{
  return 20 + ((20 - outsideTemperature) / 1.5);
}

short setRadiantFloorTemperature()
{
  requestDataFromMonoBus(2110);
  delay(100);
}

short setRadiantFloorTemperatureCallback()
{
  crc16 = CRC16(serialReceiveBuffer, 5);
  if(serialReceiveBuffer[5] == (crc16 % 256) && serialReceiveBuffer[6] == (crc16 >> 8))
    {
      if(menuCodes[menuIndex] == 2120)
        {
          outsideTemp = serialReceiveBuffer[4];
        }
      else
        {
          outsideTemp = convertUnSignedByteToSigned(serialReceiveBuffer[4]);
        }
    }
  else
    {
      displaySerialReceiveBuffer();
      lcd.setCursor(0,1);
      lcd.print("crc ");
      lcd.print(crc16, HEX);
      return 0;
    }
  if(outsideTemp > -23 && outsideTemp < 25)
    {
    radiantTemperature = calcRadiantFloorTemperature(outsideTemp);
    setMonoBlocTemperature(radiantTemperature);
    }
    COP = calcCOP(radiantTemperature, outsideTemp);
    lcd.print("COP ");
    lcd.print(COP);
  return radiantTemperature;
}

float interpolateCOP(int8_t HiWaterTemp, int8_t HiAirTemp, int8_t waterTemp, int8_t airTemp, int8_t deltaWater, int8_t deltaAir, float copHiWaterHiAir, float copHiWaterLowAir, float copLoWaterHiAir, float copLoWaterLowAir)
{
float hiAirMidWaterCOP = copLoWaterLowAir + ((copLoWaterHiAir - copHiWaterHiAir)*(waterTemp - HiWaterTemp)/deltaWater);
float loWaterMidAirCOP = copLoWaterLowAir + ((copLoWaterHiAir - copLoWaterLowAir)*(airTemp - HiAirTemp)/deltaAir);
return (hiAirMidWaterCOP  + loWaterMidAirCOP )/2;
}

int8_t airTemps[] = {15, 7, 2, -7, -12, -15, -20, -25};
int8_t waterTemps[] = {50, 45, 41, 35, 30, 20};
float waterCOPs[7][9] ={ 
                      {4.25, 3.5, 2.81, 2.65, 2.41, 2.09, 1.86, 1.32, 1.23},
                      {4.75, 3.68, 2.99, 2.84, 2.65, 2.21, 2.01, 1.57, 1.47},
                      {5.1, 3.92, 3.05, 2.96, 2.87, 2.48, 2.22, 1.73, 1.59},
                      {5.51,4.15,3.65,3.28, 2.94, 2.63, 2.32, 2.01, 1.65},
                      {5.92, 4.76, 4.2, 3.63, 3.2, 2.95, 2.60, 2.17, 1.78},
                      {6.93, 5.43, 4.85, 4.3, 3.7, 3.31, 2.81, 2.48, 2.03},
                      {7.85, 6.30, 5.45, 4.81, 4.27, 3.72, 3.12, 2.79, 2.3}
                      };
float calcCOP(int8_t waterTemp, int8_t airTemp)
{
if(waterTemp >= waterTemps[0])
  {
  return cop(waterTemps[0], waterTemp,  airTemp, waterTemps[0] - waterTemps[1], waterCOPs[0], waterCOPs[1]);
  }
if(waterTemp >= waterTemps[1])
  {
  return cop(waterTemps[1], waterTemp,  airTemp, waterTemps[1] - waterTemps[2], waterCOPs[1], waterCOPs[2]);
  }
if(waterTemp >= waterTemps[2])
  {
  return cop(waterTemps[2], waterTemp,  airTemp, waterTemps[2] - waterTemps[3], waterCOPs[2], waterCOPs[3]);
  }
if(waterTemp >= waterTemps[3])
  {
  return cop(waterTemps[3], waterTemp,  airTemp, waterTemps[3] - waterTemps[4], waterCOPs[3], waterCOPs[4]);
  }
  if(waterTemp >= waterTemps[4])
  {
  return cop(waterTemps[4], waterTemp,  airTemp, waterTemps[4] - waterTemps[5], waterCOPs[4], waterCOPs[5]);
  }
return 0;
}

float cop(int8_t highWaterTemp, int8_t waterTemp, int8_t airTemp, int8_t deltaWater, float* hiWaterCOPs, float* loWaterCOPs)
{
if(airTemp >= airTemps[0])
    {
    return interpolateCOP(highWaterTemp, airTemps[0], waterTemp, airTemp, deltaWater, airTemps[0] - airTemps[1], hiWaterCOPs[0], hiWaterCOPs[1], loWaterCOPs[0], loWaterCOPs[1]);
    }
  else if(airTemp >= airTemps[1])
    {
    return interpolateCOP(highWaterTemp, airTemps[1], waterTemp, airTemp, deltaWater, airTemps[1] - airTemps[2], hiWaterCOPs[1], hiWaterCOPs[2], loWaterCOPs[1], loWaterCOPs[2]);
    }
  else if(airTemp >= airTemps[2])
    {
    return interpolateCOP(highWaterTemp, airTemps[2], waterTemp, airTemp, deltaWater, airTemps[2] - airTemps[3], hiWaterCOPs[2], hiWaterCOPs[3], loWaterCOPs[2], loWaterCOPs[3]);
    }
  else if(airTemp >= airTemps[3])
    {
    return interpolateCOP(highWaterTemp, airTemps[3], waterTemp, airTemp, deltaWater, airTemps[3] - airTemps[4], hiWaterCOPs[3], hiWaterCOPs[4], loWaterCOPs[3], loWaterCOPs[4]);
    }
  else if(airTemp >= airTemps[4])
    {
    return interpolateCOP(highWaterTemp, airTemps[4], waterTemp, airTemp, deltaWater, airTemps[4] - airTemps[5], hiWaterCOPs[4], hiWaterCOPs[5], loWaterCOPs[4], loWaterCOPs[5]);
    }
  else if(airTemp >= airTemps[5])
    {
    return interpolateCOP(highWaterTemp, airTemps[5], waterTemp, airTemp, deltaWater, airTemps[5] - airTemps[6], hiWaterCOPs[5], hiWaterCOPs[6], loWaterCOPs[5], loWaterCOPs[6]);
    }
  else if(airTemp >= airTemps[6])
    {
    return interpolateCOP(highWaterTemp, airTemps[6], waterTemp, airTemp, deltaWater, airTemps[6] - airTemps[7], hiWaterCOPs[6], hiWaterCOPs[7], loWaterCOPs[6], loWaterCOPs[7]);
    }
return 0;
}