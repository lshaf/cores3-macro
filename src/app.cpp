#include "app.h"

#include <M5Unified.h>

#include "config.h"
#include "hid.h"
#include "serial_api.h"

namespace {

const char* stateName(MacroState state) {
    switch (state) {
        case MacroState::Running: return "running";
        case MacroState::Finished: return "finished";
        case MacroState::Stopped: return "stopped";
        case MacroState::Error: return "error";
        case MacroState::Idle:
        default: return "idle";
    }
}

void failResponse(JsonDocument& res, const String& message) {
    res["ok"] = false;
    res["error"] = message;
}

String fieldFrom(JsonDocument& req, const char* key) {
    const char* value = req[key] | "";
    String text(value);
    text.trim();
    return text;
}

constexpr const char* kNameRule = "Name must be 1-32 characters: letters, digits, space, _ - . ( )";

}

void App::setup() {
    auto config = M5.config();
    config.internal_imu = false;
    config.internal_rtc = false;
    config.internal_mic = false;
    config.internal_spk = false;
    config.output_power = true;
    M5.begin(config);
    if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);

    _storageOk = storage::begin();
    _config = storage::loadConfig();
    applyBrightness();
    _binds.load();
    _bindMode = false;
    if (_config.ble) hid::setTransport(hid::Transport::Ble);

    hid::begin();
    serial_api::begin();
    _runner.begin();
    _pad.begin();
    _ui.begin();

    refreshScripts();
    selectByName(_config.selected);
    _configDirty = false;
    _lastActivity = millis();
    _dirty = true;
}

void App::loop() {
    const uint32_t now = millis();
    M5.update();
    _pad.update();
    handleInput(now);
    handleSerial(now);
    handleRunner(now);
    handleScreen(now);
    maybeSaveConfig(now);

    if (_screenOn) {
        const bool running = _lastStatus.state == MacroState::Running;
        const uint32_t sinceRender = now - _lastRender;
        const bool due = _dirty ? sinceRender >= (running ? 150u : 40u) : (running && sinceRender >= 250u);
        if (due) {
            _ui.render(model(), now);
            _dirty = false;
            _lastRender = now;
        }
    }
    delay(5);
}

UiModel App::model() {
    UiModel m;
    m.scripts = &_scripts;
    m.selected = _selected;
    m.status = _lastStatus;
    m.usb = hid::mounted();
    m.host = serial_api::hostConnected();
    m.gamepad = _pad.available();
    m.storageOk = _storageOk;
    m.ble = hid::transport() == hid::Transport::Ble;
    m.outputReady = hid::outputReady();
    m.bindMode = _bindMode;
    m.binds = &_binds;
    m.padPressed = _binds.pressedMask();
    m.padToggled = _binds.toggledMask();
    m.setup = &_setup;
    return m;
}

void App::refreshScripts() {
    const String current = (_selected >= 0 && _selected < static_cast<int>(_scripts.size())) ? _scripts[_selected].name : String();
    _scripts = storage::list();
    if (!selectByName(current)) setSelected(_selected);
    _dirty = true;
}

bool App::selectByName(const String& name) {
    if (name.length() == 0) return false;
    for (size_t i = 0; i < _scripts.size(); ++i) {
        if (_scripts[i].name == name) {
            setSelected(static_cast<int>(i));
            return true;
        }
    }
    return false;
}

void App::setSelected(int index) {
    const int count = static_cast<int>(_scripts.size());
    if (count == 0) {
        index = 0;
    } else {
        if (index < 0) index = 0;
        if (index >= count) index = count - 1;
    }
    _selected = index;
    _dirty = true;
    const String name = count > 0 ? _scripts[_selected].name : String();
    if (name != _config.selected) {
        _config.selected = name;
        _configDirty = true;
        _configDirtyAt = millis();
        sendSelected();
    }
}

void App::moveSelection(int delta) {
    if (_scripts.empty()) return;
    setSelected(_selected + delta);
}

