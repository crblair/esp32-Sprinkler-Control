// network_state.h
// Safe introduction: empty class for future network encapsulation
#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H
#include <Arduino.h>

class NetworkState {
public:
    String ssid;
    String password;

    NetworkState() : ssid(""), password("") {}
};

#endif // NETWORK_STATE_H
