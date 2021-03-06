/*Slowly turn on a light at a scheduled time.
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <DS3232RTC.h> //For working with the RTC module
#include "LightRamp.h" //Provides LightRamp class for dimming
#include "Helper.h" //Helper functions

#define AC_LOAD 1
#define PUSHPIN 14 //should be interupt pin, in this case, interupt 14
#define TXPIN 10
#define RXPIN 9
#define ZX 2 //Zero crossing detector, should be an interput pin, in this case, interupt 2

#define RTC_STATUS 0x0F //The status register of the ds3232. Used here for checking the power status.

#define MAXBRIGHT 4
#define MINBRIGHT 124
#define MEMORYON //Select whether the previous alarm values are used or reset on power up.
#define MAXPACKETSIZE 12 //Maximum number of bytes a bt package can have

IntervalTimer ZXTimer; //Timer object that manages the firing of the triac
DS3232RTC RTC(true); //initialize the rtc object and wire.h instance
                     //If using a teensy or other board with multiple sda/scl pins, use the Wire.setSDA/SCL functions
Info Help(&Serial2);

//char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}; 
//Can be used in conjuction with the DateTime class' dayOfTheWeek() function
byte currentday = 0; //Used as a shorthand for the value of Time.weekday(). 0 is sunday.

time_t CurrentTime;
time_t LastTime;

byte alarmtime[7][2] = {{100,100},{6,20},{6,20},{6,20},{6,20},{6,20},{100,100}}; //hour and minute of alarm

bool alarmlightstatus = false;  //tracks whether the alarm has already gone off
volatile bool buttonstate = 0;

unsigned short hardtimeramp = 2000; //Time for the light to dim on a hard on/off press  milliseconds
unsigned long alarmtimeramp = 1800000; //Time for alarm light to reach max brightness
unsigned short adjusttimeramp = 500; //Time for light to change for a brightness adjustment

byte activeflag = 0; //Determines the active lightramp: 0=none, 1=button, 2=adjustment, 3=alarm
LightRamp ButtonRamp (1,&activeflag);
LightRamp AdjustRamp (2,&activeflag);
LightRamp AlarmRamp (3,&activeflag);

volatile byte dimming = MAXBRIGHT; //range is theoretically from 0-128, but limited to 4-124 due to timing limits
volatile byte wavecounter = 0; //counter used in timing interrupt
volatile boolean zerocross = 0; //Flag for zero crossing

void SetTimes(byte TimeInput[MAXPACKETSIZE]);
void SetAlarms(byte TimeInput[MAXPACKETSIZE]);
void ButtonPress();
void ZeroCross();
void DimCheck();
void BTUpdate();

byte freqstep = 65;  //For 50Hz, use 75
// It is calculated based on the frequency of your voltage supply (50Hz or 60Hz)
// and the number of brightness steps you want. 
// 
// Realize that there are 2 zerocrossing per cycle. This means
// zero crossing happens at 120Hz for a 60Hz supply or 100Hz for a 50Hz supply. 

// To calculate freqStep divide the length of one full half-wave of the power
// cycle (in microseconds) by the number of brightness steps. 
//
// (120 Hz=8333uS) / 128 brightness steps = 65 uS / brightness step
// (100Hz=10000uS) / 128 steps = 75uS/step
////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial2.begin(115200);  //Serial2 are hardware serial pins 9/10 on teensy

  #ifdef MEMORYON  //Read the alarmtime values from memory
  for (byte i = 1; i<=14; i+=2){
    alarmtime [static_cast<byte>(i/2)][0] = EEPROM.read(i);  //not sure if the type casting is needed, but ehh
    alarmtime [static_cast<byte>(i/2)][1] = EEPROM.read(i+1);
  }
  Serial2.println("EEPROM READ");
  #endif
  
  //Check if the RTC has lost power previously, and if so set the time based on the compile time.
  //Code is based on Adafruit's RTClib lostPower() function since DS3232RTC.h doesn't include a similar function.
  //readRTC(RTC_STATUS) returns the byte value of the RTC_STATUS register.
  // >>7 then shifts the bits 7 places to the right, leaving only the OSF bit, which is a value of 1 if power was lost
  
  if (RTC.readRTC(RTC_STATUS)>>7){
    Serial2.println("Setting time by compile time");
    setTime(cvt_date(__DATE__,__TIME__)); //Sets the system clock
    RTC.set(now()); //Sets the rtc based on the system clock
  }
  
  setSyncProvider(RTC.get); //Syncs the arduino's internal clock with the ds3232
  if (timeStatus() != timeSet){
    Serial2.println("Time Sync Error");
  }

  Serial2.println("Initializing");
  Help.SystemTime();
  Help.SystemDate();

  LastTime = now(); //Lasttime needs to be initiated, otherwise the first pass through the main loop thinks there is a day change
  currentday = weekday(LastTime);
  
  pinMode(AC_LOAD,OUTPUT);
  pinMode(PUSHPIN,INPUT);
  pinMode(ZX, INPUT);

  attachInterrupt(PUSHPIN,ButtonPress,RISING);
  attachInterrupt(ZX,ZeroCross,RISING);
  ZXTimer.begin(DimCheck, freqstep);
  ZXTimer.priority (10); //0-255, lower is higher priority
  
}
////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {  
  CurrentTime = now();
  
  //If the day has changed, reset the alarm flag so it can go off again. Also increment the day counter
  if (abs(day(CurrentTime)-day(LastTime))){
    alarmlightstatus=false;
    currentday = weekday(CurrentTime); // Update the day variable
  }    
  
  //If the current time is past the alarm time, and the light is less than half max brightness, and the alarm is set / valid
  if (hour(CurrentTime)>=alarmtime[currentday-1][0] && minute(CurrentTime)>=alarmtime[currentday-1][1] && dimming>=MINBRIGHT/2 && alarmtime[currentday-1][0]<24 &&!alarmlightstatus){
    Serial2.println("triggered");
    alarmlightstatus=true;
    activeflag = 3;
    AlarmRamp.Set(&dimming, MAXBRIGHT, alarmtimeramp);
    buttonstate=0;
  }

  LastTime = CurrentTime;
  
  if (buttonstate!=0){
    buttonstate = 0;
    if (dimming<MINBRIGHT){
      ButtonRamp.Set(&dimming, MINBRIGHT, hardtimeramp);
      activeflag = 1;
    }
    else{
      ButtonRamp.Set(&dimming, MAXBRIGHT, hardtimeramp);
      activeflag = 1;
    }
  }

  BTUpdate();

  ButtonRamp.Update();
  AdjustRamp.Update();
  AlarmRamp.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////

void SetTimes(byte TimeInput[MAXPACKETSIZE]){
  //Sets current time and alarm time
  byte chour = TimeInput[6]*10 + TimeInput[7];
  byte cminute = TimeInput[8]*10 + TimeInput[9];
  byte csecond = TimeInput[10]*10 + TimeInput[11];
  byte cday = TimeInput[4]*10 + TimeInput[5];
  byte cmonth = TimeInput[2]*10 + TimeInput[3];
  byte cyear = 2000 + TimeInput[0]*10 + TimeInput[1];
  setTime(chour,cminute,csecond,cday,cmonth,cyear); //Sets the system clock
  RTC.set(now()); //Sets the rtc based on the system clock
}

void SetAlarms(byte TimeInput[MAXPACKETSIZE]){
  //Sets the alarmtime array then saves it in eeprom
  for (byte i=0; i<7; i++){
    if (TimeInput[i]){ //The first 7 bytes of TimeInput are either 0 or 1
      alarmtime[i][0] = TimeInput [7]*10 + TimeInput[8];
      alarmtime[i][1] = TimeInput [9]*10 + TimeInput[10];
    }
  }
  
  for (byte i = 1; i<=14; i+=2){
    EEPROM.update(i, alarmtime [static_cast<byte>(i/2)][0]);  //EEPROM.update() doesn't write to memory unless the value is different
    EEPROM.update(i+1, alarmtime [static_cast<byte>(i/2)][1]);
  }  
}

void ButtonPress(){
  buttonstate=1;
}

void ZeroCross(){
  zerocross = true;
  wavecounter = 0; //reset the counter
  digitalWrite(AC_LOAD, LOW);    //Turn off TRIAC
}

void DimCheck() {                 
  if(zerocross == true) {             
    if(wavecounter>=dimming) {              
      digitalWrite(AC_LOAD, HIGH); // turn on light       
      wavecounter=0;  // reset time step counter                         
      zerocross = false; //reset zero cross detection
    } 
    else 
      wavecounter++; // increment time step counter                                                     
  }                                  
}

void BTUpdate(){
  /*Checks whether input from the HC05 is present and a valid packet, then proceeds with the appropriate action
   * Valid packets are:
   * Light Adjustment packet
   *  Format is 'H'XY, where H is the header char H, and XY is a number from 00 to 99
   *  EX: maximum brightness, H99
   *Time Adjustment packet
   *  Format is 'T'YrMoDyHrMnSc, where T is the header char T, then the year, month, day, hour, minute, and second
   *  All values should be 2 digits, so 2019 is 19 and 2pm is 14, and the 5th day is 05
   *  EX: Jan. 1, 2019, 11:11 pm is T190101231100
   *Alarm Adjustment
   *  Format is 'A'0111110HrMn. A is the header char A. The next 7 bytes represent Sunday to Saturday, either true (1) or false (0).
   *  HrMn is then the hour and minute the alarm should BEGIN the light ramp. The alarm time is only applied to the days that are 
   *  marked true. It overwrites existing alarms for days marked as true, but doesn't change or remove alarms on days marked false.
   *  EX: For the light to reach maximum brightness at 7am on weekdays with a 30minute ramp, A01111100630
   * 
   */
  if (Serial2.available() ){
    char btchar = Serial2.read();
    byte btinput[MAXPACKETSIZE];
    
    delay(2300); //I'm not sure if it's the terminal app I'm using or the ble package being too many bytes (probably), 
                 //but a delay is needed to allow the hc05 to receive the entire package.
    
    if (btchar == 'H'){ // a 'H' header specifies a slider adjustment to the light level
      btchar = Serial2.read();
      byte tempbrightness = (btchar - '0')*10 + (Serial2.read() - '0'); //This is a brightness percentage from 0 to 99.
      if (tempbrightness <=100 && tempbrightness >=0){
        Serial2.println((byte)(MINBRIGHT - tempbrightness * 1.2121));
        AdjustRamp.Set(&dimming, (byte)(MINBRIGHT - tempbrightness * 1.2121), adjusttimeramp);
        activeflag = 2; //set flag
      }
    }

    else if (btchar == 'T'){
      byte i =0;
      while (i<12 && Serial2.available()){
        btchar = Serial2.read();
        if (btchar >= '0' && btchar <='9')
            btinput[i] = btchar - '0';  //converts to an int from a char
         else{
          Serial2.print("Invalid Time Packet.");
          break;
         }
         i++;
      }
      //If the packet was the appropriate byte size
      if (i==12)
        SetTimes(btinput);
    }

    else if (btchar == 'A'){
      byte i =0;
      while (i<11 && Serial2.available()){
        btchar = Serial2.read();
        if (btchar >= '0' && btchar <='9')
            btinput[i] = btchar - '0';  //converts to an int from a char
         else{
          Serial2.print("Invalid Alarm Packet.");
          break;
         }
         i++;
      }

      if (i==11)
        SetAlarms(btinput);
      for (byte i=0; i<7; i++){
        Serial2.print(alarmtime[i][0]);
        Serial2.println(alarmtime[i][1]);        
      }
    }

    else{
      Serial2.print("Read Error: ");
      Serial2.println(btchar);
    }
    Serial2.clear(); //Clear any remaning input like CR or LFs
  }
}

