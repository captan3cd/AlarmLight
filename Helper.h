/* Helper functions and classes used by the core alarmlight code.  
*/
#ifndef Helper_H
#define Helper_H

#include <Arduino.h>
#include <Time.h>
#include <stdio.h>

// Convert compile time to system time 
// call with the command
//time_t build_time = cvt_date(__DATE__,__TIME__);
time_t cvt_date(char const *date, char const *time);

class Info {
  private:
  Stream* Output;
  char daysOfTheWeek[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  char monthsOfTheYear[12][10] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
  
  public:
  Info(Stream* comm);
  void SystemTime();
  void SystemDate();
  void Level(byte bright);
  void Alarms(byte (&alarms)[7][2]);
  
};


#endif 
