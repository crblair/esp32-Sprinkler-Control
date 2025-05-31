#include <Preferences.h>
#include <ArduinoJson.h>
/**
 * @file schedule.cpp
 * @brief Implements sprinkler scheduling, zone management, and Quick Run logic for the ESP32 sprinkler controller.
 *
 * Provides classes and functions for managing scheduled, manual, and Quick Run operations.
 */

#include "schedule.h"
#include "config.h"
#include "sprinkler_controller.h"

// Copy configuration from one program to another and persist
void ScheduleManager::copyProgram(int targetIndex, int sourceIndex) {
    if (targetIndex == sourceIndex) return;
    programs[targetIndex] = programs[sourceIndex];
    savePrograms(); // Persist the change
}

// Implements "Run Program Now" for a program
void ScheduleManager::startRunProgramNow(int programIndex) {
    // Set the program's startTime to the current local time (HH:mm)
    time_t now;
    struct tm* timeinfo;
    time(&now);
    timeinfo = localtime(&now);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    programs[programIndex].startTime = String(buf);

    // Force recalculation of end times for today using current local time
    time_t nowCalc;
    struct tm* timeinfoCalc;
    time(&nowCalc);
    timeinfoCalc = localtime(&nowCalc);
    int today = (timeinfoCalc->tm_wday + 6) % 7; // 0=Monday, 1=Tuesday, ...
    calculateZoneSchedules(today);

    // Reset all Run Program Now state
    currentRunNowProgramIndex = programIndex;
    runProgramNowNumZones = 0;
    runProgramNowZoneIndex = 0;
    runProgramNowCurrentZone = -1;
    runProgramNowActive = false;

    // Build list of enabled zones with nonzero duration for the selected program
    for (int i = 0; i < count; ++i) {
        if (!sprinklers[i].disabled && programs[programIndex].durations[i] > 0) {
            runProgramNowPendingZones[runProgramNowNumZones++] = i;
        }
    }
    if (runProgramNowNumZones == 0) {
        runProgramNowActive = false;
        return;
    }

    // Set up to run the first valid zone
    runProgramNowActive = true;
    runProgramNowZoneIndex = 0;
    if (runProgramNowZoneIndex >= 0 && runProgramNowZoneIndex < runProgramNowNumZones) {
        runProgramNowCurrentZone = runProgramNowPendingZones[runProgramNowZoneIndex];
    } else {
        runProgramNowActive = false;
        return;
    }
    runProgramNowStartTime = millis();
    runProgramNowDurationPerZone = programs[programIndex].durations[runProgramNowCurrentZone] * 60 * 1000; // ms

    // Mark state for main loop to handle relay control
    savePrograms();
}

// Stop Run Program Now for all programs
void ScheduleManager::stopRunProgramNow() {
    runProgramNowActive = false;
    currentRunNowProgramIndex = -1;
}

// Update Run Program Now sequencing (completely independent from Quick Run)
void ScheduleManager::updateRunProgramNow() {
    if (!runProgramNowActive) return;
    unsigned long now = millis();
    if (runProgramNowCurrentZone < 0 || runProgramNowZoneIndex >= runProgramNowNumZones) {
        runProgramNowActive = false;
        currentRunNowProgramIndex = -1;
        return;
    }
    if (now - runProgramNowStartTime >= runProgramNowDurationPerZone) {
        runProgramNowZoneIndex++;
        if (runProgramNowZoneIndex < runProgramNowNumZones) {
            runProgramNowCurrentZone = runProgramNowPendingZones[runProgramNowZoneIndex];
            runProgramNowStartTime = millis();
            // Set correct duration for this zone from the active program
            if (currentRunNowProgramIndex >= 0 && currentRunNowProgramIndex < 3) {
                runProgramNowDurationPerZone = programs[currentRunNowProgramIndex].durations[runProgramNowCurrentZone] * 60 * 1000;
            }
        } else {
            runProgramNowActive = false;
            currentRunNowProgramIndex = -1;
        }
    }
}

