
#include <Preferences.h>
#include <WiFi.h>
#include "network.h"
#include "config.h"

void loadNetworkSettings() {
    Preferences preferences;
    preferences.begin("network", true);
    local_IP.fromString(preferences.getString("local_ip", "192.168.1.76"));
    gateway.fromString(preferences.getString("gateway", "192.168.1.1"));
    subnet.fromString(preferences.getString("subnet", "255.255.255.0"));
    primaryDNS.fromString(preferences.getString("primaryDNS", "8.8.8.8"));
    secondaryDNS.fromString(preferences.getString("secondaryDNS", "8.8.4.4"));
    preferences.end();
}

String generateNetworkConfigHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Network Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; }
        input { width: 100%; margin: 10px 0; padding: 5px; }
        .submit-btn { background-color: #4CAF50; color: white; border: none; padding: 10px; cursor: pointer; }
    </style>
</head>
<body>
    <h1>Network Configuration</h1>
    <form action="/set_network" method="POST">
        <label>WiFi SSID:</label>
        <input type="text" name="wifi_ssid" value=")rawliteral";
    html += WIFI_SSID;
    html += R"rawliteral(" required>
        <label>WiFi Password:</label>
        <input type="password" name="wifi_password" value=")rawliteral";
    html += WIFI_PASSWORD;
    html += R"rawliteral(" required>
        <label>IP Address:</label>
        <input type="text" name="local_ip" value=")rawliteral";
    html += local_IP.toString();
    html += R"rawliteral(" required pattern="^(\d{1,3}\.){3}\d{1,3}$">
        <label>Gateway:</label>
        <input type="text" name="gateway" value=")rawliteral";
    html += gateway.toString();
    html += R"rawliteral(" required pattern="^(\d{1,3}\.){3}\d{1,3}$">
        <label>Subnet Mask:</label>
        <input type="text" name="subnet" value=")rawliteral";
    html += subnet.toString();
    html += R"rawliteral(" required pattern="^(\d{1,3}\.){3}\d{1,3}$">
        <label>Primary DNS:</label>
        <input type="text" name="primaryDNS" value=")rawliteral";
    html += primaryDNS.toString();
    html += R"rawliteral(" required pattern="^(\d{1,3}\.){3}\d{1,3}$">
        <label>Secondary DNS:</label>
        <input type="text" name="secondaryDNS" value=")rawliteral";
    html += secondaryDNS.toString();
    html += R"rawliteral(" required pattern="^(\d{1,3}\.){3}\d{1,3}$">
        <input type="submit" value="Save Network Settings" class="submit-btn">
    </form>
    <br>
    <a href="/">Back to Home</a>
</body>
</html>
)rawliteral";
    return html;
}

bool updateNetworkSettings(
    const String& localIPStr, 
    const String& gatewayStr, 
    const String& subnetStr, 
    const String& primaryDNSStr, 
    const String& secondaryDNSStr
) {
    IPAddress newLocalIP, newGateway, newSubnet, newPrimaryDNS, newSecondaryDNS;

    // Validate and parse IP addresses
    if (!newLocalIP.fromString(localIPStr) ||
        !newGateway.fromString(gatewayStr) ||
        !newSubnet.fromString(subnetStr) ||
        !newPrimaryDNS.fromString(primaryDNSStr) ||
        !newSecondaryDNS.fromString(secondaryDNSStr)) {
        // Serial.println("Invalid IP address format");
        return false;
    }

    // Update network settings in Preferences
    Preferences preferences;
    preferences.begin("network", false);
    preferences.putString("local_ip", newLocalIP.toString());
    preferences.putString("gateway", newGateway.toString());
    preferences.putString("subnet", newSubnet.toString());
    preferences.putString("primaryDNS", newPrimaryDNS.toString());
    preferences.putString("secondaryDNS", newSecondaryDNS.toString());
    preferences.end();

    // Update global network settings
    local_IP = newLocalIP;
    gateway = newGateway;
    subnet = newSubnet;
    primaryDNS = newPrimaryDNS;
    secondaryDNS = newSecondaryDNS;

    // Serial.println("Network settings updated successfully");
    return true;
}

void loadWiFiCredentials() {
    Preferences preferences;
    preferences.begin("wifi", true);
    // If nothing stored, defaults remain
    String ssid = preferences.getString("ssid", WIFI_SSID);
    String pass = preferences.getString("password", WIFI_PASSWORD);
    WIFI_SSID = ssid;
    WIFI_PASSWORD = pass;
    preferences.end();
    // Serial.println("WiFi credentials loaded: "+ WIFI_SSID);
}

bool updateWiFiCredentials(const String& ssid, const String& password) {
    Preferences preferences;
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    WIFI_SSID = ssid;
    WIFI_PASSWORD = password;
    // Serial.println("WiFi credentials updated successfully");
    return true;
}

bool configureNetworkSettings() {
    // Disconnect any existing WiFi connection
    WiFi.disconnect(true);
    delay(100);

    // Load the stored WiFi credentials
    loadWiFiCredentials();

    // Configure static IP
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        // Serial.println("WiFi configuration failed");
        return false;
    }

    // Connect to WiFi using stored credentials
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        
        // Timeout after 10 seconds
        if (millis() - startAttemptTime > 10000) {
            // Serial.println("\nWiFi connection failed");
            return false;
        }
    }
    
    Serial.println("\nWiFi connected!");
    // Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    return true;
}

void printNetworkConfiguration() {
    // Serial.println("Current Network Configuration:");
    // Serial.print("IP Address: ");
    // Serial.println(local_IP);
    // Serial.print("Gateway: ");
    // Serial.println(gateway);
    // Serial.print("Subnet: ");
    // Serial.println(subnet);
    // Serial.print("Primary DNS: ");
    // Serial.println(primaryDNS);
    // Serial.print("Secondary DNS: ");
    // Serial.println(secondaryDNS);
}
