#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace hid {

enum MouseButton : uint8_t {
    MOUSE_BTN_LEFT = 1,
    MOUSE_BTN_RIGHT = 2,
    MOUSE_BTN_MIDDLE = 4,
};

void begin();
bool mounted();
bool keyCodeFor(const char* name, uint8_t& code);
bool consumerCodeFor(const char* name, uint16_t& usage);
bool mouseButtonFor(const char* name, uint8_t& button);
bool parseCombo(const String& text, uint8_t* codes, uint8_t maxCodes, uint8_t& count, String& err);
void pressKey(uint8_t code);
void releaseKey(uint8_t code);
void releaseAll();
void typeChar(char c);
void mouseMove(int dx, int dy);
void mouseScroll(int amount);
void mousePress(uint8_t button);
void mouseRelease(uint8_t button);
void mouseClick(uint8_t button);
void consumerTap(uint16_t usage);

}
