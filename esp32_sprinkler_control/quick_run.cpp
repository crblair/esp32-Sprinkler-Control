#include "schedule.h"
#include <Arduino.h>

// Sequential Quick Run implementation for ScheduleManager
// Runs through all enabled zones, one at a time, for the specified duration

void ScheduleManager::startQuickRun(int durationSeconds) {
    stopQuickRun(); // Ensure any previous run is cleared
    quickRunDurationPerZone = durationSeconds * 1000;
    quickRunNumZones = 0;
    quickRunZoneIndex = 0;
    quickRunCurrentZone = -1;
    // Debug: Print enabled/disabled state of all zones
    for (int i = 0; i < count; ++i) {
    }
    // Build list of enabled zones
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
    // Start first zone
    for (int i = 0; i < count; ++i) sprinklers[i].setState(false);
    sprinklers[quickRunCurrentZone].setState(true);
}

void ScheduleManager::stopQuickRun() {
    if (quickRunActive && quickRunCurrentZone >= 0 && quickRunCurrentZone < count) {
        sprinklers[quickRunCurrentZone].setState(false);
    }
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
        sprinklers[quickRunCurrentZone].setState(false);
        quickRunZoneIndex++;
        if (quickRunZoneIndex < quickRunNumZones) {
            quickRunCurrentZone = quickRunPendingZones[quickRunZoneIndex];
            sprinklers[quickRunCurrentZone].setState(true);
            quickRunStartTime = millis(); // Use fresh millis for next zone
        } else {
            stopQuickRun();
        }
    }
}

bool ScheduleManager::isQuickRunActive() const {
    return quickRunActive;
}
