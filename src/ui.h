#pragma once

#include <M5Unified.h>

#include <vector>

#include "bindings.h"
#include "macro.h"
#include "storage.h"

struct UiModel {
    const std::vector<ScriptInfo>* scripts = nullptr;
    int selected = 0;
    MacroStatus status;
    bool usb = false;
    bool host = false;
    bool gamepad = false;
    bool storageOk = true;
    bool ble = false;
    bool outputReady = false;
    bool bindMode = false;
    const Bindings* binds = nullptr;
    uint8_t padPressed = 0;
    uint8_t padToggled = 0;
    const BindSetup* setup = nullptr;
    const MenuState* menu = nullptr;
    const PresetState* presets = nullptr;
    const std::vector<String>* presetNames = nullptr;
    String activePreset;
    uint32_t screenTimeoutSec = 30;
    uint8_t brightness = 150;
};

class Ui {
public:
    void begin();
    void render(const UiModel& model, uint32_t now);
    int rowAt(int x, int y) const;
    bool statusCardAt(int x, int y) const;
    int visibleRows() const;

private:
    void ensureVisible(int selected, int count);
    void drawHeader(const UiModel& model);
    void drawList(const UiModel& model, uint32_t now);
    void drawBinds(const UiModel& model);
    void drawSetupList(const UiModel& model);
    void drawSetupEdit(const UiModel& model);
    void drawMenu(const UiModel& model);
    void drawPresets(const UiModel& model);
    void drawStatus(const UiModel& model, uint32_t now);
    void drawFooter(const UiModel& model);
    int drawKeycap(const char* label, int x, int cy);
    int drawHint(const char* cap, const char* text, int x, int cy);
    int drawArrowHint(bool vertical, const char* text, int x, int cy);
    String fitText(const String& text, int maxWidth);

    M5Canvas _canvas;
    int _scroll = 0;
};
