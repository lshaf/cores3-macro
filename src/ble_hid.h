#pragma once

#include <stdint.h>

namespace ble_hid {

void begin(const char* name);
void end();
bool active();
bool connected();
void pressKey(uint8_t code);
void releaseKey(uint8_t code);
void releaseAll();
void typeChar(char c);
void mouseMove(int8_t dx, int8_t dy, int8_t wheel);
void mousePress(uint8_t button);
void mouseRelease(uint8_t button);
void consumerTap(uint16_t usage);

}