// Utility: Clear all schedule data from Preferences
void ScheduleManager::clearAllPreferences() {
    Preferences prefs;
    prefs.begin("sprinkler_sched", false);
    for (int i = 0; i < count; i++) {
        for (int d = 0; d < 7; d++) {
            for (int p = 0; p < 3; p++) {
                String keyStart = String("sch_") + i + "_" + d + "_" + p + "_start";
                String keyEnd = String("sch_") + i + "_" + d + "_" + p + "_end";
                prefs.remove(keyStart.c_str());
                prefs.remove(keyEnd.c_str());
            }
        }
    }
    prefs.end();

}

// Sprinkler Constructor
Sprinkler::Sprinkler() : state(false), disabled(false), zone(-1), _controller(nullptr) {
    for (int p = 0; p < 3; p++) {
        calculatedSchedules[p].startTime = "";
        calculatedSchedules[p].endTime = "";
        calculatedSchedules[p].programIndex = 0;
    }
}

void Sprinkler::init(int zoneIndex, SprinklerController* controller) {
    zone = zoneIndex;
    _controller = controller;
    state = false;
    // Initialization of pin handled by SprinklerController::begin()
}

void Sprinkler::setState(bool on) {
    if (_controller && zone != -1) {
        // Always turn OFF if disabled
        if (disabled) {
            _controller->setRelay(zone, false);
            state = false;
            return;
        }
        _controller->setRelay(zone, on);
        state = on;
    }
}

// ScheduleManager Implementation
/**
 * @class ScheduleManager
 * @brief Manages sprinkler schedules, manual control, and Quick Run state.
 *
 * Handles zone state, schedule calculation, persistence, and Quick Run sequencing.
 */
ScheduleManager::ScheduleManager(int num, SprinklerController* controller) : count(num), _controller(controller) {
    sprinklers = new Sprinkler[count];
    // Initialize each sprinkler with its zone index and controller
    for (int i = 0; i < count; i++) {
        sprinklers[i].init(i, controller);
    }
}

/**
 * @brief Destructor for ScheduleManager.
 *
 * Frees allocated memory for sprinklers.
 */
ScheduleManager::~ScheduleManager() {
    delete[] sprinklers;
}

/**
 * @brief Initializes default schedules for all sprinklers.
 *
 * Clears all programs and calculated schedules.
 */
void ScheduleManager::initializePrograms() {
    // Clear all programs so the user can input their own from the web interface
    for (int p = 0; p < 3; p++) {
        programs[p].startTime = "";
        programs[p].enabled = false;
        for (int z = 0; z < 8; z++) {
            programs[p].durations[z] = 0;
        }
        for (int d = 0; d < 7; d++) {
            programs[p].daysOfWeek[d] = false;
        }
    }

    // Clear all calculated schedules in sprinklers
    for (int s = 0; s < count; s++) {
        for (int p = 0; p < 3; p++) {
            sprinklers[s].calculatedSchedules[p].startTime = "";
            sprinklers[s].calculatedSchedules[p].endTime = "";
            sprinklers[s].calculatedSchedules[p].programIndex = 0;
        }
    }

    
    for (int i = 0; i < count; i++) {

        
        for (int d = 0; d < 7; d++) {
            for (int p = 0; p < 3; p++) {
                sprinklers[i].calculatedSchedules[p].startTime = "";
                sprinklers[i].calculatedSchedules[p].endTime = "";
                sprinklers[i].calculatedSchedules[p].programIndex = 0;
            }
        }
    }
    

}

/**
 * @brief Calculates schedules for all sprinklers on a given day.
 *
 * @param dayOfWeek Day of the week (0-6) to calculate schedules for.
 */
