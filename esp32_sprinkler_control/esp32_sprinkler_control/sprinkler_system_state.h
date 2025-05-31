#ifndef SPRINKLER_SYSTEM_STATE_H
#define SPRINKLER_SYSTEM_STATE_H

class SprinklerSystemState {
    bool manualMode = false;
public:
    bool isManualMode() const { return manualMode; }
    void setManualMode(bool mode) { manualMode = mode; }
};

#endif // SPRINKLER_SYSTEM_STATE_H
