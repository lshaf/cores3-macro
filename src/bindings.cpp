#include "bindings.h"

#include <LittleFS.h>
#include <strings.h>

#include "config.h"
#include "hid.h"

namespace {

constexpr const char* kNames[Bindings::kSlots] = {"up", "down", "left", "right", "a", "b"};
constexpr uint8_t kMasks[Bindings::kSlots] = {1, 2, 4, 8, 16, 32};

constexpr const char* kPickerKeys[] = {
    "",       "a",     "b",      "c",       "d",        "e",        "f",         "g",         "h",       "i",
    "j",      "k",     "l",      "m",       "n",        "o",        "p",         "q",         "r",       "s",
    "t",      "u",     "v",      "w",       "x",        "y",        "z",         "0",         "1",       "2",
    "3",      "4",     "5",      "6",       "7",        "8",        "9",         "space",     "enter",   "esc",
    "tab",    "backspace", "delete", "insert", "home",   "end",      "pageup",    "pagedown",  "up",      "down",
    "left",   "right", "f1",     "f2",      "f3",       "f4",       "f5",        "f6",        "f7",      "f8",
    "f9",     "f10",   "f11",    "f12",     "minus",    "equal",    "lbracket",  "rbracket",  "backslash", "semicolon",
    "quote",  "grave", "comma",  "period",  "slash",    "capslock", "printscreen", "menu",    "kp0",     "kp1",
    "kp2",    "kp3",   "kp4",    "kp5",     "kp6",      "kp7",      "kp8",       "kp9",       "kpenter", "kpplus",
    "kpminus",
};
constexpr int kPickerKeyCount = sizeof(kPickerKeys) / sizeof(kPickerKeys[0]);

constexpr const char* kModifiers[] = {
    "", "ctrl", "shift", "alt", "gui", "ctrl+shift", "ctrl+alt", "alt+shift", "gui+shift", "ctrl+alt+shift",
};
constexpr int kModifierCount = sizeof(kModifiers) / sizeof(kModifiers[0]);

bool isModifierToken(const String& token) {
    uint8_t code = 0;
    return hid::keyCodeFor(token.c_str(), code) && code >= 0xE0 && code <= 0xE7;
}
constexpr const char* kBindsPath = "/binds.json";
constexpr int kMinInterval = 20;
constexpr int kMaxInterval = 5000;

String labelFor(int slot) {
    String label(kNames[slot]);
    label.toUpperCase();
    return label;
}

}

const char* Bindings::buttonName(int slot) {
    return (slot >= 0 && slot < kSlots) ? kNames[slot] : "";
}

uint8_t Bindings::buttonMask(int slot) {
    return (slot >= 0 && slot < kSlots) ? kMasks[slot] : 0;
}

int Bindings::slotForName(const char* name) {
    if (name == nullptr) return -1;
    for (int i = 0; i < kSlots; ++i) {
        if (strcasecmp(kNames[i], name) == 0) return i;
    }
    return -1;
}

const char* Bindings::behaviorName(BindBehavior behavior) {
    switch (behavior) {
        case BindBehavior::Burst: return "burst";
        case BindBehavior::Toggle: return "toggle";
        case BindBehavior::Normal:
        default: return "normal";
    }
}

bool Bindings::behaviorFromName(const char* name, BindBehavior& out) {
    if (name == nullptr) return false;
    if (strcasecmp(name, "normal") == 0 || strcasecmp(name, "hold") == 0) {
        out = BindBehavior::Normal;
        return true;
    }
    if (strcasecmp(name, "burst") == 0) {
        out = BindBehavior::Burst;
        return true;
    }
    if (strcasecmp(name, "toggle") == 0) {
        out = BindBehavior::Toggle;
        return true;
    }
    return false;
}

int Bindings::pickerKeyCount() {
    return kPickerKeyCount;
}

const char* Bindings::pickerKey(int index) {
    return (index >= 0 && index < kPickerKeyCount) ? kPickerKeys[index] : "";
}

int Bindings::modifierCount() {
    return kModifierCount;
}

const char* Bindings::modifierName(int index) {
    return (index >= 0 && index < kModifierCount) ? kModifiers[index] : "";
}

void Bindings::split(const String& keys, int& keyIndex, int& modIndex) {
    keyIndex = 0;
    modIndex = 0;
    String mods;
    String mainKey;
    String token;
    auto flush = [&]() {
        if (token.length() == 0) return;
        token.toLowerCase();
        if (isModifierToken(token)) {
            if (mods.length()) mods += "+";
            mods += token;
        } else {
            mainKey = token;
        }
        token = "";
    };
    for (size_t i = 0; i < keys.length(); ++i) {
        const char c = keys[i];
        if (c == '+' || c == ' ' || c == ',') flush();
        else token += c;
    }
    flush();
    for (int i = 0; i < kPickerKeyCount; ++i) {
        if (mainKey == kPickerKeys[i]) {
            keyIndex = i;
            break;
        }
    }
    String normalized = mods;
    normalized.replace("control", "ctrl");
    normalized.replace("win", "gui");
    normalized.replace("cmd", "gui");
    for (int i = 0; i < kModifierCount; ++i) {
        if (normalized == kModifiers[i]) {
            modIndex = i;
            break;
        }
    }
}