void ScheduleManager::calculateZoneSchedules(int dayOfWeek) {

    // Clear all existing calculated schedules first
    for (int s = 0; s < count; s++) {
        for (int p = 0; p < 3; p++) {
            sprinklers[s].calculatedSchedules[p].startTime = "";
            sprinklers[s].calculatedSchedules[p].endTime = "";
            sprinklers[s].calculatedSchedules[p].programIndex = 0;
        }
    }
    // --- Program A (index 0) ---
    int p = 0;


    

    if (programs[p].enabled && programs[p].daysOfWeek[dayOfWeek]) {
        String currentTime = programs[p].startTime;
        if (currentTime.length() == 5) {
            for (int z = 0; z < count; z++) {
                if (programs[p].durations[z] == 0) continue;
            
                sprinklers[z].calculatedSchedules[0].startTime = currentTime;
                sprinklers[z].calculatedSchedules[0].programIndex = p;
                int hours = currentTime.substring(0, 2).toInt();
                int minutes = currentTime.substring(3, 5).toInt();
                minutes += programs[p].durations[z];
                while (minutes >= 60) {
                    hours++;
                    minutes -= 60;
                }
                if (hours >= 24) hours -= 24;
                char endTime[6];
                sprintf(endTime, "%02d:%02d", hours, minutes);
                sprinklers[z].calculatedSchedules[0].endTime = String(endTime);
            
currentTime = String(endTime);
            }
        }
    }


    // --- Program B (index 1) ---
    p = 1;


    

    if (programs[p].enabled && programs[p].daysOfWeek[dayOfWeek]) {
        String currentTime = programs[p].startTime;
        if (currentTime.length() == 5) {
            for (int z = 0; z < count; z++) {
                if (programs[p].durations[z] == 0) continue;
            
                sprinklers[z].calculatedSchedules[1].startTime = currentTime;
                sprinklers[z].calculatedSchedules[1].programIndex = p;
                int hours = currentTime.substring(0, 2).toInt();
                int minutes = currentTime.substring(3, 5).toInt();
                minutes += programs[p].durations[z];
                while (minutes >= 60) {
                    hours++;
                    minutes -= 60;
                }
                if (hours >= 24) hours -= 24;
                char endTime[6];
                sprintf(endTime, "%02d:%02d", hours, minutes);
                sprinklers[z].calculatedSchedules[1].endTime = String(endTime);
            
                currentTime = String(endTime);
            }
        }
    }
    // --- Program C (index 2) ---
    p = 2;


    

    if (programs[p].enabled && programs[p].daysOfWeek[dayOfWeek]) {
        String currentTime = programs[p].startTime;
        if (currentTime.length() == 5) {
            for (int z = 0; z < count; z++) {
                if (programs[p].durations[z] == 0) continue;
            
                sprinklers[z].calculatedSchedules[2].startTime = currentTime;
                sprinklers[z].calculatedSchedules[2].programIndex = p;
                int hours = currentTime.substring(0, 2).toInt();
                int minutes = currentTime.substring(3, 5).toInt();
                minutes += programs[p].durations[z];
                while (minutes >= 60) {
                    hours++;
                    minutes -= 60;
                }
                if (hours >= 24) hours -= 24;
                char endTime[6];
                sprintf(endTime, "%02d:%02d", hours, minutes);
                sprinklers[z].calculatedSchedules[2].endTime = String(endTime);
                currentTime = String(endTime);
            }
        }
    }
}


// Batch save: Save all schedules for all sprinklers and days in a single Preferences session
/**
 * @brief Saves all programs to Preferences.
 *
 * Serializes program data to JSON and stores it in Preferences.
 */
void ScheduleManager::savePrograms() {

    Preferences prefs;
    prefs.begin("sprinkler_prog", false);
    StaticJsonDocument<4096> doc;

    doc["version"] = 1;
    JsonArray programsArray = doc.createNestedArray("programs");
    
    for (int p = 0; p < 3; p++) {
        JsonObject program = programsArray.createNestedObject();
        program["start"] = programs[p].startTime;
        program["enabled"] = programs[p].enabled;
        
        JsonArray durations = program.createNestedArray("durations");
        for (int z = 0; z < 8; z++) {
            durations.add(programs[p].durations[z]);
        }
        
        JsonArray days = program.createNestedArray("days");
        for (int d = 0; d < 7; d++) {
            days.add(programs[p].daysOfWeek[d]);
        }
    }

    String json;
    serializeJson(doc, json);
    prefs.putString("programs", json);
    prefs.end();

}

