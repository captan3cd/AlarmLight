/*Slowly turn on a light at a scheduled time.
 * 
 * 
 */
#include <Wire.h>
#include "RTClib.h"
#include <SoftwareSerial.h> 

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

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

DateTime currenttime;
DateTime lasttime;
SoftwareSerial BTSerial(rxPin, txPin); 

unsigned short alarmtime [2] = {12,1}; //hour and minute of alarm
short brightness = 0; //current brightness of the light (analog range from 0 t0 255). We do some math with this variable where the value can be less then 0, so it can't be unsigned (probs).
bool alarmlightstatus = false;  //tracks whether the alarm has already gone off
volatile bool buttonstate = 0;

char BTchar;
short BTinput[18];

unsigned short hardtimeramp = 3000; //Time for the light to dim on a hard on/off press
unsigned long alarmramp = 30000; //Time for alarm light to reach max brightness
unsigned short adjustramp = 500; //Time for light to change for a brightness adjustment

unsigned short dimming = 60;


void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  
  delay(3000); // wait for console opening

  while (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    delay(5000);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  pinMode(AC_LOAD,OUTPUT);
  pinMode(PUSHPIN,INPUT);
  pinMode(ZX, INPUT);

  attachInterrupt(0,ButtonPress,RISING);
  attachInterrupt(1,ZeroCross,RISING);

}

void loop() {
  if (rtc.lostPower()){
    //Error handling if the rtc loses power
    Serial.println("Please reset the time!");
  }
  
  currenttime = rtc.now();

  //If the day has changed, reset the alarm flag so it can go off again.
  if (abs(currenttime.day()-lasttime.day()))
    alarmlightstatus=false;

  //If the current time is past the alarm time, and the light is less than half max brightness
  if (currenttime.hour()>=alarmtime[0] && currenttime.minute()>=alarmtime[1] && brightness<=125 && !alarmlightstatus){
    alarmlightstatus=true;
    LightRamp(brightness, 255, alarmramp);
    buttonstate=0;
  }
  //Serial.print(currenttime.hour(),DEC); Serial.println(currenttime.minute(),DEC);
  //Serial.println(brightness);
  Serial.println(buttonstate);
  if (buttonstate!=0){
    buttonstate = 0;
    if (brightness>0)
      LightRamp(brightness,0,hardtimeramp);
    else
      LightRamp(brightness,255,hardtimeramp);
  }

  lasttime = currenttime;

  if (BTSerial.available()){  //Reads in serial data. A proper input package should start with 'H', followed by the alarm time, current time and current date.
    BTchar = BTSerial.read();
    if (BTchar == 'H'){
      for( short i =0; i<18 && BTSerial.available(); i++) {
         BTchar = BTSerial.read();
         if (BTchar >= '0' && BTchar <='9')
            BTinput[i] = BTchar - '0';
         else{
          BTSerial.flush();
          Serial.print("Invalid Input. Flushing...");
          break;
         }
      }
      SetTimes(BTinput);
    }
    
    else if (BTchar == 'h'){ // a 'h' header specifies a slider adjustment to the light level
      BTchar = BTSerial.read();
      short tempbrightness = (BTchar - '0')*10 + (BTSerial.read() - '0'); //This is a brightness percentage from 0 to 99.
      if (tempbrightness <=100 && tempbrightness >=0){
        LightRamp(brightness,tempbrightness * 2.55,adjustramp);  //2.55 is the conversation factor to get to convert from percentage to pin output.
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
    analogWrite(AC_LOAD,cbright);
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
  rtc.adjust( DateTime(cyear, cmonth, cday, chour, cminute, csecond) );
  
  alarmtime[0] = TimeInput[0]*10 + TimeInput[1];
  alarmtime[1] = TimeInput[2]*10 + TimeInput[3];
  //TimeInput[4:5] Is the alarm time seconds, which is not currently used.

  for (short i = 0; i<0; i++) //clears the TimeInput / BTinput array
    TimeInput[i] = 0;
}

void ButtonPress(){
  buttonstate=1;
}

void ZeroCross(){
  // Firing angle calculation : 1 full 50Hz wave =1/50=20ms 
  // Every zerocrossing thus: (50Hz)-> 10ms (1/2 Cycle) 
  // For 60Hz => 8.33ms (10.000/120)
  // 10ms=10000us
  // (10000us - 10us) / 128 = 75 (Approx) For 60Hz =>65
  
  // The logic for the above brightness is 0-255 for Digital output. However, the timing logic was made for a 124 to 5 scale. Conversion is 124-brightness/2.14285714286. Needs to be unified later.
  delayMicroseconds(65*(124-brightness/2.14285714286));    // Wait till firing the TRIAC, // For 60Hz =>65
  digitalWrite(AC_LOAD, HIGH);   // Fire the TRIAC
  delayMicroseconds(8.33);         // triac On propogation delay 
         // (for 60Hz use 8.33) Some Triacs need a longer period
  digitalWrite(AC_LOAD, LOW);    // No longer trigger the TRIAC (the next zero crossing will swith it off) TRIAC
}