void App::runSelected() {
    if (_scripts.empty() || _selected < 0 || _selected >= static_cast<int>(_scripts.size())) return;
    const String name = _scripts[_selected].name;
    String source;
    if (!storage::read(name, source)) return;
    ParseError err;
    _runner.start(name, source, err);
    _dirty = true;
}

void App::stopMacro() {
    _runner.stop();
    _dirty = true;
}

bool App::isRunning(const String& script) {
    return _runner.running() && _runner.snapshot().script == script;
}

bool App::exists(const String& script) {
    return storage::exists(script);
}

void App::run(const String& script) {
    String source;
    if (!storage::read(script, source)) return;
    ParseError err;
    _runner.start(script, source, err);
    _dirty = true;
}

void App::setBindMode(bool enabled, uint32_t now) {
    if (enabled == _bindMode) return;
    _bindMode = enabled;
    _setup = BindSetup();
    if (enabled) {
        stopMacro();
        _binds.activate();
    } else {
        _binds.deactivate();
        stopMacro();
    }
    _dirty = true;
    sendMode();
}

void App::setTransport(bool ble, uint32_t now) {
    const bool current = hid::transport() == hid::Transport::Ble;
    if (ble == current) return;
    hid::setTransport(ble ? hid::Transport::Ble : hid::Transport::Usb);
    _config.ble = ble;
    _configDirty = true;
    _configDirtyAt = now;
    _dirty = true;
    sendState(now);
}

void App::handleStartButton(uint32_t now) {
    const bool held = _pad.pressed(PAD_START);
    if (_pad.fired(PAD_START)) {
        _startDownAt = now;
        _startConsumed = false;
        _startHeld = true;
        return;
    }
    if (held && _startHeld && !_startConsumed && now - _startDownAt >= cfg::kStartHoldMs) {
        _startConsumed = true;
        setTransport(hid::transport() != hid::Transport::Ble, now);
        return;
    }
    if (!held && _startHeld) {
        _startHeld = false;
        if (_startConsumed) return;
        if (!_bindMode) return;
        if (_setup.open) {
            if (!_setup.editing) closeSetup();
        } else {
            openSetup();
        }
    }
}

void App::openSetup() {
    _binds.deactivate();
    _setup = BindSetup();
    _setup.open = true;
    _dirty = true;
}

void App::closeSetup() {
    _setup = BindSetup();
    _binds.activate();
    _dirty = true;
}

void App::beginEdit() {
    const Binding& b = _binds.slot(_setup.row);
    Bindings::split(b.keys, _setup.group, _setup.keyIndex, _setup.modIndex);
    if (b.script.length()) {
        _setup.group = Bindings::kMacroGroup;
        _setup.keyIndex = 0;
        for (size_t i = 0; i < _scripts.size(); ++i) {
            if (_scripts[i].name == b.script) _setup.keyIndex = static_cast<int>(i);
        }
    }
    _setup.behavior = b.behavior;
    _setup.interval = b.intervalMs;
    _setup.field = 0;
    _setup.editing = true;
    _dirty = true;
}

void App::commitEdit(uint32_t now) {
    String err;
    const String keys = Bindings::compose(_setup.group, _setup.keyIndex, _setup.modIndex);
    bool ok = false;
    if (_setup.group == Bindings::kMacroGroup) {
        if (_setup.keyIndex >= 0 && _setup.keyIndex < static_cast<int>(_scripts.size())) ok = _binds.setSlotMacro(_setup.row, _scripts[_setup.keyIndex].name);
    } else {
        ok = _binds.setSlot(_setup.row, keys, _setup.behavior, static_cast<uint16_t>(_setup.interval), err);
    }
    if (ok) {
        _binds.save();
        sendBinds();
    }
    _setup.editing = false;
    _dirty = true;
    (void)now;
}

