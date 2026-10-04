#include "gamepad.h"

#include <M5Unified.h>

#include "config.h"

void Gamepad::begin() {
    _available = probe();
    _lastProbe = millis();
}

bool Gamepad::probe() {
    if (!M5.In_I2C.scanID(cfg::kFacesAddr, cfg::kFacesI2cFreq)) return false;
    m5faces_err_t err = _dev.begin(&M5.In_I2C, cfg::kFacesAddr, cfg::kFacesI2cFreq);
    _rawMode = (err != M5FACES_OK);
    _pressed = 0;
    _repeating = 0;
    _settleUntil = millis() + cfg::kGamepadSettleMs;
    return true;
}

bool Gamepad::readRaw(uint8_t& raw) {
    if (!_rawMode) {
        _dev.update();
        raw = _dev.getState().raw;
        return true;
    }
    uint8_t value = 0xFF;
    if (!M5.In_I2C.readRegister(cfg::kFacesAddr, M5FACES_REG_KEY, &value, 1, cfg::kFacesI2cFreq)) return false;
    if (value == 0x00) {
        raw = static_cast<uint8_t>(~_pressed);
        return true;
    }
    raw = value;
    return true;
}

void Gamepad::update() {
    const uint32_t now = millis();
    _fired = 0;

    if (now - _lastProbe >= cfg::kGamepadProbeMs) {
        _lastProbe = now;
        const bool present = M5.In_I2C.scanID(cfg::kFacesAddr, cfg::kFacesI2cFreq);
        if (present && !_available) _available = probe();
        if (!present && _available) {
            _available = false;
            _pressed = 0;
            _repeating = 0;
        }
    }
    if (!_available) return;
    if (now - _lastPoll < cfg::kGamepadPollMs) return;
    _lastPoll = now;

    uint8_t raw = 0xFF;
    if (!readRaw(raw)) return;

    const uint8_t pressed = static_cast<uint8_t>(~raw);
    const uint8_t edges = static_cast<uint8_t>(pressed & ~_pressed);
    _pressed = pressed;
    if (static_cast<int32_t>(now - _settleUntil) < 0) {
        _repeating = 0;
        return;
    }
    _fired = edges;

    const uint8_t repeatable = pressed & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT);
    if (edges & repeatable) {
        _holdSince = now;
        _lastRepeat = now;
        _repeating = edges & repeatable;
    } else if (repeatable && (_repeating & repeatable)) {
        if (now - _holdSince >= cfg::kRepeatDelayMs && now - _lastRepeat >= cfg::kRepeatRateMs) {
            _lastRepeat = now;
            _fired |= _repeating & repeatable;
        }
    } else {
        _repeating = 0;
    }
}
