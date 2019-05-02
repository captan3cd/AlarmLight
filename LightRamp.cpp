/* LightRamp.h - Library for altering the brightness variable of a lamp over a specified time interval. */
#include <Arduino.h>
#include "LightRamp.h"

LightRamp::LightRamp(byte f, volatile byte* a ){
  flag = f;
  activeflag = a;
}

void LightRamp::Set(volatile byte* cbright, byte tbright, unsigned long ramp){ //the cbright param should be passed with &variable
  targetbright = tbright;
  beginbright = *cbright;
  currentbright = cbright;
  timeramp = ramp;
  
  delaytime = timeramp / abs (targetbright - beginbright);
  sign = (targetbright - beginbright)/ abs (targetbright - beginbright);
}

void LightRamp::Update (){
  if (flag != *activeflag)  //if this isn't the active lightramp, quit
    return;

  currentclick = millis();

  if (currentclick - prevclick > delaytime){
    *currentbright = (*currentbright + sign);
    Serial2.println(*currentbright);
    prevclick = currentclick;
    if (*currentbright == targetbright)  //if the current brightness is the target brightness, reset the activeflag so no more updates occur.
      *activeflag = 0; 
  }
}
