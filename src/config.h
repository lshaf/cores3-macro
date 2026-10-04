#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cfg {

constexpr const char* kFirmwareVersion = "1.0.0";
constexpr const char* kDeviceName = "core-macro";
constexpr const char* kMacroDir = "/macros";
constexpr const char* kMacroExt = ".txt";
constexpr const char* kConfigPath = "/config.json";

constexpr uint32_t kDefaultScreenTimeoutMs = 30000;
constexpr uint32_t kMaxScreenTimeoutMs = 3600000;
constexpr uint8_t kDefaultBrightness = 150;
constexpr uint8_t kMinBrightness = 5;

constexpr size_t kMaxScriptBytes = 24 * 1024;
constexpr size_t kMaxScriptNameLen = 32;
constexpr size_t kMaxSerialLine = 32 * 1024;
constexpr size_t kSerialRxBuffer = 16 * 1024;
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kSerialTxTimeoutMs = 20;

constexpr uint8_t kFacesAddr = 0x08;
constexpr uint32_t kFacesI2cFreq = 100000;
constexpr uint32_t kGamepadPollMs = 15;
constexpr uint32_t kGamepadProbeMs = 2000;
constexpr uint32_t kGamepadSettleMs = 400;
constexpr uint32_t kRepeatDelayMs = 350;
constexpr uint32_t kRepeatRateMs = 110;

constexpr uint32_t kKeyTapHoldMs = 8;
constexpr uint32_t kSelectionSaveDelayMs = 2000;

}
