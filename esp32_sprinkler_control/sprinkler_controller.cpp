#include "sprinkler_controller.h"

SprinklerController::SprinklerController(const int* pins, const bool* activeLow, int count, int statusLedPin)
    : _pins(pins), _activeLow(activeLow), _count(count), _statusLedPin(statusLedPin) {}

void SprinklerController::begin() {
    for (int i = 0; i < _count; ++i) {
        pinMode(_pins[i], OUTPUT);
        // Default all relays OFF
        setRelay(i, false);
    }
    pinMode(_statusLedPin, OUTPUT);
    setStatusLed(false);
}

void SprinklerController::setRelay(int zone, bool on) {
    if (zone < 0 || zone >= _count) return;
    digitalWrite(_pins[zone], _activeLow[zone] ? !on : on);
}

void SprinklerController::setAllOff() {
    for (int i = 0; i < _count; ++i) {
        setRelay(i, false);
    }
}

void SprinklerController::setStatusLed(bool on) {
    digitalWrite(_statusLedPin, on ? LOW : HIGH); // Assuming active-low LED
}

int SprinklerController::getZoneCount() const {
    return _count;
}

int SprinklerController::getPin(int zone) const {
    if (zone < 0 || zone >= _count) return -1;
    return _pins[zone];
}

bool SprinklerController::isActiveLow(int zone) const {
    if (zone < 0 || zone >= _count) return false;
    return _activeLow[zone];
}
