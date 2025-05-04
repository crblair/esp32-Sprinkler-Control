#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Arduino.h>

class SprinklerController;

struct Program {
    String startTime;           // Initial start time for the program (HH:mm)
    uint16_t durations[8];     // Duration in minutes for each zone
    bool enabled;              // Whether this program is active
    bool daysOfWeek[7];       // Which days this program runs (0=Monday to 6=Sunday)
};

struct CalculatedSchedule {
    String startTime;          // Calculated start time for this zone
    String endTime;           // Calculated end time for this zone
    uint8_t programIndex;     // Which program this schedule belongs to (0-2)
};

class Sprinkler {
public:
    Sprinkler();
    void init(int zoneIndex, SprinklerController* controller); // zoneIndex instead of pin
    void setState(bool on);
    CalculatedSchedule calculatedSchedules[3];  // Up to 3 run times per day from different programs
    bool state;
    bool disabled;
    int zone; // zone index (0-based)
private:
    SprinklerController* _controller;
};

class ScheduleManager {
public:
    ScheduleManager(int num, SprinklerController* controller); // Pass controller
    ~ScheduleManager();
    void initializePrograms();
    void calculateZoneSchedules(int dayOfWeek);  // Calculate start/end times for all zones
    void savePrograms();
    void loadPrograms();
    void printPrograms();
    Sprinkler& getSprinkler(int index);
    bool checkSprinklerSchedule(int sprinklerIndex, int today, int currentHour, int currentMinute);
    bool verifyPrograms();
    bool verifySchedules();  // Verify program configurations
    void clearAllPreferences();  // Clear all program data from Preferences
    
    Program programs[3];        // 3 available programs (A, B, C)
    Sprinkler* sprinklers;      // Array of sprinkler objects
    int count;                  // Number of sprinklers

    // --- Quick Run State and Methods ---
    bool quickRunActive = false;               // Is Quick Run running?
    int quickRunCurrentZone = -1;              // Index of zone currently running
    unsigned long quickRunStartTime = 0;       // When did current zone start (millis)
    int quickRunDurationPerZone = 10000;       // Duration per zone in ms (default 10 sec)
    int quickRunPendingZones[8];               // Indices of zones to run (max 8 zones)
    int quickRunNumZones = 0;                  // How many zones in this session
    int quickRunZoneIndex = 0;                 // Which zone in pending list is running

    void startQuickRun(int durationSeconds);
    void stopQuickRun();
    void updateQuickRun();
    bool isQuickRunActive() const;
private:
    SprinklerController* _controller;
};

#endif // SCHEDULE_H