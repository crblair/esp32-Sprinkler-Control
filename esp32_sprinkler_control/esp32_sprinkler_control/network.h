#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <WebServer.h>
#include "config.h"

// Load network settings
void loadNetworkSettings();

// Configure network with loaded settings (calls loadWiFiCredentials)
bool configureNetworkSettings();

// Generate HTML for network configuration page (now includes WiFi credentials)
String generateNetworkConfigHTML();

// Handle network configuration update (only network settings)
bool updateNetworkSettings(
    const String& localIPStr, 
    const String& gatewayStr, 
    const String& subnetStr, 
    const String& primaryDNSStr, 
    const String& secondaryDNSStr
);

// New functions to manage WiFi credentials (stored via Preferences)
void loadWiFiCredentials();
bool updateWiFiCredentials(const String& ssid, const String& password);

// Print current network configuration
void printNetworkConfiguration();

#endif // NETWORK_H
