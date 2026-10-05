#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

enum class BindBehavior : uint8_t { Normal, Burst, Toggle, ToggleBurst };
constexpr int kBindBehaviorCount = 4;

struct Binding {
    String keys;
    String script;
    BindBehavior behavior = BindBehavior::Normal;
    uint16_t intervalMs = 100;
    uint8_t codes[8] = {};
    uint8_t codeCount = 0;
};

struct MenuState {
    bool open = false;
    int row = 0;
};

struct PresetState {
    bool open = false;
    int row = 0;
    int confirm = 0;
    int confirmRow = -1;
};

struct BindSetup {
    bool open = false;
    bool editing = false;
    int row = 0;
    int field = 0;
    int group = 0;
    int keyIndex = 0;
    int modIndex = 0;
    BindBehavior behavior = BindBehavior::Normal;
    int interval = 100;
};

class MacroHost {
public:
    virtual ~MacroHost() = default;
    virtual bool isRunning(const String& script) = 0;
    virtual void run(const String& script) = 0;
    virtual void stop() = 0;
    virtual bool exists(const String& script) = 0;
};

class Bindings {
public:
    static constexpr int kSlots = 6;
    static constexpr int kMacroGroup = 1;
    static constexpr int kSetupFields = 5;

    static const char* buttonName(int slot);
    static uint8_t buttonMask(int slot);
    static int slotForName(const char* name);
    static const char* behaviorName(BindBehavior behavior);
    static bool behaviorFromName(const char* name, BindBehavior& out);
    static int groupCount();
    static const char* groupName(int group);
    static int groupKeyCount(int group);
    static const char* groupKey(int group, int index);
    static int modifierCount();
    static const char* modifierName(int index);
    static void split(const String& keys, int& group, int& keyIndex, int& modIndex);
    static String compose(int group, int keyIndex, int modIndex);

    void load();
    bool save() const;
    bool applyJson(JsonArrayConst list, String& err);
    bool setSlot(int slot, const String& keys, BindBehavior behavior, uint16_t intervalMs, String& err);
    bool setSlotMacro(int slot, const String& script);
    static std::vector<String> presetNames();
    static bool presetExists(const String& name);
    bool savePreset(const String& name) const;
    bool loadPreset(const String& name);
    static bool deletePreset(const String& name);
    static bool renamePreset(const String& from, const String& to);
    static String freePresetName();
    void toJson(JsonArray out) const;
    const Binding& slot(int index) const { return _slots[index]; }

    void activate();
    void deactivate();
    void update(uint8_t pressedMask, uint32_t now, MacroHost& host);
    uint8_t pressedMask() const { return _pressed; }
    uint8_t toggledMask() const { return _toggled; }
    bool takeChanged();

private:
    static bool compile(Binding& binding, String& err);
    void pressSlot(int index);
    void releaseSlot(int index);
    void tapSlot(int index);

    Binding _slots[kSlots];
    uint8_t _pressed = 0;
    uint8_t _toggled = 0;
    uint8_t _bursting = 0;
    bool _fresh = true;
    uint32_t _nextBurst[kSlots] = {};
    bool _changed = false;
};
