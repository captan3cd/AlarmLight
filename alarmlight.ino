/*Slowly turn on a light at a scheduled time.
 * 
 * 
 */
#include <Wire.h>
#include "RTClib.h" //For working with the RTC module
#include <SoftwareSerial.h> //Used for bluetooth serial
#include <TimerOne.h> //Simplifies handling the hw timer

/*RTC Pins are 
 * Vin to 3-5V
 * GND to GND
 * SCL to Arduino SCL
 * SDA to Arduino SDA 
 */
#define AC_LOAD A2
#define PUSHPIN 2 //should be interupt pin, in this case, interupt 0
#define txPin 5
#define rxPin 4
#define ZX 3 //Zero crossing detector, should be an interput pin, in this case, interupt 1

RTC_DS3231 RTC;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

DateTime CurrentTime;
DateTime LastTime;
SoftwareSerial BTSerial(rxPin, txPin); 

unsigned short alarmtime [2] = {12,1}; //hour and minute of alarm
bool alarmlightstatus = false;  //tracks whether the alarm has already gone off
volatile bool buttonstate = 0;

char btchar;
short btinput[18];

unsigned short hardtimeramp = 3000; //Time for the light to dim on a hard on/off press
unsigned long alarmramp = 30000; //Time for alarm light to reach max brightness
unsigned short adjustramp = 500; //Time for light to change for a brightness adjustment

short dimming = 64; //range is theoretically from 0-128, but limited to 4-124 due to timing limits
volatile short wavecounter =0; //counter used in timing interrupt
volatile boolean zerocross = 0; //Flag for zero crossing
short freqstep = 65;  //For 50Hz, use 75
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

  while (! RTC.begin()) {
    Serial.println("Couldn't find RTC");
    delay(5000);
  }

  if (RTC.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // RTC.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  pinMode(AC_LOAD,OUTPUT);
  pinMode(PUSHPIN,INPUT);
  pinMode(ZX, INPUT);

  attachInterrupt(0,ButtonPress,RISING);
  attachInterrupt(1,ZeroCross,RISING);
  Timer1.initialize(freqstep);
  Timer1.attachInterrupt(DimCheck,freqstep);

}

void loop() {
  if (RTC.lostPower()){
    //Error handling if the RTC loses power
    Serial.println("Please reset the time!");
  }
  
  CurrentTime = RTC.now();

  //If the day has changed, reset the alarm flag so it can go off again.
  if (abs(CurrentTime.day()-LastTime.day()))
    alarmlightstatus=false;

  //If the current time is past the alarm time, and the light is less than half max brightness
  if (CurrentTime.hour()>=alarmtime[0] && CurrentTime.minute()>=alarmtime[1] && dimming>=64 && !alarmlightstatus){
    alarmlightstatus=true;
    LightRamp(dimming, 4, alarmramp); //4 for max brightness
    buttonstate=0;
  }
  //Serial.print(CurrentTime.hour(),DEC); Serial.println(CurrentTime.minute(),DEC);
  //Serial.println(brightness);
  Serial.println(buttonstate);
  if (buttonstate!=0){
    buttonstate = 0;
    if (dimming<124)
      LightRamp(dimming,124,hardtimeramp);
    else
      LightRamp(dimming,4,hardtimeramp);
  }

  LastTime = CurrentTime;

  if (BTSerial.available()){  //Reads in serial data. A proper input package should start with 'H', followed by the alarm time, current time and current date.
    btchar = BTSerial.read();
    if (btchar == 'H'){
      for( short i =0; i<18 && BTSerial.available(); i++) {
         btchar = BTSerial.read();
         if (btchar >= '0' && btchar <='9')
            btinput[i] = btchar - '0';
         else{
          BTSerial.flush();
          Serial.print("Invalid Input. Flushing...");
          break;
         }
      }
      SetTimes(btinput);
    }
    
    else if (btchar == 'h'){ // a 'h' header specifies a slider adjustment to the light level
      btchar = BTSerial.read();
      short tempbrightness = (btchar - '0')*10 + (BTSerial.read() - '0'); //This is a brightness percentage from 0 to 99.
      if (tempbrightness <=100 && tempbrightness >=0){
        LightRamp(dimming,124 - tempbrightness * 1.2121,adjustramp);  //y=124-1.2121x converts from percent to dimming
      }
    }
    else
      BTSerial.flush();
  }
  
}

void LightRamp (short &cbright, short tbright, unsigned short timeramp){
  unsigned short delaytime = timeramp/abs(tbright-cbright);
  short sign = (tbright-cbright)/abs(tbright-cbright);
  while((tbright-cbright)!=0 && !buttonstate){
    cbright=cbright+sign;
    //analogWrite(AC_LOAD,cbright);
    Serial.println(cbright);
    delay(delaytime);
  }
}

void SetTimes(short TimeInput[18]){
  //Sets current time and alarm time
  short chour = TimeInput[6]*10 + TimeInput[7];
  short cminute = TimeInput[8]*10 + TimeInput[9];
  short csecond = TimeInput[10]*10 + TimeInput[11];
  short cday = TimeInput[12]*10 + TimeInput[13];
  short cmonth = TimeInput[14]*10 + TimeInput[15];
  short cyear = 2000 + TimeInput[16]*10 + TimeInput[17];
  RTC.adjust( DateTime(cyear, cmonth, cday, chour, cminute, csecond) );
  
  alarmtime[0] = TimeInput[0]*10 + TimeInput[1];
  alarmtime[1] = TimeInput[2]*10 + TimeInput[3];
  //TimeInput[4:5] Is the alarm time seconds, which is not currently used.

  for (short i = 0; i<0; i++) //clears the TimeInput / btinput array
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

