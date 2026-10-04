#pragma once

#include <M5Faces.h>
#include <stdint.h>

enum PadButton : uint8_t {
    PAD_UP = 1 << 0,
    PAD_DOWN = 1 << 1,
    PAD_LEFT = 1 << 2,
    PAD_RIGHT = 1 << 3,
    PAD_A = 1 << 4,
    PAD_B = 1 << 5,
    PAD_SELECT = 1 << 6,
    PAD_START = 1 << 7,
};

class Gamepad {
public:
    void begin();
    void update();
    bool available() const { return _available; }
    bool pressed(uint8_t mask) const { return (_pressed & mask) != 0; }
    bool fired(uint8_t mask) const { return (_fired & mask) != 0; }
    bool anyFired() const { return _fired != 0; }
    uint8_t pressedMask() const { return _pressed; }

private:
    bool probe();
    bool readRaw(uint8_t& raw);

    M5Faces_Gamepad3 _dev;
    bool _available = false;
    bool _rawMode = false;
    uint8_t _pressed = 0;
    uint8_t _fired = 0;
    uint8_t _repeating = 0;
    uint32_t _lastPoll = 0;
    uint32_t _lastProbe = 0;
    uint32_t _settleUntil = 0;
    uint32_t _holdSince = 0;
    uint32_t _lastRepeat = 0;
};
