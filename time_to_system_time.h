/*  Function to convert compile time reported as 
  MBED compiler macro into a System time we can 
  use to set time and date in our chip.  This can
  be a helpful way to initialize clock chips for
  low volume tests 
  By Joe Ellsworth CTO of A2WH Free for all, no promised, no warranty

 call with the command
   printf("compile time=%s date=%s\r\n",__TIME__,__DATE__);
   time_t build_time = cvt_date(__DATE__,__TIME__);
   printf("compile time reformate=%s r\n", ctime(&build_time));
   
    
*/
  
#ifndef compile_time_to_system_time_H
#define compile_time_to_system_time_H

#include <Arduino.h>
#include <Time.h>
#include <stdio.h>

 
// call with the command
//time_t build_time = cvt_date(__DATE__,__TIME__);

// Convert compile time to system time 
time_t cvt_date(char const *date, char const *time)
{
    char s_month[5];
    int year;
    tmElements_t t;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    sscanf(date, "%s %d %d", s_month, &t.Day, &year);
    sscanf(time, "%2d %*c %2d %*c %2d", &t.Hour, &t.Minute, &t.Second);
    // Find where is s_month in month_names. Deduce month value.
    t.Month = (strstr(month_names, s_month) - month_names) / 3 + 1;    
    t.Year = year - 1900;    
    return makeTime(t);
}


#endif 
