#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace serial_api {

void begin();
bool nextLine(String& out);
void send(JsonDocument& doc);
bool hostConnected();

}
