#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

#include "bindings.h"
#include "gamepad.h"
#include "macro.h"
#include "storage.h"
#include "ui.h"

class App : public MacroHost {
public:
    void setup();
    void loop();
    bool isRunning(const String& script) override;
    void run(const String& script) override;
    void stop() override { stopMacro(); }
    bool exists(const String& script) override;

private:
    void refreshScripts();
    bool selectByName(const String& name);
    void setSelected(int index);
    void moveSelection(int delta);
    void runSelected();
    void stopMacro();
    void setBindMode(bool enabled, uint32_t now);
    void setTransport(bool ble, uint32_t now);
    void handleHolds(uint32_t now);
    void handleMenuInput(uint32_t now);
    void handlePresetInput(uint32_t now);
    void openPresets();
    void closePresets();
    void savePresetAs(const String& name, uint32_t now);
    bool loadPresetNamed(const String& name, uint32_t now);
    void sendPresets();
    void fillPresets(JsonDocument& doc);
    void openSetup();
    void closeSetup();
    void beginEdit();
    void commitEdit(uint32_t now);
    void adjustField(int delta);
    void handleSetupInput(uint32_t now);
    void sendMode();
    void sendBinds();
    void sendPad();
    void fillBinds(JsonDocument& doc);
    void markActivity(uint32_t now);
    void wakeScreen(uint32_t now);
    void sleepScreen();
    void applyBrightness();

    void handleInput(uint32_t now);
    void handleSerial(uint32_t now);
    void handleRunner(uint32_t now);
    void handleScreen(uint32_t now);
    void maybeSaveConfig(uint32_t now);

    void dispatch(JsonDocument& req, uint32_t now);
    void fillState(JsonDocument& doc, const MacroStatus& status, uint32_t now);
    void fillList(JsonDocument& doc);
    void sendState(uint32_t now);
    void sendSelected();
    UiModel model();

    std::vector<ScriptInfo> _scripts;
    int _selected = 0;
    DeviceConfig _config{};
    Gamepad _pad;
    Bindings _binds;
    bool _bindMode = false;
    BindSetup _setup;
    MenuState _menu;
    PresetState _presets;
    std::vector<String> _presetNames;

    struct HoldTracker {
        uint32_t downAt = 0;
        bool held = false;
        bool consumed = false;
    };
    HoldTracker _holdSelect;
    HoldTracker _holdStart;
    MacroRunner _runner;
    Ui _ui;
    MacroStatus _lastStatus;

    uint32_t _lastActivity = 0;
    uint32_t _lastRender = 0;
    uint32_t _lastProgressSent = 0;
    uint32_t _configDirtyAt = 0;
    bool _configDirty = false;
    bool _screenOn = true;
    bool _dirty = true;
    bool _swallowTouch = false;
    bool _storageOk = false;
};
