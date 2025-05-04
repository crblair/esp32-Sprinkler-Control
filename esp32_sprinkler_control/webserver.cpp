#include "network_state.h"
extern NetworkState testNetState;
#include <WiFi.h>
#include <WebServer.h>
#include <time.h> // Only standard time functions needed
#include <ArduinoJson.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

// Globals for timezone persistence
String currentTimeZoneName = "Eastern";
bool currentDstEnabled = false;

/**
 * @file webserver.cpp
 * @brief Implements the web server routes and HTTP handlers for the ESP32 sprinkler controller.
 *
 * Provides endpoints for manual control, Quick Run, configuration, and status display.
 */

#include "webserver.h"
#include "config.h"
#include "network.h"

void handleCurrentTime(WebServer& server) {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    String time12 = formatTime12Hour(String(buf));
    String tzInfo = currentTimeZoneName + " (DST: " + String(currentDstEnabled ? "On" : "Off") + ")";
    server.send(200, "application/json", "{\"time\":\"" + time12 + "\",\"timezone\":\"" + currentTimeZoneName + "\",\"dst\":\"" + (currentDstEnabled ? "true" : "false") + "\"}");
}

#include "schedule.h"
#include "timeprefs.h"
#include "program_page.h"

// OTA Setup
void setupOTA() {
    ArduinoOTA.setHostname("SprinklerController");
    
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    });
    
    ArduinoOTA.onEnd([]() {
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        if (error == OTA_AUTH_ERROR) ;
        else if (error == OTA_BEGIN_ERROR) ;
        else if (error == OTA_CONNECT_ERROR) ;
        else if (error == OTA_RECEIVE_ERROR) ;
        else if (error == OTA_END_ERROR) ;
    });
    
    ArduinoOTA.begin();
}

// Helper function to convert 24-hour time to 12-hour format
String formatTime12Hour(const String& time24hr) {
  // Parse hours and minutes
  int hours = time24hr.substring(0, 2).toInt();
  String minutes = time24hr.substring(3, 5);
  
  // Convert to 12-hour format
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
  
  // Format the result
  return String(hours) + ":" + minutes + " " + ampm;
}

// Set your time zone offset in seconds here. Example: -5*3600 for EST, -8*3600 for PST, etc.
// US time zone offsets in seconds
#define TZ_OFFSET_EASTERN   (-5 * 3600)
#define TZ_OFFSET_CENTRAL   (-6 * 3600)
#define TZ_OFFSET_MOUNTAIN  (-7 * 3600)
#define TZ_OFFSET_PACIFIC   (-8 * 3600)
#define TZ_OFFSET_ALASKA    (-9 * 3600)
#define TZ_OFFSET_HAWAII   (-10 * 3600)

// Returns day of week for a given date (0=Sunday, 1=Monday, ..., 6=Saturday)
int dayOfWeek(int year, int month, int day) {
    if (month < 3) {
        month += 12;
        year--;
    }
    int K = year % 100;
    int J = year / 100;
    int h = (day + 13*(month+1)/5 + K + K/4 + J/4 + 5*J) % 7;
    // h = 0 is Saturday, so adjust:
    int d = (h + 6) % 7;
    return d;
}

// Handler for WiFi Setup button
void handleWiFiSetup(WebServer& server) {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putBool("force_ap", true);
    prefs.end();
    server.send(200, "text/html", "<h1>Rebooting for WiFi Setup...</h1><p>Connect to the Sprinkler_Setup WiFi network to configure.</p>");
    delay(1500);
    ESP.restart();
}