void App::adjustField(int delta) {
    switch (_setup.field) {
        case 0: {
            const int n = Bindings::groupCount();
            _setup.group = (_setup.group + delta % n + n) % n;
            _setup.keyIndex = 0;
            break;
        }
        case 1: {
            const int n = _setup.group == Bindings::kMacroGroup ? static_cast<int>(_scripts.size()) : Bindings::groupKeyCount(_setup.group);
            if (n > 0) _setup.keyIndex = (_setup.keyIndex + delta % n + n) % n;
            break;
        }
        case 2: {
            const int n = Bindings::modifierCount();
            _setup.modIndex = (_setup.modIndex + delta % n + n) % n;
            break;
        }
        case 3: {
            int v = (static_cast<int>(_setup.behavior) + delta % kBindBehaviorCount + kBindBehaviorCount) % kBindBehaviorCount;
            _setup.behavior = static_cast<BindBehavior>(v);
            break;
        }
        case 4: {
            int v = _setup.interval + delta * 10;
            if (v < 20) v = 20;
            if (v > 5000) v = 5000;
            _setup.interval = v;
            break;
        }
    }
    _dirty = true;
}

void App::handleSetupInput(uint32_t now) {
    if (!_setup.editing) {
        if (_pad.fired(PAD_UP)) _setup.row = (_setup.row + Bindings::kSlots - 1) % Bindings::kSlots;
        if (_pad.fired(PAD_DOWN)) _setup.row = (_setup.row + 1) % Bindings::kSlots;
        if (_pad.fired(PAD_A)) beginEdit();
        if (_pad.fired(PAD_B)) closeSetup();
        _dirty = true;
        return;
    }
    auto enabled = [&](int field) {
        if (field == 1) return _setup.group != 0;
        if (_setup.group == Bindings::kMacroGroup) return field == 0 || field == 1;
        if (field == 4) return _setup.behavior == BindBehavior::Burst || _setup.behavior == BindBehavior::ToggleBurst;
        return true;
    };
    auto step = [&](int delta) {
        int f = _setup.field;
        for (int i = 0; i < Bindings::kSetupFields; ++i) {
            f = (f + delta + Bindings::kSetupFields) % Bindings::kSetupFields;
            if (enabled(f)) break;
        }
        _setup.field = f;
    };
    if (_pad.fired(PAD_UP)) step(-1);
    if (_pad.fired(PAD_DOWN)) step(1);
    if (!enabled(_setup.field)) step(1);
    if (_pad.fired(PAD_LEFT)) adjustField(-1);
    if (_pad.fired(PAD_RIGHT)) adjustField(1);
    if (_pad.fired(PAD_A)) commitEdit(now);
    if (_pad.fired(PAD_B)) _setup.editing = false;
    _dirty = true;
}

void App::markActivity(uint32_t now) {
    _lastActivity = now;
    if (!_screenOn) wakeScreen(now);
}

void App::wakeScreen(uint32_t now) {
    M5.Display.wakeup();
    applyBrightness();
    _screenOn = true;
    _lastActivity = now;
    _lastRender = 0;
    _dirty = true;
}

void App::sleepScreen() {
    M5.Display.sleep();
    _screenOn = false;
}

void App::applyBrightness() {
    M5.Display.setBrightness(_config.brightness);
}

