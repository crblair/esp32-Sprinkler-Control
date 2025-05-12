#include "config.h"
const int EEPROM_NET_CONFIG_SIZE = 100;
const uint8_t SCHED_MAGIC = 0xA5;
const int EEPROM_SCHEDULE_SIZE = 2048;  // Size for schedule storage
const int EEPROM_SIZE = EEPROM_NET_CONFIG_SIZE + EEPROM_SCHEDULE_SIZE;

// WiFi Credentials now stored in mutable Strings

// Firmware Version
const char* FIRMWARE_VERSION = "3.2.2";

// Status LED Pin
const int statusLedPin = 23;

// Sprinkler Configuration
const int numSprinklers = 8;

// Relay Configuration (Active-Low for all)
const bool relayActiveLow[numSprinklers] = { 
    false, false, false, false, false, false, false, false 
};

// Sprinkler Pins
const int sprinklerPins[numSprinklers] = {13, 12, 14, 27, 26, 25, 33, 32}; // Zone 1 = Relay 1 (GPIO13), Zone 2 = Relay 2 (GPIO12), ..., Zone 8 = Relay 8 (GPIO32)

// Global Network Settings
IPAddress local_IP;
IPAddress gateway;
IPAddress subnet;
IPAddress primaryDNS;
IPAddress secondaryDNS;



