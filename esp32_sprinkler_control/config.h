#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// Configuration Constants
#define MAX_PERIODS_PER_DAY 3

// WiFi Credentials as mutable global Strings
extern String WIFI_SSID;
extern String WIFI_PASSWORD;

// Firmware Version
extern const char* FIRMWARE_VERSION;

// Status LED
extern const int statusLedPin;

// Sprinkler Configuration
extern const int numSprinklers;
extern const int EEPROM_SCHEDULE_SIZE;
extern const int EEPROM_SIZE;

// Relay Configuration
extern const bool relayActiveLow[];
extern const int sprinklerPins[];

// Network Settings (global variables)
extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress primaryDNS;
extern IPAddress secondaryDNS;


// Utility functions


#endif // CONFIG_H