void App::handleInput(uint32_t now) {
    if (_pad.anyFired()) {
        if (!_screenOn) {
            wakeScreen(now);
        } else {
            _lastActivity = now;
            if (_pad.fired(PAD_SELECT)) {
                setBindMode(!_bindMode, now);
            } else if (_bindMode && _setup.open) {
                handleSetupInput(now);
            } else if (!_bindMode) {
                if (_pad.fired(PAD_UP)) moveSelection(-1);
                if (_pad.fired(PAD_DOWN)) moveSelection(1);
                if (_pad.fired(PAD_LEFT)) moveSelection(-_ui.visibleRows());
                if (_pad.fired(PAD_RIGHT)) moveSelection(_ui.visibleRows());
                if (_pad.fired(PAD_A)) runSelected();
                if (_pad.fired(PAD_B)) stopMacro();
            }
        }
    }
    if (_screenOn) handleStartButton(now);
    if (_bindMode && !_setup.open) {
        _binds.update(static_cast<uint8_t>(_pad.pressedMask() & ~(PAD_SELECT | PAD_START)), now, *this);
        if (_binds.takeChanged()) {
            _dirty = true;
            sendPad();
        }
    }

    const auto& touch = M5.Touch.getDetail();
    if (touch.wasPressed()) {
        if (!_screenOn) {
            wakeScreen(now);
            _swallowTouch = true;
        } else {
            _lastActivity = now;
        }
    }
    if (touch.wasClicked()) {
        if (_swallowTouch) {
            _swallowTouch = false;
            return;
        }
        if (!_screenOn) return;
        if (_bindMode) {
            if (_ui.statusCardAt(touch.x, touch.y)) {
                if (_setup.open) closeSetup();
                else setBindMode(false, now);
            }
            return;
        }
        const int row = _ui.rowAt(touch.x, touch.y);
        if (row >= 0 && row < static_cast<int>(_scripts.size())) {
            if (row == _selected) runSelected();
            else setSelected(row);
        } else if (_ui.statusCardAt(touch.x, touch.y) && _lastStatus.state == MacroState::Running) {
            stopMacro();
        }
    }
}

void App::handleSerial(uint32_t now) {
    String line;
    for (int guard = 0; guard < 4 && serial_api::nextLine(line); ++guard) {
        JsonDocument req;
        const DeserializationError err = deserializeJson(req, line);
        if (err) {
            JsonDocument res;
            res["type"] = "error";
            failResponse(res, String("Bad JSON: ") + err.c_str());
            serial_api::send(res);
            continue;
        }
        dispatch(req, now);
    }
}

void App::handleRunner(uint32_t now) {
    const MacroStatus s = _runner.snapshot();
    if (s.generation != _lastStatus.generation) {
        _lastStatus = s;
        _dirty = true;
        sendState(now);
        _lastProgressSent = now;
        return;
    }
    if (s.state != MacroState::Running) return;
    if (s.line != _lastStatus.line || s.loopIteration != _lastStatus.loopIteration || s.loopDepth != _lastStatus.loopDepth) {
        _lastStatus = s;
        _dirty = true;
        if (now - _lastProgressSent >= 200) {
            sendState(now);
            _lastProgressSent = now;
        }
    }
}

void App::handleScreen(uint32_t now) {
    if (_screenOn && _config.screenTimeoutMs > 0 && now - _lastActivity >= _config.screenTimeoutMs) sleepScreen();
}

void App::maybeSaveConfig(uint32_t now) {
    if (_configDirty && now - _configDirtyAt >= cfg::kSelectionSaveDelayMs) {
        _configDirty = false;
        storage::saveConfig(_config);
    }
}

void App::fillState(JsonDocument& doc, const MacroStatus& s, uint32_t now) {
    doc["state"] = stateName(s.state);
    doc["script"] = s.script;
    doc["line"] = s.line;
    doc["total"] = s.totalLines;
    doc["loop"] = s.loopIteration;
    doc["loopCount"] = s.loopCount;
    doc["loopDepth"] = s.loopDepth;
    uint32_t elapsed = 0;
    if (s.state == MacroState::Running) elapsed = now - s.startedMs;
    else if (s.startedMs != 0) elapsed = s.finishedMs - s.startedMs;
    doc["elapsed"] = elapsed;
    doc["error"] = s.error;
    doc["usb"] = hid::mounted();
    doc["gamepad"] = _pad.available();
    doc["mode"] = _bindMode ? "bind" : "macro";
    doc["transport"] = hid::transport() == hid::Transport::Ble ? "ble" : "usb";
    doc["ready"] = hid::outputReady();
}

void App::fillBinds(JsonDocument& doc) {
    doc["mode"] = _bindMode ? "bind" : "macro";
    _binds.toJson(doc["binds"].to<JsonArray>());
}