// Web Server Routes Setup
void setupWebServerRoutes(
    WebServer& server, 
    ScheduleManager& scheduleManager, 
    SprinklerSystemState& manualState
) {
    // Handler for instant disable relay from UI
    // Handler for WiFi Setup (GET shows form, POST saves settings)
    server.on("/wifi_setup", HTTP_GET, [&]() {
        // Scan networks
        int n = WiFi.scanNetworks();
        String ssidOptions = "";
        for (int i = 0; i < n; i++) {
            String s = WiFi.SSID(i);
            if (s.length() == 0) continue;
            ssidOptions += "<option value='" + s + "'";
            if (s == testNetState.ssid) ssidOptions += " selected";
            ssidOptions += ">" + s + "</option>";
        }
        Preferences prefs;
        prefs.begin("wifi", true);
        bool useStatic = prefs.getBool("use_static_ip", false);
        prefs.end();
        String html = "<html><head><title>WiFi Setup</title>\n";
        html += "<script>\nfunction toggleStaticIP() {\n  var checked = document.getElementById('use_static').checked;\n  var fields = document.getElementsByClassName('static-field');\n  for (var i = 0; i < fields.length; i++) {\n    fields[i].style.display = checked ? 'block' : 'none';\n  }\n}\nwindow.onload = toggleStaticIP;\n<\/script></head><body>";
        html += "<h1>WiFi Configuration</h1>";
        html += "<form method='POST' action='/wifi_setup'>";
        html += "SSID: <select name='ssid'>" + ssidOptions + "</select><br>";
        html += "Password: <input name='password' type='text' value='" + testNetState.password + "'><br>";
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
    server.on("/wifi_setup", HTTP_POST, [&]() {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        bool useStatic = server.hasArg("use_static");
        String ip = server.arg("ip");
        String gw = server.arg("gw");
        String sn = server.arg("sn");
        String dns1 = server.arg("dns1");
        String dns2 = server.arg("dns2");
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

    // Handler for instant disable relay from UI
    server.on("/disable_zone", HTTP_POST, [&](){
        if (server.hasArg("zone")) {
            int zone = server.arg("zone").toInt();
            if (zone >= 0 && zone < scheduleManager.count) {
                scheduleManager.sprinklers[zone].disabled = true;
                scheduleManager.sprinklers[zone].setState(false);
                server.send(200, "text/plain", "OK");
                return;
            }
        }
        server.send(400, "text/plain", "Invalid zone");
    });

    // Handler for instant enable relay from UI
    server.on("/enable_zone", HTTP_POST, [&](){
        if (server.hasArg("zone")) {
            int zone = server.arg("zone").toInt();
            if (zone >= 0 && zone < scheduleManager.count) {
                scheduleManager.sprinklers[zone].disabled = false;
                // Check if it should be ON right now
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);
                int today = t->tm_wday == 0 ? 6 : t->tm_wday - 1;
                int hour = t->tm_hour;
                int minute = t->tm_min;
                bool shouldBeOn = scheduleManager.checkSprinklerSchedule(zone, today, hour, minute);
                scheduleManager.sprinklers[zone].setState(shouldBeOn);
                server.send(200, "text/plain", "OK");
                return;
            }
        }
        server.send(400, "text/plain", "Invalid zone");
    });
    // Route for AJAX time polling
    server.on("/current_time", HTTP_GET, [&](){ handleCurrentTime(server); });

    // --- QUICK RUN ENDPOINTS ---
    server.on("/quick_run/start", HTTP_POST, [&]() {
        int duration = 10; // default
        if (server.hasArg("duration")) duration = server.arg("duration").toInt();
        scheduleManager.startQuickRun(duration);
        server.send(200, "application/json", "{\"result\":true,\"message\":\"Quick Run started\"}");
    });
    server.on("/quick_run/stop", HTTP_POST, [&]() {
        scheduleManager.stopQuickRun();
        server.send(200, "application/json", "{\"result\":true,\"message\":\"Quick Run stopped\"}");
    });
    server.on("/quick_run/status", HTTP_GET, [&]() {
        String json = "{";
        json += "\"isActive\":" + String(scheduleManager.isQuickRunActive() ? "true" : "false");
        json += ",\"currentZone\":" + String(scheduleManager.quickRunCurrentZone);
        json += ",\"zoneIndex\":" + String(scheduleManager.quickRunZoneIndex);
        json += ",\"numZones\":" + String(scheduleManager.quickRunNumZones);
        json += ",\"duration\":" + String(scheduleManager.quickRunDurationPerZone/1000);
        json += "}";
        server.send(200, "application/json", json);
    });

    // Program-based scheduling routes
    server.on("/programs", HTTP_GET, [&](){ handleProgramPage(server, scheduleManager); });
    server.on("/save_programs", HTTP_POST, [&](){ handleSavePrograms(server, scheduleManager); });

    // Timezone settings handler
    // Helper to map timezone name and DST to TZ string
    auto getTZString = [](const String& tzName, bool dst) -> const char* {
        if (tzName == "Eastern") return dst ? "EST5EDT,M3.2.0/2,M11.1.0/2" : "EST5";
        if (tzName == "Central") return dst ? "CST6CDT,M3.2.0/2,M11.1.0/2" : "CST6";
        if (tzName == "Mountain") return dst ? "MST7MDT,M3.2.0/2,M11.1.0/2" : "MST7";
        if (tzName == "Pacific") return dst ? "PST8PDT,M3.2.0/2,M11.1.0/2" : "PST8";
        if (tzName == "Alaska") return dst ? "AKST9AKDT,M3.2.0/2,M11.1.0/2" : "AKST9";
        if (tzName == "Hawaii") return "HST10"; // Hawaii does not observe DST
        return "EST5EDT,M3.2.0/2,M11.1.0/2"; // Default
    };

    server.on("/set_timezone", HTTP_POST, [&](){
        if (!server.hasArg("timezone")) {
            server.send(400, "text/plain", "Missing timezone parameter");
            return;
        }
        String tz = server.arg("timezone");
        bool dst = server.hasArg("dst") && server.arg("dst") == "on";
        // Save to global variables
        currentTimeZoneName = tz;
        currentDstEnabled = dst;
        // Persist to Preferences
        Preferences prefs;
        prefs.begin("sprinkler_time", false);
        prefs.putString("timezone", tz);
        prefs.putBool("dst", dst);
        prefs.end();
        // Apply timezone immediately
        configTzTime(getTZString(currentTimeZoneName, currentDstEnabled), "pool.ntp.org");
        server.sendHeader("Location", "/", true);
        server.send(302, "");
    });

    // Helper to load timezone/DST at startup
    auto loadTimezonePrefs = [&](){
        Preferences prefs;
        prefs.begin("sprinkler_time", true);
        currentTimeZoneName = prefs.getString("timezone", "Eastern");
        currentDstEnabled = prefs.getBool("dst", false);
        prefs.end();
        // Apply timezone on boot
        configTzTime(getTZString(currentTimeZoneName, currentDstEnabled), "pool.ntp.org");
    };
    loadTimezonePrefs();

    // Root route
    server.on("/", HTTP_GET, [&]() { handleRoot(server, scheduleManager, manualState); });

    // Manual mode toggle
    server.on("/manual", HTTP_POST, [&]() {
        handleManualMode(server, scheduleManager, manualState);
    });

    // Manual control routes for each sprinkler
    for (int i = 0; i < numSprinklers; i++) {
        // Determine if this sprinkler is active according to the schedule
        time_t raw = time(nullptr);
        struct tm* t = localtime(&raw);
        bool isActive = scheduleManager.checkSprinklerSchedule(
            i,
            t->tm_wday == 0 ? 6 : t->tm_wday - 1, // Convert Sunday=0 to 6, Mon=1 to 0, etc.
            t->tm_hour,
            t->tm_min
        );
        String nameClass = isActive ? "sprinkler-active" : "";
        server.on("/manual_control" + String(i), HTTP_POST, [&, i]() {
            handleManualControl(server, scheduleManager, manualState, i);
        });
    }
}

void handleRoot(
    WebServer& server, 
    ScheduleManager& scheduleManager,
    SprinklerSystemState& manualState
) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Sprinkler Control</title>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <style>
      body { font-family: Arial, sans-serif; margin: 10px; background: #f8fafc; }
      .header-row {
        display: flex;
        align-items: center;
        justify-content: flex-start;
        background: #e3f2fd;
        box-shadow: 0 2px 8px rgba(44,62,80,0.08);
        border-radius: 8px;
        padding: 18px 20px 12px 20px;
        margin-bottom: 18px;
        position: relative;
        max-width: 580px;
        margin-left: auto;
        margin-right: auto;
      }
      .header-title {
        flex: 1;
        text-align: center;
        font-size: 2em;
        font-weight: bold;
        color: #2c3e50;
        letter-spacing: 1px;
        margin: 0;
      }
      #currentTime {
        font-size: 1.05em;
        font-weight: bold;
        margin-right: auto;
      }
      .tzinfo { font-size: 1em; color: #333; margin-left: 20px; }
      table {
        width: 100%;
        border-collapse: separate;
        border-spacing: 0;
        border-radius: 8px;
        overflow: hidden;
        background: #fff;
        box-shadow: 0 1px 4px rgba(44,62,80,0.06);
      }
      th, td {
        border: 1px solid #ddd;
        padding: 10px 8px;
        text-align: center;
      }
      tr:hover {
        background: #f1f7fa;
      }
      th {
        background: #e3f2fd;
        font-weight: bold;
      }
      .btn {
        display: inline-block;
        padding: 8px 14px;
        background-color: #4CAF50;
        color: white;
        text-decoration: none;
        border-radius: 6px;
        margin: 6px 0;
        border: none;
        transition: background 0.2s;
      }
      .btn:hover {
        background-color: #388e3c;
      }
      .status-on { color: green; font-weight: bold; }
      .status-off { color: red; }
      .header-container {
        display: flex;
        justify-content: space-between;
        align-items: center;
      }
      .current-time {
        font-size: 18px;
        font-weight: bold;
        color: #2c3e50;
        margin-bottom: 15px;
      }
      .feedback {
        padding: 10px;
        background-color: #e7f3fe;
        border-left: 4px solid #2196F3;
        margin: 10px 0;
        display: none;
      }
      .current-settings {
        font-style: italic;
        color: #666;
        margin-bottom: 10px;
      }
      .sprinkler-active { color: #2ecc40; font-weight: bold; }
    </style>
</head>
<body>
    <div style='display:flex; flex-direction:row; align-items:center; width:100%; margin-bottom:18px;'>
        <div style='display:flex; flex-direction:column; align-items:flex-start; justify-content:center; min-width:190px; padding-left:10px;'>
            <span id='currentTime' style='font-size:1.05em;'></span>
            <span style='font-size:0.95em; color:#555;'>Firmware Version: )rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span>
        </div>
        <div style='flex:1;'>
            <div class='header-row' style='margin-bottom:0;'>
                <span class='header-title'>Sprinkler Control System</span>
            </div>
        </div>
    </div>
    <h2>Sprinkler Status</h2>
    <table>
        <tr>
            <th>Sprinkler</th>
            <th>Status</th>
            <th>Control</th>
        </tr>
)rawliteral";

    for (int i = 0; i < numSprinklers; i++) {
        Sprinkler& sprinkler = scheduleManager.getSprinkler(i);
        // Check if this sprinkler is currently scheduled to be active
        time_t raw = time(nullptr);
        struct tm* t = localtime(&raw);
        bool isActive = scheduleManager.checkSprinklerSchedule(
            i,
            t->tm_wday == 0 ? 6 : t->tm_wday - 1, // Convert Sunday=0 to 6, Mon=1 to 0, etc.
            t->tm_hour,
            t->tm_min
        );
        String nameClass = isActive ? "sprinkler-active" : "";
        html += "<tr><td><span class='" + nameClass + "'>Sprinkler " + String(i + 1) + "</span></td>";
        if (sprinkler.state) {
            html += "<td class='status-on'>ON</td>";
        } else {
            html += "<td class='status-off'>OFF</td>";
        }
        html += "<td><form method='POST' action='/manual_control" + String(i) + "'>";
        html += "<button type='submit' class='btn'>" + String(sprinkler.state ? "Turn OFF" : "Turn ON") + "</button>";
        html += "</form></td></tr>";
    }

    html += R"rawliteral(
    </table>
    <br>
    <!-- Quick Run Controls -->
    <form id='quickRunForm' style='display:inline-flex; align-items:center; gap:8px; margin-bottom:4px;' onsubmit='return quickRunSubmit(event);'>
      <label for='quickRunDuration' style='margin-right:4px;'>Quick Run</label>
      <input id='quickRunDuration' name='duration' type='number' min='1' max='3600' value='10' style='width:48px; text-align:center;' title='Seconds per zone'>
      <span style='font-size:0.95em; margin-left:2px;'>(sec)</span>
      <button id='quickRunBtn' type='submit'>Start</button>
      <span id='quickRunStatus' style='font-size:0.95em; margin-left:8px;'></span>
    </form>
    <script>
      let quickRunActive = false;
      function updateQuickRunUI(state) {
        quickRunActive = state.isActive;
        document.getElementById('quickRunBtn').textContent = quickRunActive ? 'Stop' : 'Start';
        document.getElementById('quickRunDuration').disabled = quickRunActive;
        let status = '';
        if (quickRunActive) {
          status = `Running zone ${state.zoneIndex+1} of ${state.numZones} (Zone #${state.currentZone+1})`;
        } else {
          status = 'Idle';
        }
        document.getElementById('quickRunStatus').textContent = status;
      }
      async function pollQuickRun() {
        try {
          const res = await fetch('/quick_run/status');
          const state = await res.json();
          updateQuickRunUI(state);
        } catch {}
        setTimeout(pollQuickRun, 1000);
      }
      pollQuickRun();
      async function quickRunSubmit(e) {
        e.preventDefault();
        if (quickRunActive) {
          await fetch('/quick_run/stop', {method:'POST'});
        } else {
          const duration = document.getElementById('quickRunDuration').value;
          await fetch('/quick_run/start', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:`duration=${duration}` });
        }
        setTimeout(pollQuickRun, 250);
        return false;
      }
    </script>
    <div style='display:flex; align-items:center; justify-content:center; gap:24px;'>
        <p style='margin:0;'><strong>Current Mode:</strong> )rawliteral" + String(manualState.isManualMode() ? "Manual Control" : "Automatic Schedule") + R"rawliteral(</p>
        <div class="button-bar" style='margin:0;'>
            <form method="POST" action="/manual" style="display:inline;">
                <button type="submit" class="nav-btn">)rawliteral" + String(manualState.isManualMode() ? "Switch to Automatic" : "Switch to Manual") + R"rawliteral(</button>
            </form>
            <form action="/update_page" method="GET" style="display:inline;">
                <button class="nav-btn" type="submit">Firmware Update</button>
            </form>
            <form action="/wifi_setup" method="GET" style="display:inline;">
                <button class="nav-btn" type="submit">WiFi Setup</button>
            </form>
            <form action="/programs" method="GET" style="display:inline;">
                <button class="nav-btn" type="submit">Edit Programs</button>
            </form>
        </div>
    </div>
    <br>
    <form method="POST" action="/set_timezone" class="tz-form">
      <label for="timezone"><strong>Time Zone:</strong></label>
      <select id="timezone" name="timezone" class="tz-select">
        <option value="Eastern" " + String(strcmp(currentTimeZoneName, "Eastern") == 0 ? "selected" : "") + ">Eastern (EST/EDT)</option>
        <option value="Central" " + String(strcmp(currentTimeZoneName, "Central") == 0 ? "selected" : "") + ">Central (CST/CDT)</option>
        <option value="Mountain" " + String(strcmp(currentTimeZoneName, "Mountain") == 0 ? "selected" : "") + ">Mountain (MST/MDT)</option>
        <option value="Pacific" " + String(strcmp(currentTimeZoneName, "Pacific") == 0 ? "selected" : "") + ">Pacific (PST/PDT)</option>
        <option value="Alaska" " + String(strcmp(currentTimeZoneName, "Alaska") == 0 ? "selected" : "") + ">Alaska (AKST/AKDT)</option>
        <option value="Hawaii" " + String(strcmp(currentTimeZoneName, "Hawaii") == 0 ? "selected" : "") + ">Hawaii (HST)</option>
      </select>
      <label style='margin-left:16px;'><input type='checkbox' name='dst' id='dst' " + String(currentDstEnabled ? "checked" : "") + "> DST</label>
      <button type="submit" class="nav-btn" style="margin-left:16px;">Set Time Zone</button>
    </form>
    <style>
      .button-bar {
        display: flex;
        flex-wrap: wrap;
        justify-content: center;
        gap: 16px;
        margin: 24px 0 10px 0;
      }
      .nav-btn {
        background-color: #2196F3;
        color: white;
        border: none;
        border-radius: 6px;
        padding: 10px 22px;
        font-size: 1em;
        cursor: pointer;
        margin: 0 4px;
        transition: background 0.2s;
      }
      .nav-btn:hover {
        background-color: #1976d2;
      }
      .tz-form {
        display: flex;
        align-items: center;
        justify-content: center;
        margin: 18px 0 0 0;
        flex-wrap: wrap;
        gap: 10px;
      }
      .tz-select {
        font-size: 1em;
        padding: 6px 12px;
        border-radius: 4px;
        border: none;
        margin-left: 8px;
        background-color: #2196F3;
        color: white;
        transition: background 0.2s;
      }
      .tz-select:focus, .tz-select:hover {
        background-color: #1976d2;
        color: white;
        outline: none;
      }
    </style>
<script>
    function updateTime() {
        fetch('/current_time')
          .then(response => response.json())
          .then(data => {
            document.getElementById('currentTime').innerHTML = '<span style="font-weight:bold;">' + data.time + '</span> <span style="font-weight:normal;">(' + data.timezone + ' DST: ' + (data.dst === 'true' ? 'On' : 'Off') + ')</span>';
          });
      }
      setInterval(updateTime, 1000);
      updateTime();
</script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}

/**
 * @brief Handles HTTP request to stop Quick Run from the web interface.
 *
 * Stops Quick Run operation and redirects to the main page.
 * @param server Reference to the web server instance.
 * @param scheduleManager Reference to the schedule manager.
 */
void handleStopQuickRun(WebServer& server, ScheduleManager& scheduleManager) {
    String json = "{ \"sprinklers\": [";
    
    for (int i = 0; i < numSprinklers; i++) {
        Sprinkler& sprinkler = scheduleManager.getSprinkler(i);
        json += "{\"id\":" + String(i) + ",\"state\":" + String(sprinkler.state ? "true" : "false") + "}";
        if (i < numSprinklers - 1) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

/**
 * @brief Handles HTTP request to start Quick Run from the web interface.
 *
 * Initiates Quick Run for all enabled zones and redirects to the main page.
 * @param server Reference to the web server instance.
 * @param scheduleManager Reference to the schedule manager.
 */
void handleQuickRun(WebServer& server, ScheduleManager& scheduleManager) {
    scheduleManager.startQuickRun(10);
    server.sendHeader("Location", "/", true);
    server.send(302, "");
}

void handleSetNetwork(WebServer& server) {
    if (!server.hasArg("local_ip") || !server.hasArg("gateway") ||
        !server.hasArg("subnet") || !server.hasArg("primaryDNS") ||
        !server.hasArg("secondaryDNS")) {
        server.send(400, "text/plain", "Missing network parameters");
        return;
    }

    bool success = updateNetworkSettings(
        server.arg("local_ip"),
        server.arg("gateway"),
        server.arg("subnet"),
        server.arg("primaryDNS"),
        server.arg("secondaryDNS")
    );

    if (success) {
        server.send(200, "text/html", 
            "<h1>Network Settings Updated</h1>"
            "<p>Device will restart to apply new settings.</p>"
            "<script>setTimeout(function(){window.location.href='/';}, 5000);</script>"
        );
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Failed to update network settings");
    }
}

void handleUpdatePage(WebServer& server) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Firmware Update</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; }
        input[type="file"] { width: 100%; margin: 10px 0; }
        .submit-btn { background-color: #4CAF50; color: white; border: none; padding: 10px; cursor: pointer; }
    </style>
</head>
<body>
    <h1>Firmware Update</h1>
    <form method="POST" action="/update" enctype="multipart/form-data">
        <input type="file" name="update" accept=".bin" required>
        <input type="submit" value="Upload Firmware" class="submit-btn">
    </form>
    <br>
    <a href="/">Back to Home</a>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}        

// --- STUBS FOR MANUAL MODE AND MANUAL CONTROL ---
#include "webserver.h"

void handleManualMode(WebServer& server, ScheduleManager& scheduleManager, SprinklerSystemState& manualState) {
    // Toggle manual mode
    manualState.setManualMode(!manualState.isManualMode());
    // If switching to manual, turn off all sprinklers
    if (manualState.isManualMode()) {
        for (int i = 0; i < scheduleManager.count; i++) {
            scheduleManager.sprinklers[i].setState(false);
        }
    }
    // Redirect back to home page
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

void handleManualControl(WebServer& server, ScheduleManager& scheduleManager, SprinklerSystemState& manualState, int index) {
    // Only allow control if manual mode is enabled
    if (!manualState.isManualMode()) {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
        return;
    }
    // Toggle the state of the selected sprinkler zone
    if (index >= 0 && index < scheduleManager.count) {
        bool currentState = scheduleManager.sprinklers[index].state;
        scheduleManager.sprinklers[index].setState(!currentState);
    }
    // Redirect back to home page
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}