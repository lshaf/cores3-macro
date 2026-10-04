#include <Arduino.h>

#include "app.h"

namespace {
App* app = nullptr;
}

void setup() {
    app = new App();
    app->setup();
}

void loop() {
    app->loop();
}