void App::fillList(JsonDocument& doc) {
    JsonArray arr = doc["scripts"].to<JsonArray>();
    for (const ScriptInfo& script : _scripts) {
        JsonObject entry = arr.add<JsonObject>();
        entry["name"] = script.name;
        entry["size"] = script.size;
    }
    doc["selected"] = _scripts.empty() ? String() : _scripts[_selected].name;
}

void App::sendState(uint32_t now) {
    if (!serial_api::hostConnected()) return;
    JsonDocument ev;
    ev["type"] = "state";
    fillState(ev, _lastStatus, now);
    serial_api::send(ev);
}

void App::sendBinds() {
    if (!serial_api::hostConnected()) return;
    JsonDocument ev;
    ev["type"] = "binds";
    fillBinds(ev);
    serial_api::send(ev);
}

void App::sendMode() {
    if (!serial_api::hostConnected()) return;
    JsonDocument ev;
    ev["type"] = "mode";
    ev["mode"] = _bindMode ? "bind" : "macro";
    serial_api::send(ev);
}

void App::sendPad() {
    if (!serial_api::hostConnected()) return;
    JsonDocument ev;
    ev["type"] = "pad";
    ev["pressed"] = _binds.pressedMask();
    ev["toggled"] = _binds.toggledMask();
    serial_api::send(ev);
}

void App::sendSelected() {
    if (!serial_api::hostConnected()) return;
    JsonDocument ev;
    ev["type"] = "selected";
    ev["name"] = _config.selected;
    serial_api::send(ev);
}

