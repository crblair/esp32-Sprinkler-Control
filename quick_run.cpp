#include "schedule.h"
#include <Arduino.h>

// Sequential Quick Run implementation for ScheduleManager
// Runs through all enabled zones, one at a time, for the specified duration

// Updated: Only run zones that are enabled and have nonzero duration for the selected program
void ScheduleManager::startQuickRun(int durationSeconds, int programIndex) {
    stopQuickRun(); // Ensure any previous run is cleared
    quickRunDurationPerZone = durationSeconds * 1000;
    quickRunNumZones = 0;
    quickRunZoneIndex = 0;
    quickRunCurrentZone = -1;
    // Build list of enabled zones with nonzero duration for the selected program
    for (int i = 0; i < count; ++i) {
        if (!sprinklers[i].disabled) {
            quickRunPendingZones[quickRunNumZones++] = i;
        }
    }
    if (quickRunNumZones == 0) {
        quickRunActive = false;
        return;
    }
    quickRunActive = true;
    quickRunZoneIndex = 0;
    quickRunCurrentZone = quickRunPendingZones[quickRunZoneIndex];
    quickRunStartTime = millis();
    // Relay ON/OFF will be handled centrally in the main loop
}

void ScheduleManager::stopQuickRun() {
    // Relay OFF will be handled centrally in the main loop
    quickRunActive = false;
    quickRunCurrentZone = -1;
    quickRunZoneIndex = 0;
    quickRunNumZones = 0;
}

void ScheduleManager::updateQuickRun() {
    if (!quickRunActive) {
        return;
    }
    unsigned long now = millis();
    if (quickRunCurrentZone < 0 || quickRunZoneIndex >= quickRunNumZones) {
        stopQuickRun();
        return;
    }
    if (now - quickRunStartTime >= (unsigned long)quickRunDurationPerZone) {
        // Time to move to next zone
        quickRunZoneIndex++;
        if (quickRunZoneIndex < quickRunNumZones) {
            quickRunCurrentZone = quickRunPendingZones[quickRunZoneIndex];
            quickRunStartTime = millis(); // Use fresh millis for next zone
            
        } else {
            stopQuickRun();
        }
    }
}


bool ScheduleManager::isQuickRunActive() const {
    return quickRunActive;
}
