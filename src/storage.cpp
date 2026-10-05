#include "storage.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <algorithm>

#include "config.h"

namespace {

bool fsReady = false;

bool allowedNameChar(char c) {
    if (isalnum(static_cast<unsigned char>(c))) return true;
    return c == ' ' || c == '_' || c == '-' || c == '.' || c == '(' || c == ')';
}

String baseName(const String& path) {
    const int slash = path.lastIndexOf('/');
    return slash < 0 ? path : path.substring(slash + 1);
}

}

bool storage::begin() {
    fsReady = LittleFS.begin(true);
    if (fsReady && !LittleFS.exists(cfg::kMacroDir)) LittleFS.mkdir(cfg::kMacroDir);
    return fsReady;
}

bool storage::ready() {
    return fsReady;
}

bool storage::validName(const String& name) {
    if (name.length() == 0 || name.length() > cfg::kMaxScriptNameLen) return false;
    if (name[0] == '.' || name[0] == ' ' || name[name.length() - 1] == ' ') return false;
    for (size_t i = 0; i < name.length(); ++i) {
        if (!allowedNameChar(name[i])) return false;
    }
    return true;
}

String storage::pathFor(const String& name) {
    return String(cfg::kMacroDir) + "/" + name + cfg::kMacroExt;
}

std::vector<ScriptInfo> storage::list() {
    std::vector<ScriptInfo> out;
    if (!fsReady) return out;
    File dir = LittleFS.open(cfg::kMacroDir);
    if (!dir || !dir.isDirectory()) return out;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (f.isDirectory()) continue;
        String name = baseName(f.name());
        if (!name.endsWith(cfg::kMacroExt)) continue;
        name.remove(name.length() - strlen(cfg::kMacroExt));
        out.push_back({name, static_cast<size_t>(f.size())});
    }
    std::sort(out.begin(), out.end(), [](const ScriptInfo& a, const ScriptInfo& b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return out;
}

bool storage::exists(const String& name) {
    return fsReady && validName(name) && LittleFS.exists(pathFor(name));
}

bool storage::read(const String& name, String& out) {
    if (!fsReady || !validName(name)) return false;
    File f = LittleFS.open(pathFor(name), FILE_READ);
    if (!f) return false;
    out = f.readString();
    f.close();
    return true;
}

bool storage::write(const String& name, const String& content) {
    if (!fsReady || !validName(name) || content.length() > cfg::kMaxScriptBytes) return false;
    File f = LittleFS.open(pathFor(name), FILE_WRITE);
    if (!f) return false;
    const size_t written = f.print(content);
    f.close();
    return written == content.length();
}

bool storage::remove(const String& name) {
    return fsReady && validName(name) && LittleFS.remove(pathFor(name));
}

bool storage::rename(const String& from, const String& to) {
    if (!fsReady || !validName(from) || !validName(to)) return false;
    if (LittleFS.exists(pathFor(to))) return false;
    return LittleFS.rename(pathFor(from), pathFor(to));
}

DeviceConfig storage::loadConfig() {
    DeviceConfig config{cfg::kDefaultScreenTimeoutMs, cfg::kDefaultBrightness, "", false, false, ""};
    if (!fsReady) return config;
    File f = LittleFS.open(cfg::kConfigPath, FILE_READ);
    if (!f) return config;
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        config.screenTimeoutMs = doc["screenTimeoutMs"] | config.screenTimeoutMs;
        config.brightness = doc["brightness"] | config.brightness;
        config.selected = doc["selected"] | "";
        config.bindMode = doc["bindMode"] | false;
        config.ble = doc["ble"] | false;
        config.preset = doc["preset"] | "";
    }
    f.close();
    return config;
}

bool storage::saveConfig(const DeviceConfig& config) {
    if (!fsReady) return false;
    JsonDocument doc;
    doc["screenTimeoutMs"] = config.screenTimeoutMs;
    doc["brightness"] = config.brightness;
    doc["selected"] = config.selected;
    doc["bindMode"] = config.bindMode;
    doc["ble"] = config.ble;
    doc["preset"] = config.preset;
    File f = LittleFS.open(cfg::kConfigPath, FILE_WRITE);
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

size_t storage::usedBytes() {
    return fsReady ? LittleFS.usedBytes() : 0;
}

size_t storage::totalBytes() {
    return fsReady ? LittleFS.totalBytes() : 0;
}