void App::dispatch(JsonDocument& req, uint32_t now) {
    JsonDocument res;
    String cmd = fieldFrom(req, "cmd");
    cmd.toLowerCase();
    if (!req["id"].isNull()) res["id"] = req["id"];
    res["type"] = cmd;
    res["ok"] = true;

    if (cmd == "ping") {
        res["device"] = cfg::kDeviceName;
        res["fw"] = cfg::kFirmwareVersion;
        fillState(res, _runner.snapshot(), now);
    } else if (cmd == "list") {
        fillList(res);
    } else if (cmd == "get") {
        const String name = fieldFrom(req, "name");
        String content;
        if (!storage::read(name, content)) {
            failResponse(res, "Macro not found: " + name);
        } else {
            res["name"] = name;
            res["content"] = content;
        }
    } else if (cmd == "put") {
        const String name = fieldFrom(req, "name");
        const char* contentPtr = req["content"] | "";
        const String content(contentPtr);
        if (!storage::validName(name)) {
            failResponse(res, kNameRule);
        } else if (content.length() > cfg::kMaxScriptBytes) {
            failResponse(res, "Macro is too large (max 24 KB)");
        } else if (!storage::write(name, content)) {
            failResponse(res, "Could not write to device storage");
        } else {
            ParseError perr;
            const bool valid = MacroRunner::check(content, perr);
            res["name"] = name;
            res["valid"] = valid;
            if (!valid) {
                res["error"] = perr.message;
                res["line"] = perr.line;
            }
            refreshScripts();
            selectByName(name);
            markActivity(now);
        }
    } else if (cmd == "del") {
        const String name = fieldFrom(req, "name");
        if (!storage::exists(name)) {
            failResponse(res, "Macro not found: " + name);
        } else {
            if (_lastStatus.state == MacroState::Running && _lastStatus.script == name) stopMacro();
            if (!storage::remove(name)) {
                failResponse(res, "Could not delete " + name);
            } else {
                refreshScripts();
                markActivity(now);
            }
        }
    } else if (cmd == "rename") {
        const String from = fieldFrom(req, "from");
        const String to = fieldFrom(req, "to");
        if (!storage::exists(from)) {
            failResponse(res, "Macro not found: " + from);
        } else if (!storage::validName(to)) {
            failResponse(res, kNameRule);
        } else if (storage::exists(to)) {
            failResponse(res, "A macro named " + to + " already exists");
        } else {
            if (_lastStatus.state == MacroState::Running && _lastStatus.script == from) stopMacro();
            if (!storage::rename(from, to)) {
                failResponse(res, "Could not rename " + from);
            } else {
                refreshScripts();
                selectByName(to);
                markActivity(now);
            }
        }
    } else if (cmd == "run") {
        const String name = fieldFrom(req, "name");
        if (_bindMode) {
            failResponse(res, "Device is in bind mode. Switch to macros first.");
        } else if (name.length() > 0 && !selectByName(name)) {
            failResponse(res, "Macro not found: " + name);
        } else {
            runSelected();
            fillState(res, _runner.snapshot(), now);
            markActivity(now);
        }
    } else if (cmd == "stop") {
        stopMacro();
        fillState(res, _runner.snapshot(), now);
        markActivity(now);
    } else if (cmd == "select") {
        const String name = fieldFrom(req, "name");
        if (!selectByName(name)) failResponse(res, "Macro not found: " + name);
        else markActivity(now);
    } else if (cmd == "status") {
        fillState(res, _runner.snapshot(), now);
    } else if (cmd == "check") {
        const char* contentPtr = req["content"] | "";
        ParseError perr;
        const bool valid = MacroRunner::check(String(contentPtr), perr);
        res["valid"] = valid;
        if (!valid) {
            res["error"] = perr.message;
            res["line"] = perr.line;
        }
    } else if (cmd == "config") {
        if (req["set"].is<JsonObject>()) {
            JsonObject set = req["set"].as<JsonObject>();
            if (!set["screenTimeout"].isNull()) {
                uint32_t seconds = set["screenTimeout"].as<uint32_t>();
                if (seconds * 1000u > cfg::kMaxScreenTimeoutMs) seconds = cfg::kMaxScreenTimeoutMs / 1000u;
                _config.screenTimeoutMs = seconds * 1000u;
            }
            if (!set["brightness"].isNull()) {
                int brightness = set["brightness"].as<int>();
                if (brightness < cfg::kMinBrightness) brightness = cfg::kMinBrightness;
                if (brightness > 255) brightness = 255;
                _config.brightness = static_cast<uint8_t>(brightness);
                applyBrightness();
            }
            if (!set["transport"].isNull()) {
                const String transport = String(set["transport"] | "usb");
                setTransport(transport == "ble", now);
            }
            _configDirty = true;
            _configDirtyAt = now;
            markActivity(now);
        }
        res["screenTimeout"] = _config.screenTimeoutMs / 1000u;
        res["brightness"] = _config.brightness;
        res["transport"] = hid::transport() == hid::Transport::Ble ? "ble" : "usb";
    } else if (cmd == "binds") {
        if (req["set"].is<JsonArray>()) {
            String err;
            if (!_binds.applyJson(req["set"].as<JsonArrayConst>(), err)) {
                failResponse(res, err);
            } else {
                if (_bindMode && !_setup.open) _binds.activate();
                if (_setup.editing) beginEdit();
                if (!_binds.save()) failResponse(res, "Could not write bindings to device storage");
                markActivity(now);
                _dirty = true;
            }
        }
        if (res["ok"] == true) fillBinds(res);
    } else if (cmd == "mode") {
        const String set = fieldFrom(req, "set");
        if (set.length() > 0) {
            if (set == "bind") setBindMode(true, now);
            else if (set == "macro") setBindMode(false, now);
            else failResponse(res, "Mode must be macro or bind");
            markActivity(now);
        }
        res["mode"] = _bindMode ? "bind" : "macro";
    } else if (cmd == "info") {
        res["device"] = cfg::kDeviceName;
        res["fw"] = cfg::kFirmwareVersion;
        res["fsUsed"] = storage::usedBytes();
        res["fsTotal"] = storage::totalBytes();
        res["uptime"] = now;
        res["freeHeap"] = ESP.getFreeHeap();
        res["scripts"] = _scripts.size();
    } else {
        failResponse(res, cmd.length() > 0 ? "Unknown command: " + cmd : String("Missing cmd"));
    }

    serial_api::send(res);
}
