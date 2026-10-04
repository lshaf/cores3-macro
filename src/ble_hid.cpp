#include "ble_hid.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

namespace {

constexpr uint8_t kReportKeyboard = 1;
constexpr uint8_t kReportConsumer = 2;
constexpr uint8_t kReportMouse = 3;

const uint8_t kReportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, kReportKeyboard,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x95, 0x05, 0x75, 0x01, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0xC0,
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, kReportConsumer,
    0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
    0xC0,
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, kReportMouse, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x05, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
    0xC0, 0xC0,
};

struct KeyReport {
    uint8_t modifiers = 0;
    uint8_t reserved = 0;
    uint8_t keys[6] = {};
};

NimBLEServer* server = nullptr;
NimBLEHIDDevice* hidDevice = nullptr;
NimBLECharacteristic* keyboardInput = nullptr;
NimBLECharacteristic* consumerInput = nullptr;
NimBLECharacteristic* mouseInput = nullptr;
KeyReport keyReport;
uint8_t mouseButtons = 0;
bool initialized = false;
bool enabled = false;
volatile bool linkUp = false;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, NimBLEConnInfo&) override { linkUp = true; }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        linkUp = false;
        if (enabled) NimBLEDevice::startAdvertising();
    }
};

ServerCallbacks serverCallbacks;

void sendKeyboard() {
    if (!keyboardInput || !linkUp) return;
    keyboardInput->setValue(reinterpret_cast<uint8_t*>(&keyReport), sizeof(keyReport));
    keyboardInput->notify();
}

void sendMouse(int8_t dx, int8_t dy, int8_t wheel) {
    if (!mouseInput || !linkUp) return;
    uint8_t report[4] = {mouseButtons, static_cast<uint8_t>(dx), static_cast<uint8_t>(dy), static_cast<uint8_t>(wheel)};
    mouseInput->setValue(report, sizeof(report));
    mouseInput->notify();
}

void sendConsumer(uint16_t usage) {
    if (!consumerInput || !linkUp) return;
    uint8_t report[2] = {static_cast<uint8_t>(usage & 0xFF), static_cast<uint8_t>(usage >> 8)};
    consumerInput->setValue(report, sizeof(report));
    consumerInput->notify();
}

bool asciiToCode(char c, uint8_t& code, bool& shift) {
    shift = false;
    if (c >= 'a' && c <= 'z') { code = 0x04 + (c - 'a'); return true; }
    if (c >= 'A' && c <= 'Z') { code = 0x04 + (c - 'A'); shift = true; return true; }
    if (c >= '1' && c <= '9') { code = 0x1E + (c - '1'); return true; }
    switch (c) {
        case '0': code = 0x27; return true;
        case '\n': code = 0x28; return true;
        case '\t': code = 0x2B; return true;
        case ' ': code = 0x2C; return true;
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
        case '!': code = 0x1E; shift = true; return true;
        case '@': code = 0x1F; shift = true; return true;
        case '#': code = 0x20; shift = true; return true;
        case '$': code = 0x21; shift = true; return true;
        case '%': code = 0x22; shift = true; return true;
        case '^': code = 0x23; shift = true; return true;
        case '&': code = 0x24; shift = true; return true;
        case '*': code = 0x25; shift = true; return true;
        case '(': code = 0x26; shift = true; return true;
        case ')': code = 0x27; shift = true; return true;
        case '_': code = 0x2D; shift = true; return true;
        case '+': code = 0x2E; shift = true; return true;
        case '{': code = 0x2F; shift = true; return true;
        case '}': code = 0x30; shift = true; return true;
        case '|': code = 0x31; shift = true; return true;
        case ':': code = 0x33; shift = true; return true;
        case '"': code = 0x34; shift = true; return true;
        case '~': code = 0x35; shift = true; return true;
        case '<': code = 0x36; shift = true; return true;
        case '>': code = 0x37; shift = true; return true;
        case '?': code = 0x38; shift = true; return true;
        case 0x08: code = 0x2A; return true;
        case 0x1B: code = 0x29; return true;
        default: return false;
    }
}

}

void ble_hid::begin(const char* name) {
    enabled = true;
    if (!initialized) {
        initialized = true;
        NimBLEDevice::init(name);
        NimBLEDevice::setSecurityAuth(true, false, true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        server = NimBLEDevice::createServer();
        server->setCallbacks(&serverCallbacks);
        hidDevice = new NimBLEHIDDevice(server);
        hidDevice->setManufacturer("M5Stack");
        hidDevice->setPnp(0x02, 0x303A, 0x8119, 0x0110);
        hidDevice->setHidInfo(0x00, 0x01);
        hidDevice->setReportMap(const_cast<uint8_t*>(kReportMap), sizeof(kReportMap));
        keyboardInput = hidDevice->getInputReport(kReportKeyboard);
        consumerInput = hidDevice->getInputReport(kReportConsumer);
        mouseInput = hidDevice->getInputReport(kReportMouse);
        server->start();
        hidDevice->setBatteryLevel(100);
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        adv->setAppearance(0x03C1);
        adv->addServiceUUID(hidDevice->getHidService()->getUUID());
        adv->setName(name);
        adv->enableScanResponse(true);
    }
    NimBLEDevice::startAdvertising();
}

void ble_hid::end() {
    enabled = false;
    if (!initialized) return;
    releaseAll();
    NimBLEDevice::stopAdvertising();
    if (server) {
        for (int i = 0; i < static_cast<int>(server->getConnectedCount()); ++i) {
            server->disconnect(server->getPeerInfo(i).getConnHandle());
        }
    }
    linkUp = false;
}

bool ble_hid::active() {
    return enabled;
}

bool ble_hid::connected() {
    return enabled && linkUp;
}

void ble_hid::pressKey(uint8_t code) {
    if (code >= 0xE0 && code <= 0xE7) {
        keyReport.modifiers |= static_cast<uint8_t>(1 << (code - 0xE0));
    } else if (code != 0) {
        for (uint8_t& k : keyReport.keys) {
            if (k == code) { sendKeyboard(); return; }
        }
        for (uint8_t& k : keyReport.keys) {
            if (k == 0) { k = code; break; }
        }
    }
    sendKeyboard();
}

void ble_hid::releaseKey(uint8_t code) {
    if (code >= 0xE0 && code <= 0xE7) {
        keyReport.modifiers &= static_cast<uint8_t>(~(1 << (code - 0xE0)));
    } else {
        for (uint8_t& k : keyReport.keys) {
            if (k == code) k = 0;
        }
    }
    sendKeyboard();
}

void ble_hid::releaseAll() {
    keyReport = KeyReport();
    mouseButtons = 0;
    sendKeyboard();
    sendMouse(0, 0, 0);
    sendConsumer(0);
}

void ble_hid::typeChar(char c) {
    uint8_t code = 0;
    bool shift = false;
    if (!asciiToCode(c, code, shift)) return;
    if (shift) pressKey(0xE1);
    pressKey(code);
    delay(4);
    releaseKey(code);
    if (shift) releaseKey(0xE1);
}

void ble_hid::mouseMove(int8_t dx, int8_t dy, int8_t wheel) {
    sendMouse(dx, dy, wheel);
}

void ble_hid::mousePress(uint8_t button) {
    mouseButtons |= button;
    sendMouse(0, 0, 0);
}

void ble_hid::mouseRelease(uint8_t button) {
    mouseButtons &= static_cast<uint8_t>(~button);
    sendMouse(0, 0, 0);
}

void ble_hid::consumerTap(uint16_t usage) {
    sendConsumer(usage);
    delay(10);
    sendConsumer(0);
}
