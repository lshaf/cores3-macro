#pragma once

#include <Arduino.h>

#include <vector>

struct ScriptInfo {
    String name;
    size_t size;
};

struct DeviceConfig {
    uint32_t screenTimeoutMs;
    uint8_t brightness;
    String selected;
    bool bindMode;
    bool ble;
    String preset;
};

namespace storage {

bool begin();
bool ready();
bool validName(const String& name);
String pathFor(const String& name);
std::vector<ScriptInfo> list();
bool exists(const String& name);
bool read(const String& name, String& out);
bool write(const String& name, const String& content);
bool remove(const String& name);
bool rename(const String& from, const String& to);
DeviceConfig loadConfig();
bool saveConfig(const DeviceConfig& config);
size_t usedBytes();
size_t totalBytes();

}
