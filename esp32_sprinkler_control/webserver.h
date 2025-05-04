#ifndef SPRINKLER_WEBSERVER_H
#include "sprinkler_system_state.h"
#include <ArduinoJson.h>
#define SPRINKLER_WEBSERVER_H

extern String currentTimeZoneName;
extern bool currentDstEnabled;

// currentTimeZoneOffset removed; handled by configTime() and system time.
// currentTimeZoneName removed; handled by configTime() and system time.

// setTimeZoneSimple removed; all time handled by configTime().

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h> // Only standard time functions needed
#include "program_page.h"
#include "schedule.h"

void setupOTA();

void setupWebServerRoutes(
    WebServer& server,
    ScheduleManager& scheduleManager,
    SprinklerSystemState& manualState
);

void handleRoot(
    WebServer& server,
    ScheduleManager& scheduleManager,
    SprinklerSystemState& manualState
);

// handleRoot updated to accept SprinklerSystemState& manualState as a parameter.

void handleStatus(
    WebServer& server,
    ScheduleManager& scheduleManager,
    SprinklerSystemState& manualState
);

void handleManualMode(
    WebServer& server,
    ScheduleManager& scheduleManager,
    SprinklerSystemState& manualState
);

void handleManualControl(
    WebServer& server,
    ScheduleManager& scheduleManager,
    SprinklerSystemState& manualState,
    int index
);

void handleNetworkConfig(WebServer& server);
void handleSetNetwork(WebServer& server);

void handleUpdatePage(WebServer& server);

String formatTime12Hour(const String& time24hr);
void handleCurrentTime(WebServer& server);

// Quick Run endpoints (for clarity)
// These are implemented inline in setupWebServerRoutes
// void handleQuickRunStart(WebServer& server, ScheduleManager& scheduleManager);
// void handleQuickRunStop(WebServer& server, ScheduleManager& scheduleManager);
// void handleQuickRunStatus(WebServer& server, ScheduleManager& scheduleManager);

#endif // SPRINKLER_WEBSERVER_H