String Bindings::compose(int keyIndex, int modIndex) {
    String mods(modifierName(modIndex));
    String key(pickerKey(keyIndex));
    if (mods.length() && key.length()) return mods + "+" + key;
    return mods.length() ? mods : key;
}

bool Bindings::setSlot(int slot, const String& keys, BindBehavior behavior, uint16_t intervalMs, String& err) {
    if (slot < 0 || slot >= kSlots) {
        err = "Unknown button";
        return false;
    }
    Binding next;
    next.keys = keys;
    next.behavior = behavior;
    if (intervalMs < kMinInterval) intervalMs = kMinInterval;
    if (intervalMs > kMaxInterval) intervalMs = kMaxInterval;
    next.intervalMs = intervalMs;
    if (!compile(next, err)) return false;
    _slots[slot] = next;
    return true;
}

bool Bindings::compile(Binding& binding, String& err) {
    binding.codeCount = 0;
    binding.keys.trim();
    if (binding.keys.length() == 0) return true;
    return hid::parseCombo(binding.keys, binding.codes, sizeof(binding.codes), binding.codeCount, err);
}

void Bindings::load() {
    for (Binding& b : _slots) b = Binding();
    File f = LittleFS.open(kBindsPath, FILE_READ);
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok && doc.is<JsonArray>()) {
        String err;
        applyJson(doc.as<JsonArrayConst>(), err);
    }
    f.close();
}

bool Bindings::save() const {
    JsonDocument doc;
    toJson(doc.to<JsonArray>());
    File f = LittleFS.open(kBindsPath, FILE_WRITE);
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool Bindings::applyJson(JsonArrayConst list, String& err) {
    Binding next[kSlots];
    for (JsonObjectConst item : list) {
        const int slot = slotForName(item["button"] | "");
        if (slot < 0) continue;
        Binding& b = next[slot];
        b.keys = item["keys"] | "";
        if (!behaviorFromName(item["mode"] | "normal", b.behavior)) {
            err = "Button " + labelFor(slot) + ": mode must be normal, burst or toggle";
            return false;
        }
        int interval = item["interval"] | 100;
        if (interval < kMinInterval) interval = kMinInterval;
        if (interval > kMaxInterval) interval = kMaxInterval;
        b.intervalMs = static_cast<uint16_t>(interval);
        String keyErr;
        if (!compile(b, keyErr)) {
            err = "Button " + labelFor(slot) + ": " + keyErr;
            return false;
        }
    }
    for (int i = 0; i < kSlots; ++i) _slots[i] = next[i];
    return true;
}

void Bindings::toJson(JsonArray out) const {
    for (int i = 0; i < kSlots; ++i) {
        JsonObject item = out.add<JsonObject>();
        item["button"] = kNames[i];
        item["keys"] = _slots[i].keys;
        item["mode"] = behaviorName(_slots[i].behavior);
        item["interval"] = _slots[i].intervalMs;
    }
}

void Bindings::activate() {
    _pressed = 0;
    _toggled = 0;
    _changed = true;
    hid::releaseAll();
}

void Bindings::deactivate() {
    _pressed = 0;
    _toggled = 0;
    _changed = true;
    hid::releaseAll();
}

bool Bindings::takeChanged() {
    const bool was = _changed;
    _changed = false;
    return was;
}

void Bindings::pressSlot(int index) {
    const Binding& b = _slots[index];
    for (uint8_t i = 0; i < b.codeCount; ++i) hid::pressKey(b.codes[i]);
}

void Bindings::releaseSlot(int index) {
    const Binding& b = _slots[index];
    for (uint8_t i = b.codeCount; i-- > 0;) hid::releaseKey(b.codes[i]);
}

void Bindings::tapSlot(int index) {
    pressSlot(index);
    delay(cfg::kKeyTapHoldMs);
    releaseSlot(index);
}

void Bindings::update(uint8_t pressedMask, uint32_t now) {
    const uint8_t down = static_cast<uint8_t>(pressedMask & ~_pressed);
    const uint8_t up = static_cast<uint8_t>(_pressed & ~pressedMask);
    _pressed = pressedMask;
    if (down || up) _changed = true;

    for (int i = 0; i < kSlots; ++i) {
        const uint8_t bit = kMasks[i];
        const Binding& b = _slots[i];
        if (b.codeCount == 0) continue;
        const bool pressedNow = (down & bit) != 0;
        const bool releasedNow = (up & bit) != 0;
        const bool held = (pressedMask & bit) != 0;

        switch (b.behavior) {
            case BindBehavior::Normal:
                if (pressedNow) pressSlot(i);
                if (releasedNow) releaseSlot(i);
                break;
            case BindBehavior::Burst:
                if (pressedNow) {
                    tapSlot(i);
                    _nextBurst[i] = now + b.intervalMs;
                } else if (held && static_cast<int32_t>(now - _nextBurst[i]) >= 0) {
                    tapSlot(i);
                    _nextBurst[i] = now + b.intervalMs;
                }
                break;
            case BindBehavior::Toggle:
                if (pressedNow) {
                    if (_toggled & bit) {
                        releaseSlot(i);
                        _toggled = static_cast<uint8_t>(_toggled & ~bit);
                    } else {
                        pressSlot(i);
                        _toggled = static_cast<uint8_t>(_toggled | bit);
                    }
                }
                break;
        }
    }
}
