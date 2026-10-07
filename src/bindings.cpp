#include "bindings.h"

#include <LittleFS.h>
#include <strings.h>

#include <algorithm>

#include "config.h"
#include "hid.h"

namespace {

constexpr const char* kNames[Bindings::kSlots] = {"up", "down", "left", "right", "a", "b"};
constexpr uint8_t kMasks[Bindings::kSlots] = {1, 2, 4, 8, 16, 32};

constexpr const char* kLetters[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                                    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};
constexpr const char* kNumbers[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
constexpr const char* kFunction[] = {"f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",  "f8",  "f9",  "f10", "f11", "f12",
                                     "f13", "f14", "f15", "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23", "f24"};
constexpr const char* kNavigation[] = {"up", "down", "left", "right", "home", "end", "pageup", "pagedown", "insert", "delete"};
constexpr const char* kControl[] = {"space", "enter", "esc", "tab", "backspace", "capslock", "printscreen",
                                    "scrolllock", "pause", "menu", "mute", "volumeup", "volumedown"};
constexpr const char* kSymbols[] = {"grave",     "tilde",     "exclaim",  "at",          "hash",      "dollar",
                                    "percent",   "caret",     "ampersand", "asterisk",   "lparen",    "rparen",
                                    "minus",     "underscore", "equal",   "plus",        "lbracket",  "lbrace",
                                    "rbracket",  "rbrace",    "backslash", "pipe",       "semicolon", "colon",
                                    "quote",     "doublequote", "comma",  "lt",          "period",    "gt",
                                    "slash",     "question"};

struct Glyph {
    const char* key;
    const char* glyph;
};

constexpr Glyph kGlyphs[] = {
    {"grave", "`"},        {"tilde", "~"},         {"exclaim", "!"},     {"at", "@"},
    {"hash", "#"},         {"dollar", "$"},        {"percent", "%"},     {"caret", "^"},
    {"ampersand", "&"},    {"asterisk", "*"},      {"lparen", "("},      {"rparen", ")"},
    {"minus", "-"},        {"underscore", "_"},    {"equal", "="},       {"plus", "+"},
    {"lbracket", "["},     {"lbrace", "{"},        {"rbracket", "]"},    {"rbrace", "}"},
    {"backslash", "\\"},   {"pipe", "|"},          {"semicolon", ";"},   {"colon", ":"},
    {"quote", "'"},        {"doublequote", "\""},  {"comma", ","},       {"lt", "<"},
    {"period", "."},       {"gt", ">"},            {"slash", "/"},       {"question", "?"},
    {"space", "' '"},      {"kpplus", "+"},        {"kpminus", "-"},     {"kpasterisk", "*"},
    {"kpslash", "/"},      {"kpdot", "."},
};

constexpr const char* kNumpad[] = {"kp0", "kp1", "kp2", "kp3", "kp4", "kp5", "kp6", "kp7", "kp8", "kp9",
                                   "kpenter", "kpplus", "kpminus", "kpasterisk", "kpslash", "kpdot", "numlock"};

struct LegacyShift {
    const char* combo;
    const char* name;
};

constexpr LegacyShift kLegacyShift[] = {
    {"shift+1", "exclaim"},      {"shift+2", "at"},           {"shift+3", "hash"},        {"shift+4", "dollar"},
    {"shift+5", "percent"},      {"shift+6", "caret"},        {"shift+7", "ampersand"},   {"shift+8", "asterisk"},
    {"shift+9", "lparen"},       {"shift+0", "rparen"},       {"shift+minus", "underscore"}, {"shift+equal", "plus"},
    {"shift+lbracket", "lbrace"}, {"shift+rbracket", "rbrace"}, {"shift+backslash", "pipe"}, {"shift+semicolon", "colon"},
    {"shift+quote", "doublequote"}, {"shift+grave", "tilde"},  {"shift+comma", "lt"},       {"shift+period", "gt"},
    {"shift+slash", "question"},
};

struct KeyGroup {
    const char* name;
    const char* const* keys;
    int count;
};

#define KEY_GROUP(label, arr) {label, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0]))}
constexpr KeyGroup kGroups[] = {
    {"none", nullptr, 0},
    {"macro", nullptr, 0},
    KEY_GROUP("letters", kLetters),
    KEY_GROUP("numbers", kNumbers),
    KEY_GROUP("function", kFunction),
    KEY_GROUP("navigation", kNavigation),
    KEY_GROUP("control", kControl),
    KEY_GROUP("symbols", kSymbols),
    KEY_GROUP("numpad", kNumpad),
};
#undef KEY_GROUP
constexpr int kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);

constexpr const char* kModifiers[] = {
    "", "ctrl", "shift", "alt", "gui", "ctrl+shift", "ctrl+alt", "alt+shift", "gui+shift", "ctrl+alt+shift",
};
constexpr int kModifierCount = sizeof(kModifiers) / sizeof(kModifiers[0]);

bool isModifierToken(const String& token) {
    uint8_t code = 0;
    return hid::keyCodeFor(token.c_str(), code) && code >= 0xE0 && code <= 0xE7;
}
constexpr const char* kBindsPath = "/binds.json";
constexpr const char* kPresetDir = "/presets";

String presetPath(const String& name) {
    return String(kPresetDir) + "/" + name + ".json";
}

bool validPresetName(const String& name) {
    if (name.length() == 0 || name.length() > 32) return false;
    if (name[0] == '.' || name[0] == ' ' || name[name.length() - 1] == ' ') return false;
    for (size_t i = 0; i < name.length(); ++i) {
        const char c = name[i];
        if (!(isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '_' || c == '-' || c == '.' || c == '(' || c == ')')) return false;
    }
    return true;
}
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
        case BindBehavior::ToggleBurst: return "toggleburst";
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
    if (strcasecmp(name, "toggleburst") == 0 || strcasecmp(name, "toggle_burst") == 0 || strcasecmp(name, "toggle-burst") == 0) {
        out = BindBehavior::ToggleBurst;
        return true;
    }
    return false;
}

