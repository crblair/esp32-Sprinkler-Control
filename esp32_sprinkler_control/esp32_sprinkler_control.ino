#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include <ctime> // Ensure time functions and struct tm are available
#include <time.h>
#include "timeprefs.h"

#include "config.h"
#include "network_state.h"

// Temporary test object for incremental refactor
NetworkState testNetState;

#include "weekday.h"
/**
 * @file esp32_sprinkler_control.ino
 * @brief Main entry point for the ESP32-based sprinkler controller.
 *
 * Handles hardware initialization, WiFi/network setup, OTA updates, and the main loop for sprinkler control.
 * Integrates with web server and schedule management modules to provide manual, scheduled, and Quick Run zone control.
 */

#include "schedule.h"
#include "network.h"
#include "webserver.h"
#include "sprinkler_controller.h"
#include "sprinkler_system_state.h"
SprinklerSystemState manualState;

// Global variables
WebServer server(80);
// No longer needed: using configTime() for NTP and timezone/DST
SprinklerController sprinklerController(sprinklerPins, relayActiveLow, numSprinklers, statusLedPin);
ScheduleManager* scheduleManager;

// Timing variables
unsigned long lastHeartbeat = 0;
unsigned long lastScheduleCheck = 0;
unsigned long lastBlink = 0;
bool ledState = false;

// Wrapper functions for WebServer handlers
// REMOVED: handleRootWrapper is not needed, root route handled in setupWebServerRoutes


void handleStatusWrapper() {
  handleStatus(server, *scheduleManager, manualState);
}

void handleManualModeWrapper() {
  handleManualMode(server, *scheduleManager, manualState);
}

void handleNetworkConfigWrapper() {
  handleNetworkConfig(server);
}

void handleSetNetworkWrapper() {
  handleSetNetwork(server);
}

void handleUpdatePageWrapper() {
  handleUpdatePage(server);
}

// Wrappers for manual control
void handleManualControl0() { handleManualControl(server, *scheduleManager, manualState, 0); }
void handleManualControl1() { handleManualControl(server, *scheduleManager, manualState, 1); }
void handleManualControl2() { handleManualControl(server, *scheduleManager, manualState, 2); }
void handleManualControl3() { handleManualControl(server, *scheduleManager, manualState, 3); }
void handleManualControl4() { handleManualControl(server, *scheduleManager, manualState, 4); }
void handleManualControl5() { handleManualControl(server, *scheduleManager, manualState, 5); }
void handleManualControl6() { handleManualControl(server, *scheduleManager, manualState, 6); }
void handleManualControl7() { handleManualControl(server, *scheduleManager, manualState, 7); }

// Update handlers
void handleUpdateUploadWrapper() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    // Serial.printf("Update: %s\n", upload.filename.c_str());
    // Serial.println("Updating firmware only (preserving EEPROM data)");
    
    // Only update the program flash, not EEPROM
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      // Serial.printf("Update Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleUpdateCompleteWrapper() {
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    String html = "<html><head><title>Firmware Update</title></head><body><h1>Update Failed!</h1></body></html>";
    server.send(200, "text/html", html);
    Serial.println("Update failed");
  } else {
    String html = "<html><head><meta http-equiv='refresh' content='5;url=http://";
    html += local_IP.toString();
    html += "'><title>Firmware Update</title></head><body>";
    html += "<h1>Update Successful! Rebooting...</h1>";
    html += "<p>You will be redirected to the new IP address (";
    html += local_IP.toString();
    html += ") in 5 seconds. If not, click <a href='http://";
    html += local_IP.toString();
    html += "'>here</a>.</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    Serial.println("Update successful");
    delay(2000);
    ESP.restart();
  }
}

// Define a utility function to convert 24-hour time to 12-hour format
// This can be useful for displaying in the web interface
String format12HourTime(int hours, int minutes) {
  String ampm = "AM";
  if (hours >= 12) {
    ampm = "PM";
    if (hours > 12) {
      hours -= 12;
    }
  }
  if (hours == 0) {
    hours = 12;
  }
  
  char timeStr[10];
  sprintf(timeStr, "%d:%02d %s", hours, minutes, ampm.c_str());
  return String(timeStr);
}

#include <WiFi.h>
#include <WebServer.h>

/**
 * @brief Arduino setup function.
 *
 * Initializes hardware, loads configuration, connects to WiFi, sets up web server and OTA, and prepares the system for operation.
 */
