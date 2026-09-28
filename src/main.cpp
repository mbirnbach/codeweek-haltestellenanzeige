// CodeWeek Kassel – Haltestellenanzeige
//
// Participants: your files are in src/student/ (start with display_config.h).
// Maintainers: see README.md for the architecture.

#include <Arduino.h>

#include "app/AppController.h"

static AppController* app = nullptr;

void setup() {
    Serial.begin(115200);
    delay(300);  // let the Serial monitor attach
    app = new AppController(createDisplay());
    app->begin();
}

void loop() {
    app->loop();
    delay(5);
}