int Bindings::groupCount() {
    return kGroupCount;
}

const char* Bindings::groupName(int group) {
    return (group >= 0 && group < kGroupCount) ? kGroups[group].name : "";
}

int Bindings::groupKeyCount(int group) {
    return (group >= 0 && group < kGroupCount) ? kGroups[group].count : 0;
}

const char* Bindings::groupKey(int group, int index) {
    if (group < 0 || group >= kGroupCount) return "";
    const KeyGroup& g = kGroups[group];
    return (index >= 0 && index < g.count) ? g.keys[index] : "";
}

int Bindings::modifierCount() {
    return kModifierCount;
}

const char* Bindings::modifierName(int index) {
    return (index >= 0 && index < kModifierCount) ? kModifiers[index] : "";
}

void Bindings::split(const String& keys, int& group, int& keyIndex, int& modIndex) {
    group = 0;
    keyIndex = 0;
    modIndex = 0;
    for (const LegacyShift& legacy : kLegacyShift) {
        if (!keys.equalsIgnoreCase(legacy.combo)) continue;
        for (int g = 1; g < kGroupCount; ++g) {
            for (int i = 0; i < kGroups[g].count; ++i) {
                if (strcmp(kGroups[g].keys[i], legacy.name) == 0) {
                    group = g;
                    keyIndex = i;
                    return;
                }
            }
        }
    }
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
    for (int g = 1; g < kGroupCount && group == 0; ++g) {
        for (int i = 0; i < kGroups[g].count; ++i) {
            if (mainKey == kGroups[g].keys[i]) {
                group = g;
                keyIndex = i;
                break;
            }
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

String Bindings::keyPreview(const char* key) {
    if (key == nullptr) return String();
    for (const Glyph& g : kGlyphs) {
        if (strcmp(g.key, key) == 0) return String(g.glyph);
    }
    return String();
}

String Bindings::compose(int group, int keyIndex, int modIndex) {
    String mods(modifierName(modIndex));
    String key(groupKey(group, keyIndex));
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

bool Bindings::setSlotMacro(int slot, const String& script) {
    if (slot < 0 || slot >= kSlots || script.length() == 0) return false;
    Binding next;
    next.script = script;
    next.behavior = BindBehavior::Toggle;
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

std::vector<String> Bindings::presetNames() {
    std::vector<String> out;
    File dir = LittleFS.open(kPresetDir);
    if (!dir || !dir.isDirectory()) return out;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (f.isDirectory()) continue;
        String name = f.name();
        const int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        if (!name.endsWith(".json")) continue;
        name.remove(name.length() - 5);
        out.push_back(name);
    }
    std::sort(out.begin(), out.end(), [](const String& a, const String& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
    return out;
}

bool Bindings::presetExists(const String& name) {
    return validPresetName(name) && LittleFS.exists(presetPath(name));
}

bool Bindings::savePreset(const String& name) const {
    if (!validPresetName(name)) return false;
    if (!LittleFS.exists(kPresetDir)) LittleFS.mkdir(kPresetDir);
    JsonDocument doc;
    toJson(doc.to<JsonArray>());
    File f = LittleFS.open(presetPath(name), FILE_WRITE);
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool Bindings::loadPreset(const String& name) {
    if (!presetExists(name)) return false;
    File f = LittleFS.open(presetPath(name), FILE_READ);
    if (!f) return false;
    JsonDocument doc;
    const bool ok = deserializeJson(doc, f) == DeserializationError::Ok && doc.is<JsonArray>();
    f.close();
    if (!ok) return false;
    String err;
    if (!applyJson(doc.as<JsonArrayConst>(), err)) return false;
    return save();
}

bool Bindings::deletePreset(const String& name) {
    return presetExists(name) && LittleFS.remove(presetPath(name));
}

bool Bindings::renamePreset(const String& from, const String& to) {
    if (!presetExists(from) || !validPresetName(to) || LittleFS.exists(presetPath(to))) return false;
    return LittleFS.rename(presetPath(from), presetPath(to));
}

bool Bindings::readPreset(const String& name, JsonDocument& out) {
    Bindings temp;
    if (!presetExists(name)) return false;
    File f = LittleFS.open(presetPath(name), FILE_READ);
    if (!f) return false;
    JsonDocument doc;
    const bool ok = deserializeJson(doc, f) == DeserializationError::Ok && doc.is<JsonArray>();
    f.close();
    if (!ok) return false;
    String err;
    temp.applyJson(doc.as<JsonArrayConst>(), err);
    temp.toJson(out["binds"].to<JsonArray>());
    return true;
}

bool Bindings::writePreset(const String& name, JsonArrayConst list, String& err) {
    if (!validPresetName(name)) {
        err = "Name must be 1-32 characters: letters, digits, space, _ - . ( )";
        return false;
    }
    Bindings temp;
    if (!temp.applyJson(list, err)) return false;
    if (!temp.savePreset(name)) {
        err = "Could not write preset to device storage";
        return false;
    }
    return true;
}

String Bindings::freePresetName() {
    for (int i = 1; i < 100; ++i) {
        String name = "Preset " + String(i);
        if (!LittleFS.exists(presetPath(name))) return name;
    }
    return "Preset 99";
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
        b.script = item["script"] | "";
        b.script.trim();
        const char* modeName = item["mode"] | "normal";
        if (strcasecmp(modeName, "macro") == 0) {
            if (b.script.length() == 0) {
                err = "Button " + labelFor(slot) + ": pick a macro";
                return false;
            }
            b.keys = "";
            b.behavior = BindBehavior::Toggle;
            continue;
        }
        b.script = "";
        if (!behaviorFromName(modeName, b.behavior)) {
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
        item["script"] = _slots[i].script;
        item["mode"] = _slots[i].script.length() ? "macro" : behaviorName(_slots[i].behavior);
        item["interval"] = _slots[i].intervalMs;
    }
}

void Bindings::activate() {
    _pressed = 0;
    _toggled = 0;
    _bursting = 0;
    _fresh = true;
    _changed = true;
    hid::releaseAll();
}

void Bindings::deactivate() {
    _pressed = 0;
    _toggled = 0;
    _bursting = 0;
    _fresh = true;
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

void Bindings::update(uint8_t pressedMask, uint32_t now, MacroHost& host) {
    if (_fresh) {
        _fresh = false;
        _pressed = pressedMask;
        return;
    }
    const uint8_t down = static_cast<uint8_t>(pressedMask & ~_pressed);
    const uint8_t up = static_cast<uint8_t>(_pressed & ~pressedMask);
    _pressed = pressedMask;
    _bursting = static_cast<uint8_t>((_bursting | down) & pressedMask);
    if (down || up) _changed = true;

    for (int i = 0; i < kSlots; ++i) {
        const uint8_t bit = kMasks[i];
        const Binding& b = _slots[i];
        if (b.script.length()) {
            if (down & bit) {
                if (host.isRunning(b.script)) host.stop();
                else host.run(b.script);
            }
            const bool on = host.isRunning(b.script);
            const uint8_t next = on ? static_cast<uint8_t>(_toggled | bit) : static_cast<uint8_t>(_toggled & ~bit);
            if (next != _toggled) {
                _toggled = next;
                _changed = true;
            }
            continue;
        }
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
                } else if (held && (_bursting & bit) && static_cast<int32_t>(now - _nextBurst[i]) >= 0) {
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
            case BindBehavior::ToggleBurst:
                if (pressedNow) {
                    if (_toggled & bit) {
                        _toggled = static_cast<uint8_t>(_toggled & ~bit);
                    } else {
                        _toggled = static_cast<uint8_t>(_toggled | bit);
                        tapSlot(i);
                        _nextBurst[i] = now + b.intervalMs;
                    }
                } else if ((_toggled & bit) && static_cast<int32_t>(now - _nextBurst[i]) >= 0) {
                    tapSlot(i);
                    _nextBurst[i] = now + b.intervalMs;
                }
                break;
        }
    }
}