void setup() {
  testNetState.ssid = WIFI_SSID; // Assign the global SSID to the encapsulated member

  Serial.begin(115200);
  
  sprinklerController.begin();
  // Ensure all relays start OFF at boot
  for (int i = 0; i < numSprinklers; i++) {
    sprinklerController.setRelay(sprinklerPins[i], false);
  }
  // Serial.println("\n\nESP32 Sprinkler Controller v" + String(FIRMWARE_VERSION));
  
  // Initialize the status LED
  pinMode(statusLedPin, OUTPUT);
  sprinklerController.setStatusLed(false); // Start with LED off

  // Try connecting to WiFi first
  loadNetworkSettings();

  // Register firmware update page route
  server.on("/update_page", HTTP_GET, handleUpdatePageWrapper);

  scheduleManager = new ScheduleManager(numSprinklers, &sprinklerController);
  scheduleManager->loadPrograms();
  // Ensure all sprinkler relay pins are initialized as OUTPUT
  for (int i = 0; i < numSprinklers; i++) {
    // Sprinkler::init is now handled in ScheduleManager constructor
  }
  // Force calculation of today's schedules after loading programs
  time_t now;
  struct tm timeinfo;
  int today;
  int wifiCounter = 0;
  time(&now);
  localtime_r(&now, &timeinfo);
  today = timeinfo.tm_wday - 1; // Convert to 0=Monday
  if (today < 0) today = 6;
  scheduleManager->calculateZoneSchedules(today);
  if (!scheduleManager->verifyPrograms()) {
    Serial.println("Invalid programs found, initializing...");
    scheduleManager->initializePrograms();
    scheduleManager->savePrograms();
  }

  // Initialize all relay pins and set to OFF
  for (int i = 0; i < numSprinklers; i++) {
    pinMode(sprinklerPins[i], OUTPUT);
    sprinklerController.setRelay(sprinklerPins[i], false); // Initialize all relays to OFF
    scheduleManager->sprinklers[i].state = false;
  }
  
  // Disconnect any current WiFi connection to force new settings
  WiFi.disconnect(true);
  delay(100);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  // Flash LED to indicate WiFi connection attempt
  for (int i = 0; i < 3; i++) {
    sprinklerController.setStatusLed(true);
    delay(200);
    sprinklerController.setStatusLed(false);
    delay(200);
  }
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  // Blink LED while connecting to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    sprinklerController.setStatusLed(wifiCounter % 2 == 0);
    wifiCounter++;
    if (wifiCounter > 20) { // 10 seconds timeout
      // WiFi failed, start AP mode
      WiFi.mode(WIFI_AP);
      WiFi.softAP("Sprinkler_Setup", "configure123");
      IPAddress apIP(192,168,4,1);
      IPAddress netMsk(255,255,255,0);
      WiFi.softAPConfig(apIP, apIP, netMsk);

      server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", "<h1>ESP32 Sprinkler AP Mode</h1><p>Connected!</p><p><a href='/wifi'>Configure WiFi</a></p>");
      });
      // WiFi configuration page (GET)
      server.on("/wifi", HTTP_GET, []() {
        int n = WiFi.scanNetworks();
        String ssidOptions = "";
        for (int i = 0; i < n; i++) {
          String s = WiFi.SSID(i);
          if (s.length() == 0) continue;
          ssidOptions += "<option value='" + s + "'";
          if (s == WIFI_SSID) ssidOptions += " selected";
          ssidOptions += ">" + s + "</option>";
        }
        // Determine if static IP was previously set
        Preferences prefs;
        prefs.begin("wifi", true);
        bool useStatic = prefs.getBool("use_static_ip", false);
        prefs.end();
        String html = "<html><head><title>WiFi Setup</title>\n";
        html += "<script>\nfunction toggleStaticIP() {\n  var checked = document.getElementById('use_static').checked;\n  var fields = document.getElementsByClassName('static-field');\n  for (var i = 0; i < fields.length; i++) {\n    fields[i].style.display = checked ? 'block' : 'none';\n  }\n}\nwindow.onload = toggleStaticIP;\n<\/script></head><body>";
        html += "<h1>WiFi Configuration</h1>";
        html += "<form method='POST' action='/wifi'>";
        html += "SSID: <select name='ssid'>" + ssidOptions + "</select><br>";
        html += "Password: <input name='password' type='text' value='" + WIFI_PASSWORD + "'><br>";
        html += "<input type='checkbox' id='use_static' name='use_static' value='1' onchange='toggleStaticIP()'";
        if (useStatic) html += " checked";
        html += "> Set Static IP<br>";
        html += String("<div class='static-field' style='display:") + (useStatic ? "block" : "none") + ";'>";
        html += "Static IP: <input name='ip' value='" + local_IP.toString() + "'><br>";
        html += "Gateway: <input name='gw' value='" + gateway.toString() + "'><br>";
        html += "Subnet: <input name='sn' value='" + subnet.toString() + "'><br>";
        html += "DNS1: <input name='dns1' value='" + primaryDNS.toString() + "'><br>";
        html += "DNS2: <input name='dns2' value='" + secondaryDNS.toString() + "'><br>";
        html += "</div>";
        html += "<input type='submit' value='Save & Connect'>";
        html += "</form></body></html>";
        server.send(200, "text/html", html);
      });
      // WiFi configuration (POST)
      server.on("/wifi", HTTP_POST, []() {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        bool useStatic = server.hasArg("use_static");
        String ip = server.arg("ip");
        String gw = server.arg("gw");
        String sn = server.arg("sn");
        String dns1 = server.arg("dns1");
        String dns2 = server.arg("dns2");
        // Store to Preferences
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", password);
        prefs.putBool("use_static_ip", useStatic);
        prefs.putString("ip", ip);
        prefs.putString("gw", gw);
        prefs.putString("sn", sn);
        prefs.putString("dns1", dns1);
        prefs.putString("dns2", dns2);
        prefs.end();
        server.send(200, "text/html", "<h1>Saved. Rebooting...</h1>");
        delay(1200);
        ESP.restart();
      });
      server.begin();
      Serial.println("AP mode started. Connect to Sprinkler_Setup, password: configure123, then browse to http://192.168.4.1");
      return;
    }
  }

  // Solid LED for 1 second to indicate successful connection
  sprinklerController.setStatusLed(true);
  delay(1000);
  sprinklerController.setStatusLed(false);

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize time using configTime()
  configTzTime("EST5EDT,M3.2.0/2,M11.1.0/2", "pool.ntp.org", "time.nist.gov");
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Waiting for NTP...");
    delay(1000);
  }
  Serial.printf("Local time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // Register all web server routes (including root)
  setupWebServerRoutes(server, *scheduleManager, manualState);

  setupOTA();

  // Visual indicator that OTA setup is complete
  Serial.println("OTA setup complete");
  for (int i = 0; i < 3; i++) {
    sprinklerController.setStatusLed(true);
    delay(100);
    sprinklerController.setStatusLed(false);
    delay(100);
  }

  server.begin();
  Serial.println("Web server started");
  Serial.print("Access the controller at http://");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Arduino main loop.
 *
 * Handles periodic tasks, processes web server requests, and manages sprinkler state (manual, scheduled, Quick Run).
 */
void loop() {
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastScheduleCheck = 0;
  static unsigned long lastDiagnostic = 0;
  
  // Process web requests and OTA
  server.handleClient();
  ArduinoOTA.handle();

  // --- QUICK RUN OVERRIDE ---
  if (scheduleManager->isQuickRunActive()) {
    scheduleManager->updateQuickRun();
    // While Quick Run is active, skip all other schedule/manual logic
    return;
  }
  
  if (millis() - lastDiagnostic > 1000) {  // Every 1 second
    lastDiagnostic = millis();
    
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Convert to our day indexing (0=Monday to 6=Sunday)
    int systemDay = timeinfo.tm_wday; // 0=Sunday in system
    int ourDay = (systemDay == 0) ? 6 : systemDay - 1; // Convert to our format
    
    Serial.printf("\n--------- SCHEDULE DIAGNOSTICS ---------\n");
    Serial.printf("Current date/time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    Serial.printf("Day of Week: %d (System), %d (Our format, 0=Mon)\n", systemDay, ourDay);
    Serial.printf("Day Name: %s\n", WeekdayUtils::toString((Weekday)ourDay).c_str());
    
    // Calculate minutes since midnight
    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    Serial.printf("Minutes since midnight: %d\n", currentMinutes);
    
    // Check each sprinkler's schedule for today
    Serial.println("Today's schedules:");
    bool anyPeriodsFound = false;
    
    for (int i = 0; i < numSprinklers; i++) {
      // Check each calculated schedule for this sprinkler
      for (int p = 0; p < 3; p++) {
        
        bool hasSchedules = false;
        Serial.printf("\nSprinkler %d:\n", i + 1);
        
        for (int p = 0; p < 3; p++) {
          CalculatedSchedule &schedule = scheduleManager->sprinklers[i].calculatedSchedules[p];
          if (schedule.startTime.length() == 5 && schedule.endTime.length() == 5) {
            hasSchedules = true;
            anyPeriodsFound = true;
            Serial.printf("  Program %d: %s - %s\n",
                          p + 1, schedule.startTime.c_str(), schedule.endTime.c_str());
          }
        }
        
        if (!hasSchedules) {
          Serial.println("  No active schedules");
        }
      }
      
      if (!anyPeriodsFound) {
        Serial.println("No active schedules found for any sprinkler.");
      }
    }
  }
  
  // Simple heartbeat indicator every 10 seconds
  if (millis() - lastHeartbeat > 10000) {
    Serial.println("Controller running, waiting for connections...");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Current Day: ");
    Serial.println(WeekdayUtils::toString((Weekday)WeekdayUtils::todayAsIndex()));
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    Serial.print("Current Time: ");
    Serial.printf("%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
    Serial.print("Manual Mode: ");
    Serial.println(manualState.isManualMode() ? "ON" : "OFF");
    lastHeartbeat = millis();
  }
  
  // Check schedules every minute
  if (!manualState.isManualMode() && millis() - lastScheduleCheck >= 1000) {
    lastScheduleCheck = millis();
    
    // Get current time from system clock
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentTotal = currentHour * 60 + currentMinute;
    
    // Get current weekday (0 = Monday)
    int today = (timeinfo.tm_wday + 6) % 7;
    
    Serial.printf("\nChecking schedules at %02d:%02d on %s (Day %d)\n", 
                  currentHour, currentMinute, 
                  WeekdayUtils::toString((Weekday)today).c_str(), today);
    
    static int lastDay = -1;
    if (today != lastDay) {
      // Day has changed, recalculate schedules
      Serial.println("New day detected, recalculating schedules");
      scheduleManager->calculateZoneSchedules(today);
      lastDay = today;
    }
    
    // Check if any sprinklers need to be toggled based on schedule
    for (int i = 0; i < scheduleManager->count; i++) {
      // Skip if sprinkler is disabled
      if (scheduleManager->sprinklers[i].disabled) {
        if (scheduleManager->sprinklers[i].state) {
          // Turn off if it was on
          scheduleManager->sprinklers[i].setState(false);
          // Serial.printf("Sprinkler %d turned OFF (disabled)\n", i + 1);
        }
        continue;
      }

      bool shouldBeOn = false;
      
      // Check each calculated schedule for this sprinkler
      for (int p = 0; p < 3; p++) {
        CalculatedSchedule &schedule = scheduleManager->sprinklers[i].calculatedSchedules[p];
        
        // Skip if no schedule
        if (schedule.startTime.length() != 5 || schedule.endTime.length() != 5) continue;
        
        int startHour = schedule.startTime.substring(0, 2).toInt();
        int startMin  = schedule.startTime.substring(3, 5).toInt();
        int endHour   = schedule.endTime.substring(0, 2).toInt();
        int endMin    = schedule.endTime.substring(3, 5).toInt();
        int startTotal = startHour * 60 + startMin;
        int endTotal = endHour * 60 + endMin;
        
        Serial.printf("  Checking Program %d: %02d:%02d (%d min) - %02d:%02d (%d min)\n", 
                      schedule.programIndex + 1, startHour, startMin, startTotal, endHour, endMin, endTotal);
        
        bool periodShouldBeOn = false;
        if (startTotal < endTotal) {
          periodShouldBeOn = (currentTotal >= startTotal && currentTotal < endTotal);
          Serial.printf("  Normal period (within day), Current: %d, Should be on: %s\n", 
                        currentTotal, periodShouldBeOn ? "YES" : "no");
        } else if (startTotal > endTotal) {
          periodShouldBeOn = (currentTotal >= startTotal || currentTotal < endTotal);
          Serial.printf("  Overnight period (spans midnight), Current: %d, Should be on: %s\n", 
                        currentTotal, periodShouldBeOn ? "YES" : "no");
        }
        
        if (periodShouldBeOn) {
          shouldBeOn = true;
          Serial.printf("  Program %d is ACTIVE - sprinkler should be ON\n", schedule.programIndex + 1);
          break;
        }
      }
      
      // Update relay state if necessary using Sprinkler::setState()
      if (scheduleManager->sprinklers[i].state != shouldBeOn) {
        Serial.printf("  Changing state: Setting Sprinkler %d to %s\n", 
                      i + 1, shouldBeOn ? "ON" : "OFF");
        scheduleManager->sprinklers[i].setState(shouldBeOn);
        Serial.printf("Sprinkler %d turned %s\n", i + 1, shouldBeOn ? "ON" : "OFF");
      }
    }
    
    // Blink the LED if any sprinkler is active in scheduled mode
    bool anyActive = false;
    for (int i = 0; i < scheduleManager->count; i++) {
      if (scheduleManager->sprinklers[i].state) {
        anyActive = true;
        break;
      }
    }
    
    if (anyActive) {
      if (millis() - lastBlink >= 500) {
        ledState = !ledState;
        sprinklerController.setStatusLed(ledState);
        lastBlink = millis();
      }
    } else {
      sprinklerController.setStatusLed(false);
    }
  } else if (!manualState.isManualMode()) {
    // LED blinking when not checking schedules
    bool anyActive = false;
    for (int i = 0; i < scheduleManager->count; i++) {
      if (scheduleManager->sprinklers[i].state) {
        anyActive = true;
        break;
      }
    }
    
    if (anyActive) {
      if (millis() - lastBlink >= 500) {
        ledState = !ledState;
        sprinklerController.setStatusLed(ledState);
        lastBlink = millis();
      }
    }
  }
  
  delay(5); // Much shorter delay for responsiveness
}