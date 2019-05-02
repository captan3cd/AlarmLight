//Helper functions and classes

#include <Arduino.h>
#include "Helper.h"

time_t cvt_date(char const *date, char const *time){
    char s_month[5];
    short tyear, tday;
    unsigned short hour, minute, second;
    tmElements_t t;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    
    sscanf(date, "%s %2hd %4hd", s_month, &tday, &tyear);
    sscanf(time, "%2hd %*c %2hd %*c %2hd", &hour, &minute, &second);
    // Find where is s_month in month_names. Deduce month value.
    t.Month = (strstr(month_names, s_month) - month_names) / 3 + 1;   
    t.Day = tday; 
    t.Year = tyear - 1970;
    t.Hour = hour;
    t.Minute = minute;
    t.Second = second;  
    return makeTime(t);
}

Info::Info(Stream* comm){
  Output = comm;
}

void Info::SystemTime(){
  Output->print("System time is - ");
  Output->print(hour()); Output->print(":");
  Output->print(minute()); Output->print(":");
  Output->print(second()); Output->println(".");
}

void Info::SystemDate(){
  Output->print(monthsOfTheYear[month()-1]); Output->print(" ");
  Output->print(day()); Output->print(", ");
  Output->print(year()); Output->println(".");
}

void Info::Level(byte bright){
  Output->print("Current brightness: ");
  Output->print(bright); Output->println(".");
}

void Info::Alarms(byte (&alarms)[7][2]){
  Output->println("The alarm schedule is the following...");
  for (int i=0; i<7; i++){
    Output->print(daysOfTheWeek[i]); Output->print(": ");
    if (alarms[i][0]>24)
      Output->println("Not Set");
    else{
      Output->print(alarms[i][0]); Output->print(":");
      Output->println(alarms[i][1]);
    }
  }
  Output->println();
}

