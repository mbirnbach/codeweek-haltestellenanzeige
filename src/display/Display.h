#pragma once

#include "app/DisplayModel.h"

// Everything the application may ask a screen to do. Backends:
//   SerialDisplay – text on the Serial monitor (development, no hardware)
//   CYDDisplay    – TFT of the Cheap Yellow Display
// Application code never calls graphics functions directly.
class Display {
public:
    virtual ~Display() = default;

    virtual void begin() = 0;
    virtual void showBoot(const char* firmwareVersion) = 0;
    virtual void showSetup(const SetupScreen& setup) = 0;
    virtual void showStatus(const StatusScreen& status) = 0;  // connecting, loading, errors
    virtual void showDepartures(const DisplayModel& model) = 0;

    // Called every loop iteration (e.g. for touch polling).
    virtual void loop() {}

    // True once when the user asked for the setup portal via the display
    // (e.g. long touch). Backends without input return false.
    virtual bool setupGestureDetected() { return false; }
};

// Creates the backend selected by the PlatformIO environment.
Display& createDisplay();
