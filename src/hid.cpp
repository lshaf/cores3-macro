#include "hid.h"

#include <Arduino.h>
#include <USB.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <strings.h>
#include <tusb.h>

namespace {

USBHIDKeyboard keyboard;
USBHIDMouse mouse;
USBHIDConsumerControl consumer;

struct NamedCode {
    const char* name;
    uint16_t code;
};

constexpr NamedCode kKeys[] = {
    {"ctrl", 0xE0},        {"control", 0xE0},     {"lctrl", 0xE0},       {"rctrl", 0xE4},
    {"shift", 0xE1},       {"lshift", 0xE1},      {"rshift", 0xE5},      {"alt", 0xE2},
    {"option", 0xE2},      {"lalt", 0xE2},        {"ralt", 0xE6},        {"altgr", 0xE6},
    {"gui", 0xE3},         {"win", 0xE3},         {"windows", 0xE3},     {"cmd", 0xE3},
    {"command", 0xE3},     {"meta", 0xE3},        {"super", 0xE3},       {"rgui", 0xE7},
    {"enter", 0x28},       {"return", 0x28},      {"esc", 0x29},         {"escape", 0x29},
    {"backspace", 0x2A},   {"bksp", 0x2A},        {"tab", 0x2B},         {"space", 0x2C},
    {"minus", 0x2D},       {"equal", 0x2E},       {"equals", 0x2E},      {"lbracket", 0x2F},
    {"rbracket", 0x30},    {"backslash", 0x31},   {"semicolon", 0x33},   {"quote", 0x34},
    {"grave", 0x35},       {"tilde", 0x35},       {"comma", 0x36},       {"period", 0x37},
    {"dot", 0x37},         {"slash", 0x38},       {"capslock", 0x39},    {"caps", 0x39},
    {"f1", 0x3A},          {"f2", 0x3B},          {"f3", 0x3C},          {"f4", 0x3D},
    {"f5", 0x3E},          {"f6", 0x3F},          {"f7", 0x40},          {"f8", 0x41},
    {"f9", 0x42},          {"f10", 0x43},         {"f11", 0x44},         {"f12", 0x45},
    {"f13", 0x68},         {"f14", 0x69},         {"f15", 0x6A},         {"f16", 0x6B},
    {"f17", 0x6C},         {"f18", 0x6D},         {"f19", 0x6E},         {"f20", 0x6F},
    {"f21", 0x70},         {"f22", 0x71},         {"f23", 0x72},         {"f24", 0x73},
    {"printscreen", 0x46}, {"prtsc", 0x46},       {"sysrq", 0x46},       {"scrolllock", 0x47},
    {"pause", 0x48},       {"break", 0x48},       {"insert", 0x49},      {"ins", 0x49},
    {"home", 0x4A},        {"pageup", 0x4B},      {"pgup", 0x4B},        {"delete", 0x4C},
    {"del", 0x4C},         {"end", 0x4D},         {"pagedown", 0x4E},    {"pgdn", 0x4E},
    {"right", 0x4F},       {"left", 0x50},        {"down", 0x51},        {"up", 0x52},
    {"rightarrow", 0x4F},  {"leftarrow", 0x50},   {"downarrow", 0x51},   {"uparrow", 0x52},
    {"arrowright", 0x4F},  {"arrowleft", 0x50},   {"arrowdown", 0x51},   {"arrowup", 0x52},
    {"numlock", 0x53},     {"kpslash", 0x54},     {"kpasterisk", 0x55},  {"kpminus", 0x56},
    {"kpplus", 0x57},      {"kpenter", 0x58},     {"kp1", 0x59},         {"kp2", 0x5A},
    {"kp3", 0x5B},         {"kp4", 0x5C},         {"kp5", 0x5D},         {"kp6", 0x5E},
    {"kp7", 0x5F},         {"kp8", 0x60},         {"kp9", 0x61},         {"kp0", 0x62},
    {"kpdot", 0x63},       {"menu", 0x65},        {"app", 0x65},         {"application", 0x65},
    {"power", 0x66},       {"mute", 0x7F},        {"volumeup", 0x80},    {"volumedown", 0x81},
};

constexpr NamedCode kConsumer[] = {
    {"play", 0xB0},          {"pause", 0xB1},          {"playpause", 0xCD},     {"play_pause", 0xCD},
    {"next", 0xB5},          {"prev", 0xB6},           {"previous", 0xB6},      {"stop", 0xB7},
    {"mute", 0xE2},          {"volup", 0xE9},          {"volumeup", 0xE9},      {"voldown", 0xEA},
    {"volumedown", 0xEA},    {"brightnessup", 0x6F},   {"brightnessdown", 0x70}, {"eject", 0xB8},
    {"calculator", 0x192},   {"email", 0x18A},         {"browser", 0x196},      {"home", 0x223},
    {"back", 0x224},         {"forward", 0x225},       {"refresh", 0x227},      {"search", 0x221},
    {"sleep", 0x32},
};

bool charCode(char c, uint8_t& code) {
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (c >= 'a' && c <= 'z') {
        code = static_cast<uint8_t>(0x04 + (c - 'a'));
        return true;
    }
    if (c >= '1' && c <= '9') {
        code = static_cast<uint8_t>(0x1E + (c - '1'));
        return true;
    }
    switch (c) {
        case '0': code = 0x27; return true;
        case '-': code = 0x2D; return true;
        case '=': code = 0x2E; return true;
        case '[': code = 0x2F; return true;
        case ']': code = 0x30; return true;
        case '\\': code = 0x31; return true;
        case ';': code = 0x33; return true;
        case '\'': code = 0x34; return true;
        case '`': code = 0x35; return true;
        case ',': code = 0x36; return true;
        case '.': code = 0x37; return true;
        case '/': code = 0x38; return true;
        default: return false;
    }
}

template <size_t N>
bool lookup(const NamedCode (&table)[N], const char* name, uint16_t& code) {
    for (const NamedCode& entry : table) {
        if (strcasecmp(entry.name, name) == 0) {
            code = entry.code;
            return true;
        }
    }
    return false;
}

int8_t clampStep(int value) {
    if (value > 127) return 127;
    if (value < -127) return -127;
    return static_cast<int8_t>(value);
}

}

