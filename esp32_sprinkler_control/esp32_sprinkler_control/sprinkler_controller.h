#ifndef SPRINKLER_CONTROLLER_H
#define SPRINKLER_CONTROLLER_H

#include <Arduino.h>

class SprinklerController {
public:
    // Constructor takes arrays for pin mapping and active logic
    SprinklerController(const int* pins, const bool* activeLow, int count, int statusLedPin);
    void begin();
    void setRelay(int zone, bool on);
    void setAllOff();
    void setStatusLed(bool on);
    int getZoneCount() const;
    int getPin(int zone) const;
    bool isActiveLow(int zone) const;
private:
    const int* _pins;
    const bool* _activeLow;
    int _count;
    int _statusLedPin;
};

#endif // SPRINKLER_CONTROLLER_H
