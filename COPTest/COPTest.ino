
#include <Adafruit_RGBLCDShield.h>

// These #defines make it easy to set the backlight color
#define OFF 0x0
#define ON 0x1
#define NUM_MENU_ITEMS 8
#define SERIAL_BUFFER_SIZE 16
#define outDoorResetIntervalMinutes 5
#define lcdLEDDisplayIntervalSeconds 10
Adafruit_RGBLCDShield lcd;


int led = 13;
int EN = 2;


void setup() {
  Serial.begin(9600);
  // put your setup code here, to run once:
pinMode(led,OUTPUT);
  lcd.begin(16, 2);
  float cop = calcCOP(38, 4);
  lcd.print(" cop:");
  lcd.print(cop);
  
}

void loop() {
  // put your main code here, to run repeatedly:

}

float interpolateCOP(int8_t HiWaterTemp, int8_t HiAirTemp, int8_t waterTemp, int8_t airTemp, int8_t deltaWater, int8_t deltaAir, float copHiWaterHiAir, float copHiWaterLowAir, float copLoWaterHiAir, float copLoWaterLowAir)
{
  Serial.println(HiWaterTemp);
  Serial.println(HiAirTemp);
   Serial.println();
 Serial.println(copHiWaterHiAir);
 Serial.println(copHiWaterLowAir);
 Serial.println(copLoWaterHiAir);
 Serial.println(copLoWaterLowAir);
 Serial.println();

float hiAirMidWaterCOP = copLoWaterLowAir + ((copLoWaterHiAir - copHiWaterHiAir)*(waterTemp - HiWaterTemp)/deltaWater);
float loWaterMidAirCOP = copLoWaterLowAir + ((copLoWaterHiAir - copLoWaterLowAir)*(airTemp - HiAirTemp)/deltaAir);

Serial.println(hiAirMidWaterCOP);
Serial.println(loWaterMidAirCOP);

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
