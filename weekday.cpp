#include "weekday.h"
#include <time.h>

String WeekdayUtils::toString(Weekday day) {
    switch (day) {
        case MONDAY:    return "Monday";
        case TUESDAY:   return "Tuesday";
        case WEDNESDAY: return "Wednesday";
        case THURSDAY:  return "Thursday";
        case FRIDAY:    return "Friday";
        case SATURDAY:  return "Saturday";
        case SUNDAY:    return "Sunday";
        default:        return "Unknown";
    }
}

Weekday WeekdayUtils::fromString(const String& dayStr) {
    String upperDay = dayStr;
    upperDay.toUpperCase();
    
    if (upperDay == "MONDAY")    return MONDAY;
    if (upperDay == "TUESDAY")   return TUESDAY;
    if (upperDay == "WEDNESDAY") return WEDNESDAY;
    if (upperDay == "THURSDAY")  return THURSDAY;
    if (upperDay == "FRIDAY")    return FRIDAY;
    if (upperDay == "SATURDAY")  return SATURDAY;
    if (upperDay == "SUNDAY")    return SUNDAY;
    
    return MONDAY;  // Default return
}

bool WeekdayUtils::validateTimeFormat(const String& timeStr) {
    // Check time format HH:MM
    if (timeStr.length() != 5) return false;
    
    if (timeStr.charAt(2) != ':') return false;
    
    int hours = timeStr.substring(0, 2).toInt();
    int minutes = timeStr.substring(3).toInt();
    
    return (hours >= 0 && hours < 24) && (minutes >= 0 && minutes < 60);
}

Weekday WeekdayUtils::fromEpochTime(unsigned long epochTime) {
    struct tm* timeinfo = gmtime((time_t*)&epochTime);
    int day = timeinfo->tm_wday;
    return (day == 0) ? SUNDAY : static_cast<Weekday>(day - 1);
}