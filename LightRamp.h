/* LightRamp.h - Library for altering the brightness variable of a lamp over a specified time interval. */
#ifndef LightRamp_h
#define LightRamp_h

#include <Arduino.h>

class LightRamp{

  private:
    unsigned long timeramp;
    unsigned short delaytime;
    byte targetbright;
    byte beginbright;  //may not need this
    volatile byte* currentbright;
    short sign;
  
    byte flag; // an number that designates the specific class instances. Compared against the activeflag in the main loop to determine which lightramp instance to use.
    volatile byte* activeflag;
    
    unsigned long currentclick;
    unsigned long prevclick;
  
  public:
    LightRamp(byte f, volatile byte* a );
    void Set(volatile byte* cbright, byte tbright, unsigned long ramp); //the cbright param should be passed with &variable
    void Update();
};

#endif
