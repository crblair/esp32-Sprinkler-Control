// network_state.h
// Safe introduction: empty class for future network encapsulation
#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H
#include <Arduino.h>

class NetworkState {
public:
    // Default credentials (can be changed for deployment)
    static const char* DEFAULT_SSID;
    static const char* DEFAULT_PASSWORD;

    String ssid;
    String password;

    // Constructor initializes with defaults
    NetworkState() : ssid(DEFAULT_SSID), password(DEFAULT_PASSWORD) {}

    // Reset credentials to defaults
    void resetToDefaults() {
        ssid = DEFAULT_SSID;
        password = DEFAULT_PASSWORD;
    }
};


#endif // NETWORK_STATE_H