void hid::begin() {
    keyboard.begin();
    mouse.begin();
    consumer.begin();
    USB.begin();
}

bool hid::mounted() {
    return tud_mounted();
}

bool hid::keyCodeFor(const char* name, uint8_t& code) {
    if (name == nullptr || name[0] == '\0') return false;
    if (name[1] == '\0') return charCode(name[0], code);
    uint16_t found = 0;
    if (!lookup(kKeys, name, found)) return false;
    code = static_cast<uint8_t>(found);
    return true;
}

bool hid::consumerCodeFor(const char* name, uint16_t& usage) {
    return name != nullptr && lookup(kConsumer, name, usage);
}

bool hid::mouseButtonFor(const char* name, uint8_t& button) {
    if (name == nullptr) return false;
    if (strcasecmp(name, "left") == 0 || strcasecmp(name, "l") == 0 || strcmp(name, "1") == 0) {
        button = MOUSE_BTN_LEFT;
        return true;
    }
    if (strcasecmp(name, "right") == 0 || strcasecmp(name, "r") == 0 || strcmp(name, "2") == 0) {
        button = MOUSE_BTN_RIGHT;
        return true;
    }
    if (strcasecmp(name, "middle") == 0 || strcasecmp(name, "m") == 0 || strcmp(name, "3") == 0) {
        button = MOUSE_BTN_MIDDLE;
        return true;
    }
    return false;
}

bool hid::parseCombo(const String& text, uint8_t* codes, uint8_t maxCodes, uint8_t& count, String& err) {
    count = 0;
    String token;
    auto flush = [&]() -> bool {
        if (token.length() == 0) return true;
        if (count >= maxCodes) {
            err = "Too many keys in one combo (max " + String(maxCodes) + ")";
            return false;
        }
        uint8_t code = 0;
        if (!keyCodeFor(token.c_str(), code)) {
            err = "Unknown key: " + token;
            return false;
        }
        codes[count++] = code;
        token = "";
        return true;
    };
    for (size_t i = 0; i < text.length(); ++i) {
        const char c = text[i];
        if (c == '+' || c == ' ' || c == '\t' || c == ',') {
            if (!flush()) return false;
        } else {
            token += c;
        }
    }
    if (!flush()) return false;
    if (count == 0) {
        err = "No key given";
        return false;
    }
    return true;
}

void hid::pressKey(uint8_t code) {
    keyboard.pressRaw(code);
}

void hid::releaseKey(uint8_t code) {
    keyboard.releaseRaw(code);
}

void hid::releaseAll() {
    keyboard.releaseAll();
    mouse.release(MOUSE_BTN_LEFT | MOUSE_BTN_RIGHT | MOUSE_BTN_MIDDLE);
    consumer.release();
}

void hid::typeChar(char c) {
    keyboard.write(static_cast<uint8_t>(c));
}

void hid::mouseMove(int dx, int dy) {
    while (dx != 0 || dy != 0) {
        const int8_t sx = clampStep(dx);
        const int8_t sy = clampStep(dy);
        mouse.move(sx, sy, 0, 0);
        dx -= sx;
        dy -= sy;
    }
}

void hid::mouseScroll(int amount) {
    while (amount != 0) {
        const int8_t step = clampStep(amount);
        mouse.move(0, 0, step, 0);
        amount -= step;
    }
}

void hid::mousePress(uint8_t button) {
    mouse.press(button);
}

void hid::mouseRelease(uint8_t button) {
    mouse.release(button);
}

void hid::mouseClick(uint8_t button) {
    mouse.click(button);
}

void hid::consumerTap(uint16_t usage) {
    consumer.press(usage);
    delay(10);
    consumer.release();
}
