#ifndef WEEKDAY_H
#define WEEKDAY_H

#include <Arduino.h>

enum Weekday { 
    MONDAY = 0, 
    TUESDAY, 
    WEDNESDAY, 
    THURSDAY, 
    FRIDAY, 
    SATURDAY, 
    SUNDAY,
    WEEKDAY_COUNT  // Add this for easier iteration and bounds checking
};

class WeekdayUtils {
public:
    // Convert Weekday enum to string
    static String toString(Weekday day);
    
    // Convert string to Weekday enum
    static Weekday fromString(const String& dayStr);
    
    // Validate time string format
    static bool validateTimeFormat(const String& timeStr);
    
    // Convert epoch time to Weekday
    static Weekday fromEpochTime(unsigned long epochTime);
    
    // Get today's weekday as an index (0-6)
    static int todayAsIndex() {
        return (int)fromEpochTime(time(nullptr));
    }
};

#endif // WEEKDAY_H