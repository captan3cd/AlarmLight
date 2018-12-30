/*Slowly turn on a light at a scheduled time.
*/
#include <Wire.h>
#include <DS3232RTC.h> //For working with the RTC module
#include <SoftwareSerial.h> //Used for bluetooth serial
#include <TimerOne.h> //Simplifies handling the hw timer
#include "LightRamp.h" //Provides LightRamp class for dimming

#define AC_LOAD 1
#define PUSHPIN 14 //should be interupt pin, in this case, interupt 0
#define TXPIN 10
#define RXPIN 9
#define ZX 2 //Zero crossing detector, should be an interput pin, in this case, interupt 1

#define MAXBRIGHT 4
#define MINBRIGHT 124

DS3232RTC RTC(true); //initialize the rtc object and wire.h instance
                     //If using a teensy or other board with multiple sda/scl pins, use the Wire.setSDA/SCL functions

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}; 
//Can be used in conjuction with the DateTime class' dayOfTheWeek() function
byte currentday = 0; //Used as a shorthand for the value of DateTime.dayOfTheWeek()

time_t CurrentTime;
time_t LastTime;
SoftwareSerial BTSerial(RXPIN, TXPIN);

byte alarmtime[7][2] = {{100,100},{6,20},{6,20},{6,20},{6,20},{6,20},{100,100}}; //hour and minute of alarm

bool alarmlightstatus = false;  //tracks whether the alarm has already gone off
volatile bool buttonstate = 0;

char btchar;
byte btinput[23];

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

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);

  setSyncProvider(RTC.get); //Syncs the arduino's internal clock with the ds3232
  if (timeStatus() != timeSet){
    Serial.println("RTC sync failure");
  }
    
  //Probably should be a fallback method here using __time__ and __day__

  
//  while (! RTC.begin()) {
//    Serial.println("Couldn't find RTC");
//    delay(5000);
//  }
//
//  if (RTC.lostPower()) {
//    Serial.println("RTC lost power, lets set the time!");
//    // following line sets the RTC to the date & time this sketch was compiled
//    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
//    // This line sets the RTC with an explicit date & time, for example to set
//    // January 21, 2014 at 3am you would call:
//    // RTC.adjust(DateTime(2014, 1, 21, 3, 0, 0));
//  }

  LastTime = now(); //Lasttime needs to be initiated, otherwise the first pass through the main loop thinks there is a day change
  currentday = weekday(LastTime);
  
  pinMode(AC_LOAD,OUTPUT);
  pinMode(PUSHPIN,INPUT);
  pinMode(ZX, INPUT);

  attachInterrupt(PUSHPIN,ButtonPress,RISING);
  attachInterrupt(ZX,ZeroCross,RISING);
  Timer1.initialize(freqstep);
  Timer1.attachInterrupt(DimCheck,freqstep);
}

void loop() {  
  CurrentTime = now();
  
  //If the day has changed, reset the alarm flag so it can go off again. Also increment the day counter
  if (abs(day(CurrentTime)-day(LastTime))){
    alarmlightstatus=false;
    currentday = weekday(CurrentTime); // Update the day variable
  }    
  //Serial.println(CurrentTime.hour()); Serial.println(CurrentTime.minute()); Serial.println(currentday);
  
  //If the current time is past the alarm time, and the light is less than half max brightness, and the alarm is set / valid
  if (hour(CurrentTime)>=alarmtime[currentday-1][0] && minute(CurrentTime)>=alarmtime[currentday-1][1] && dimming>=MINBRIGHT/2 && alarmtime[currentday-1][0]<24 &&!alarmlightstatus){
    Serial.println("triggered");
    alarmlightstatus=true;
    activeflag = 3;
    AlarmRamp.Set(&dimming, MAXBRIGHT, alarmtimeramp);
    buttonstate=0;
  }
  //Serial.print(CurrentTime.hour(),DEC); Serial.println(CurrentTime.minute(),DEC);

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

  //Reads in serial data. A proper input package should start with 'H', followed by the alarm time, current time and current date.
  if (BTSerial.available() ){
    delay(2300); //I'm not sure if it's the terminal app I'm using or the ble package being too many bytes (probably), 
                 //but a delay is needed to allow the hc05 to receive the entire package.
    btchar = BTSerial.read();
    if (btchar == 'H'){
      for( byte i =0; i<23 && BTSerial.available(); i++) {
         btchar = BTSerial.read();
         if (btchar >= '0' && btchar <='9')
            btinput[i] = btchar - '0';  //converts to an int from a char
         else{
          Serial.print("Invalid Input.");
          break;
         }
      }
      SetTimes(btinput);
      for (byte i=0; i<7; i++){
        BTSerial.print(alarmtime[i][0]);
        BTSerial.println(alarmtime[i][1]);        
      }
    }
    
    else if (btchar == 'h'){ // a 'h' header specifies a slider adjustment to the light level
      btchar = BTSerial.read();
      byte tempbrightness = (btchar - '0')*10 + (BTSerial.read() - '0'); //This is a brightness percentage from 0 to 99.
      if (tempbrightness <=100 && tempbrightness >=0){
        Serial.println((byte)(MINBRIGHT - tempbrightness * 1.2121));
        AdjustRamp.Set(&dimming, (byte)(MINBRIGHT - tempbrightness * 1.2121), adjusttimeramp);
        activeflag = 2; //set flag
      }
    }
    else{
      Serial.println(btchar);
    }
  }

  ButtonRamp.Update();
  AdjustRamp.Update();
  AlarmRamp.Update();
}

void SetTimes(byte TimeInput[23]){
  //Sets current time and alarm time
  byte chour = TimeInput[6]*10 + TimeInput[7];
  byte cminute = TimeInput[8]*10 + TimeInput[9];
  byte csecond = TimeInput[10]*10 + TimeInput[11];
  byte cday = TimeInput[4]*10 + TimeInput[5];
  byte cmonth = TimeInput[2]*10 + TimeInput[3];
  byte cyear = 2000 + TimeInput[0]*10 + TimeInput[1];
  setTime(chour,cminute,csecond,cday,cmonth,cyear); //Sets the system clock
  RTC.set(now()); //Sets the rtc based on the system clock
  //RTC.adjust( DateTime(cyear, cmonth, cday, chour, cminute, csecond) );

  for (byte i=0; i<7; i++){
    if (TimeInput[i+12]){
      alarmtime[i][0] = TimeInput [19]*10 + TimeInput[20];
      alarmtime[i][1] = TimeInput [21]*10 + TimeInput[22];
    }
  }

  for (byte i = 0; i<23; i++) //clears the TimeInput / btinput array
    TimeInput[i] = 0;
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

