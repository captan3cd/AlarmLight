/* LightRamp.h - Library for altering the brightness variable of a lamp over a specified time interval. */
#ifndef LightRamp_h
#define LightRamp_h

#include <Arduino.h>

class LightRamp{

  private:
    unsigned short timeramp;
    unsigned short delaytime;
    short targetbright;
    short beginbright;  //may not need this
    short* currentbright;
    short sign;
  
    short flag; // an number that designates the specific class instances. Compared against the activeflag in the main loop to determine which lightramp instance to use.
    short* activeflag;
    
    unsigned long currentclick;
    unsigned long prevclick;
  
  public:
    LightRamp(short f, short* a );
    void Set(short* cbright, short tbright, unsigned short ramp); //the cbright param should be passed with &variable
    void Update();

};


#endif