void ScheduleManager::loadPrograms() {

    Preferences prefs;
    prefs.begin("sprinkler_prog", true);
    
    String json = prefs.getString("programs", "");
    prefs.end();
    
    if (json.length() == 0) {
    
        initializePrograms();
        return;
    }
    
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
    
        initializePrograms();
        return;
    }
    
    // Load program configurations
    for (int p = 0; p < 3; p++) {
        const char* startTime = doc["programs"][p]["start"];
        programs[p].startTime = startTime ? String(startTime) : "";
        programs[p].enabled = doc["programs"][p]["enabled"] | false;
        
        // Load durations for each zone
        for (int z = 0; z < 8; z++) {
            programs[p].durations[z] = doc["programs"][p]["durations"][z];
        }
        
        // Load days of week
        for (int d = 0; d < 7; d++) {
            programs[p].daysOfWeek[d] = doc["programs"][p]["days"][d];
        }
    }
    

}

Sprinkler& ScheduleManager::getSprinkler(int index) {
    if (index < 0 || index >= count) {
        static Sprinkler defaultSprinkler;
        return defaultSprinkler;
    }
    return sprinklers[index];
}

bool ScheduleManager::checkSprinklerSchedule(
    int sprinklerIndex, 
    int today, 
    int currentHour, 
    int currentMinute
) {
    if (sprinklerIndex < 0 || sprinklerIndex >= count) 
        return false;

    // Check if sprinkler is disabled
    if (sprinklers[sprinklerIndex].disabled)
        return false;

    int currentTotal = currentHour * 60 + currentMinute;

    // Check each calculated schedule for this sprinkler
    for (int p = 0; p < 3; p++) {
        CalculatedSchedule& schedule = sprinklers[sprinklerIndex].calculatedSchedules[p];
        
        // Skip if no valid schedule
        if (schedule.startTime.length() != 5 || schedule.endTime.length() != 5)
            continue;
            
        int startHour = schedule.startTime.substring(0, 2).toInt();
        int startMin  = schedule.startTime.substring(3, 5).toInt();
        int endHour   = schedule.endTime.substring(0, 2).toInt();
        int endMin    = schedule.endTime.substring(3, 5).toInt();

        int startTotal = startHour * 60 + startMin;
        int endTotal = endHour * 60 + endMin;

        // Check if current time falls within the schedule
        if (startTotal < endTotal) {
            if (currentTotal >= startTotal && currentTotal < endTotal) {
                return true;
            }
        } else if (startTotal > endTotal) { // Handles midnight crossing
            if (currentTotal >= startTotal || currentTotal < endTotal) {
                return true;
            }
        }
    }

    return false;
}

bool ScheduleManager::verifyPrograms() {
    // Verify all programs have valid configurations
    for (int p = 0; p < 3; p++) {
        if (!programs[p].enabled) continue;
        
        // Check start time format
        if (programs[p].startTime.length() != 5) return false;
        if (programs[p].startTime[2] != ':') return false;
        
        // Check if at least one day is selected
        bool hasDay = false;
        for (int d = 0; d < 7; d++) {
            if (programs[p].daysOfWeek[d]) {
                hasDay = true;
                break;
            }
        }
        if (!hasDay) return false;
        
        // Check if at least one zone has a duration
        bool hasDuration = false;
        for (int z = 0; z < count; z++) {
            if (programs[p].durations[z] > 0) {
                hasDuration = true;
                break;
            }
        }
        if (!hasDuration) return false;
    }
    return true;
}

bool ScheduleManager::verifySchedules() {
    return verifyPrograms();
}

