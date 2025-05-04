#include "program_page.h"
#include "webserver.h"
#include "schedule.h"
#include <WebServer.h>
#include <ArduinoJson.h>

void handleProgramPage(WebServer& server, ScheduleManager& scheduleManager) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Irrigation Programs</title>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .program-container { margin-bottom: 30px; border: 1px solid #ccc; padding: 15px; border-radius: 5px; }
        .program-header { font-size: 1.2em; font-weight: bold; margin-bottom: 15px; }
        .time-input { width: 100px; }
        .duration-input { width: 40px; }
        table { width: 100%; border-collapse: collapse; margin: 10px 0; }
        th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }
        .days-container { margin: 10px 0; }
        .day-btn {
            padding: 8px;
            margin: 2px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .day-enabled { background-color: #4CAF50; color: white; }
        .day-disabled { background-color: #f1f1f1; color: #666; }
        .save-btn {
            background-color: #4CAF50;
            color: white;
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            margin-top: 20px;
        }
        .program-enabled { background-color: #e8f5e9; }
        .program-disabled { background-color: #f5f5f5; }
    </style>
</head>
<body>
)rawliteral";
    // Now add the current time header as a separate string, not inside the rawliteral
    // Timezone and DST info for header
    extern String currentTimeZoneName;
    extern bool currentDstEnabled;
    String tzInfo = currentTimeZoneName + " (DST: " + String(currentDstEnabled ? "On" : "Off") + ")";
    html += "<div class='header-row' style='display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;'>";
    html += "<div><h1 style='display:inline;'>Irrigation Programs</h1></div>";
    html += "<span id='currentTime' style='font-size:1.3em;font-weight:bold;'></span>";
    html += "</div>";
    // Sprinkler enable/disable buttons at the top
    html += "<div style='margin-bottom:15px;'><strong>Disable Sprinklers:</strong> ";
    for (int s = 0; s < scheduleManager.count; s++) {
        bool isDisabled = scheduleManager.sprinklers[s].disabled;
        html += "<button type='button' class='sprinkler-disable-btn";
        if (isDisabled) html += " disabled-btn";
        html += "' onclick='toggleSprinkler(this, " + String(s) + ")' id='sprinklerBtn" + String(s) + "'>";
        html += "Zone " + String(s + 1);
        html += "</button>";
        html += "<input type='hidden' name='sprinkler_disabled_" + String(s) + "' id='sprinklerDisabled" + String(s) + "' value='" + (isDisabled ? "1" : "0") + "'>";
    }
    html += "</div>";
    html += "<form id='programForm' action='/save_programs' method='POST'>";

    // Add each program section
    const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const char* programNames[] = {"A", "B", "C"};
    
    for (int p = 0; p < 3; p++) {
        Program& prog = scheduleManager.programs[p];
        
        html += "<div class='program-container " + String(prog.enabled ? "program-enabled" : "program-disabled") + "'>";
        html += "<div class='program-header'>Program " + String(programNames[p]) + "</div>";
        
        // Enable/disable toggle
        html += "<label><input type='checkbox' name='prog_" + String(p) + "_enabled' " +
                (prog.enabled ? "checked" : "") + "> Enable Program</label><br><br>";
        
        // Start time
        html += "Start Time: <input type='time' class='time-input' name='prog_" + String(p) + "_start' " +
                "value='" + prog.startTime + "'><br><br>";
        
        // Days of week
        html += "<div class='days-container'>Run on:";
        for (int d = 0; d < 7; d++) {
            html += "<button type='button' class='day-btn " + 
                   String(prog.daysOfWeek[d] ? "day-enabled" : "day-disabled") + "' " +
                   "onclick='toggleDay(" + String(p) + "," + String(d) + ")'>" +
                   String(days[d]) + "</button>";
            html += "<input type='hidden' name='prog_" + String(p) + "_day_" + String(d) + "' " +
                   "value='" + String(prog.daysOfWeek[d] ? "1" : "0") + "'>";
        }
        html += "</div>";
        
        // Zone durations table
        html += "<table><tr><th>Zone</th><th style='text-align:center;'>Duration (minutes)</th><th>End Time</th></tr>";
        for (int z = 0; z < scheduleManager.count; z++) {
            bool isDisabled = scheduleManager.sprinklers[z].disabled;
            String rowClass = isDisabled ? " style='background-color:#e0e0e0;opacity:0.6;'" : "";
            html += "<tr data-zone='" + String(z) + "'" + rowClass + "><td>Zone " + String(z + 1) + "</td>";
            html += "<td style='text-align:center;'><input type='number' class='duration-input' name='prog_" + String(p) + "_dur_" + String(z) + "' " +
                    "value='" + String(prog.durations[z]) + "' min='0' max='240'" + (isDisabled ? " disabled" : "") + "></td>";
            // Show calculated end time for this zone/program
            String endTime = scheduleManager.sprinklers[z].calculatedSchedules[p].endTime;
            // Display End Time in 12-hour format, left-justified
            String endTimeDisplay = "-";
            if (endTime.length() == 5) {
                int hour = endTime.substring(0, 2).toInt();
                int min = endTime.substring(3, 5).toInt();
                String ampm = "AM";
                int displayHour = hour;
                if (hour == 0) { displayHour = 12; ampm = "AM"; }
                else if (hour == 12) { displayHour = 12; ampm = "PM"; }
                else if (hour > 12) { displayHour = hour - 12; ampm = "PM"; }
                else { displayHour = hour; ampm = "AM"; }
                endTimeDisplay = String(displayHour) + ":" + (min < 10 ? "0" : "") + String(min) + " " + ampm;
            }
            html += "<td style='text-align:left; padding-left:0;'>" + endTimeDisplay + "</td></tr>";
        }
        html += "</table>";
        // Add Save Program X button
        html += "<button type='submit' name='save_program' value='" + String(p) + "' class='save-btn'>Save Program " + String(programNames[p]) + "</button>";
        // Add Return to Main Page button
        html += "<a href='/' class='save-btn' style='margin-left:15px;background-color:#2196F3;'>Return to Main Page</a>";
        html += "</div>";
    }
    
    html += R"rawliteral(
        <input type='submit' value='Save All Programs' class='save-btn'>
    </form>
    <script>
        function toggleDay(prog, day) {
            const btn = event.target;
            const input = document.querySelector(`input[name="prog_${prog}_day_${day}"]`);
            const isEnabled = btn.classList.contains('day-disabled');
            btn.classList.toggle('day-enabled');
            btn.classList.toggle('day-disabled');
            input.value = isEnabled ? '1' : '0';
        }
        // Sprinkler enable/disable logic
        function toggleSprinkler(btn, index) {
            const input = document.getElementById('sprinklerDisabled' + index);
            const isDisabled = btn.classList.contains('disabled-btn');
            // Find all table rows and duration inputs for this zone
            const rows = document.querySelectorAll('tr[data-zone="' + index + '"]');
            const inputs = document.querySelectorAll('input[name$="_dur_' + index + '"]');
            if (isDisabled) {
                btn.classList.remove('disabled-btn');
                input.value = '0';
                rows.forEach(function(row){ row.style.backgroundColor = ''; row.style.opacity = '1'; });
                inputs.forEach(function(inp){ inp.disabled = false; });
            } else {
                btn.classList.add('disabled-btn');
                input.value = '1';
                rows.forEach(function(row){ row.style.backgroundColor = '#e0e0e0'; row.style.opacity = '0.6'; });
                inputs.forEach(function(inp){ inp.disabled = true; });
                // Instantly disable relay via POST
                var xhr = new XMLHttpRequest();
                xhr.open('POST', '/disable_zone', true);
                xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
                xhr.send('zone=' + encodeURIComponent(index));
            }
            if (isDisabled) {
                // Was disabled, now enabling
                var xhr = new XMLHttpRequest();
                xhr.open('POST', '/enable_zone', true);
                xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
                xhr.send('zone=' + encodeURIComponent(index));
            }
        }
        // On page load, initialize all rows/inputs for disabled sprinklers
        window.addEventListener('DOMContentLoaded', function() {
            for (let i = 0; ; i++) {
                let btn = document.getElementById('sprinklerBtn' + i);
                let input = document.getElementById('sprinklerDisabled' + i);
                if (!btn || !input) break;
                if (input.value === '1') {
                    btn.classList.add('disabled-btn');
                    const rows = document.querySelectorAll(`tr[data-zone="${i}"]`);
                    const inputs = document.querySelectorAll(`input[name$="_dur_${i}"]`);
                    rows.forEach(row => { row.style.backgroundColor = '#e0e0e0'; row.style.opacity = '0.6'; });
                    inputs.forEach(inp => { inp.disabled = true; });
                }
            }
        });
    </script>
    <style>
        .sprinkler-disable-btn {
            margin: 0 4px 4px 0;
            padding: 6px 12px;
            border-radius: 4px;
            border: 1px solid #888;
            background: #f5f5f5;
            color: #444;
            cursor: pointer;
        }
        .sprinkler-disable-btn.disabled-btn {
            background: #bdbdbd;
            color: #888;
            opacity: 0.7;
        }
    </style>
<script>
    function updateTime() {
        fetch('/current_time')
          .then(response => response.json())
          .then(data => {
            document.getElementById('currentTime').innerHTML = '<b>' + data.time + '</b> <span style=\"font-weight:normal\">(' + data.timezone + ' DST: ' + (data.dst === 'true' ? 'On' : 'Off') + ')</span>';
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

void handleSavePrograms(WebServer& server, ScheduleManager& scheduleManager) {
    // Update sprinkler disabled/enabled state
    for (int s = 0; s < scheduleManager.count; s++) {
        String key = "sprinkler_disabled_" + String(s);
        if (server.hasArg(key)) {
            scheduleManager.sprinklers[s].disabled = (server.arg(key) == "1");
        } else {
            scheduleManager.sprinklers[s].disabled = false;
        }
    }
    // Process each program
    for (int p = 0; p < 3; p++) {
        Program& prog = scheduleManager.programs[p];
        
        // Enable/disable
        prog.enabled = server.hasArg("prog_" + String(p) + "_enabled");
        
        // Start time
        if (server.hasArg("prog_" + String(p) + "_start")) {
            prog.startTime = server.arg("prog_" + String(p) + "_start");
        }
        
        // Days of week
        for (int d = 0; d < 7; d++) {
            String dayArg = "prog_" + String(p) + "_day_" + String(d);
            if (server.hasArg(dayArg)) {
                prog.daysOfWeek[d] = (server.arg(dayArg) == "1");
            }
        }
        
        // Zone durations
        for (int z = 0; z < scheduleManager.count; z++) {
            String durArg = "prog_" + String(p) + "_dur_" + String(z);
            if (server.hasArg(durArg)) {
                prog.durations[z] = server.arg(durArg).toInt();
            }
        }
    }
    
    // Save programs to persistent storage
    scheduleManager.savePrograms();
    // Calculate today's schedules
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    int today = timeinfo->tm_wday - 1;  // Convert to our 0=Monday format
    if (today < 0) today = 6;  // Handle Sunday
    scheduleManager.calculateZoneSchedules(today);
    // Immediately update all sprinkler states after saving programs
    time_t now2;
    struct tm* timeinfo2;
    ::time(&now2);
    timeinfo2 = ::localtime(&now2);
    int today2 = timeinfo2->tm_wday == 0 ? 6 : timeinfo2->tm_wday - 1;
    int hour2 = timeinfo2->tm_hour;
    int minute2 = timeinfo2->tm_min;
    for (int i = 0; i < scheduleManager.count; i++) {
        bool shouldBeOn = scheduleManager.checkSprinklerSchedule(i, today2, hour2, minute2);
        scheduleManager.sprinklers[i].setState(shouldBeOn);
    }
    // Redirect back to program page
    server.sendHeader("Location", "/programs", true);
    server.send(302, "text/plain", "");
}
