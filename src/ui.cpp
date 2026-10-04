#include "ui.h"

#include "config.h"

namespace {

constexpr int kW = 320;
constexpr int kH = 240;
constexpr int kHeaderH = 28;
constexpr int kFooterH = 24;
constexpr int kStatusH = 40;
constexpr int kListY = kHeaderH;
constexpr int kListH = kH - kHeaderH - kStatusH - kFooterH;
constexpr int kRowH = 37;
constexpr int kRows = kListH / kRowH;
constexpr int kStatusY = kListY + kListH;
constexpr int kFooterY = kStatusY + kStatusH;

constexpr uint32_t kBg = 0x0F1216;
constexpr uint32_t kPanel = 0x171C22;
constexpr uint32_t kRowSel = 0x243244;
constexpr uint32_t kLine = 0x242B33;
constexpr uint32_t kText = 0xD6DCE4;
constexpr uint32_t kTextBright = 0xFFFFFF;
constexpr uint32_t kTextDim = 0x7A8694;
constexpr uint32_t kDotOff = 0x3A4451;
constexpr uint32_t kAccent = 0xF2B544;
constexpr uint32_t kGreen = 0x3FD47C;
constexpr uint32_t kGreenBg = 0x10281B;
constexpr uint32_t kRed = 0xF0565A;
constexpr uint32_t kRedBg = 0x2E1517;
constexpr uint32_t kBlue = 0x5AA9FF;
constexpr uint32_t kKeycap = 0x2C3542;

String formatElapsed(uint32_t ms) {
    const uint32_t totalSec = ms / 1000;
    const uint32_t minutes = totalSec / 60;
    const uint32_t seconds = totalSec % 60;
    char buf[16];
    if (minutes >= 100) {
        snprintf(buf, sizeof(buf), "%luh%02lum", static_cast<unsigned long>(minutes / 60), static_cast<unsigned long>(minutes % 60));
    } else {
        snprintf(buf, sizeof(buf), "%02lu:%02lu", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
    }
    return String(buf);
}

}

void Ui::begin() {
    _canvas.setPsram(true);
    _canvas.setColorDepth(16);
    _canvas.createSprite(kW, kH);
    _canvas.setTextWrap(false);
}

int Ui::visibleRows() const {
    return kRows;
}

int Ui::rowAt(int x, int y) const {
    (void)x;
    if (y < kListY || y >= kListY + kRows * kRowH) return -1;
    return _scroll + (y - kListY) / kRowH;
}

bool Ui::statusCardAt(int x, int y) const {
    (void)x;
    return y >= kStatusY && y < kFooterY;
}

void Ui::ensureVisible(int selected, int count) {
    if (count <= kRows) {
        _scroll = 0;
        return;
    }
    if (selected < _scroll) _scroll = selected;
    if (selected >= _scroll + kRows) _scroll = selected - kRows + 1;
    if (_scroll > count - kRows) _scroll = count - kRows;
    if (_scroll < 0) _scroll = 0;
}

String Ui::fitText(const String& text, int maxWidth) {
    if (_canvas.textWidth(text) <= maxWidth) return text;
    String out = text;
    while (out.length() > 1 && _canvas.textWidth(out + "...") > maxWidth) out.remove(out.length() - 1);
    return out + "...";
}

void Ui::render(const UiModel& model, uint32_t now) {
    const int count = model.scripts ? static_cast<int>(model.scripts->size()) : 0;
    ensureVisible(model.selected, count);
    _canvas.fillScreen(kBg);
    _canvas.setFont(&fonts::Font2);
    _canvas.setTextSize(1);
    drawHeader(model);
    const bool setupOpen = model.setup && model.setup->open;
    if (model.bindMode && setupOpen && model.setup->editing) drawSetupEdit(model);
    else if (model.bindMode && setupOpen) drawSetupList(model);
    else if (model.bindMode) drawBinds(model);
    else drawList(model, now);
    drawStatus(model, now);
    drawFooter(model);
    _canvas.pushSprite(&M5.Display, 0, 0);
}

void Ui::drawHeader(const UiModel& model) {
    _canvas.fillRect(0, 0, kW, kHeaderH, kPanel);
    _canvas.drawFastHLine(0, kHeaderH - 1, kW, kLine);
    _canvas.setTextDatum(textdatum_t::middle_left);
    _canvas.setTextColor(kAccent, kPanel);
    _canvas.drawString("CORE MACRO", 10, kHeaderH / 2);
    if (model.bindMode) {
        const int px = 10 + _canvas.textWidth("CORE MACRO") + 10;
        _canvas.fillRoundRect(px, kHeaderH / 2 - 9, (model.setup && model.setup->open) ? 48 : 40, 18, 4, kAccent);
        _canvas.setTextDatum(textdatum_t::middle_center);
        _canvas.setTextColor(kBg, kAccent);
        const bool setupOpen = model.setup && model.setup->open;
        _canvas.drawString(setupOpen ? "SETUP" : "BIND", px + (setupOpen ? 24 : 20), kHeaderH / 2);
        _canvas.setTextDatum(textdatum_t::middle_left);
    }

    struct Indicator {
        const char* label;
        bool on;
        uint32_t color;
    };
    const Indicator indicators[] = {
        {"PAD", model.gamepad, kAccent},
        {"WEB", model.host, kBlue},
        {model.ble ? "BLE" : "USB", model.ble ? model.outputReady : model.usb, model.ble ? kBlue : kGreen},
    };
    int x = kW - 10;
    for (const Indicator& ind : indicators) {
        const int width = _canvas.textWidth(ind.label) + 14;
        x -= width;
        _canvas.fillCircle(x + 4, kHeaderH / 2, 3, ind.on ? ind.color : kDotOff);
        _canvas.setTextColor(ind.on ? kText : kTextDim, kPanel);
        _canvas.drawString(ind.label, x + 12, kHeaderH / 2);
        x -= 10;
    }
}

void Ui::drawList(const UiModel& model, uint32_t now) {
    const int count = model.scripts ? static_cast<int>(model.scripts->size()) : 0;
    if (!model.storageOk) {
        _canvas.setTextDatum(textdatum_t::middle_center);
        _canvas.setTextColor(kRed, kBg);
        _canvas.drawString("Storage failed to mount", kW / 2, kListY + kListH / 2 - 10);
        _canvas.setTextColor(kTextDim, kBg);
        _canvas.drawString("Re-flash the firmware", kW / 2, kListY + kListH / 2 + 10);
        return;
    }
    if (count == 0) {
        _canvas.setTextDatum(textdatum_t::middle_center);
        _canvas.setTextColor(kText, kBg);
        _canvas.drawString("No macros yet", kW / 2, kListY + kListH / 2 - 10);
        _canvas.setTextColor(kTextDim, kBg);
        _canvas.drawString("Open the web manager over USB", kW / 2, kListY + kListH / 2 + 10);
        return;
    }

    const bool blinkOn = (now / 500) % 2 == 0;
    for (int i = 0; i < kRows; ++i) {
        const int idx = _scroll + i;
        if (idx >= count) break;
        const ScriptInfo& script = (*model.scripts)[idx];
        const int y = kListY + i * kRowH;
        const bool selected = idx == model.selected;
        const bool runningHere = model.status.state == MacroState::Running && model.status.script == script.name;
        const uint32_t rowBg = selected ? kRowSel : kBg;

        if (selected) {
            _canvas.fillRect(0, y, kW, kRowH, kRowSel);
            _canvas.fillRect(0, y, 4, kRowH, kAccent);
        } else {
            _canvas.drawFastHLine(12, y + kRowH - 1, kW - 24, kLine);
        }

        _canvas.setTextDatum(textdatum_t::middle_right);
        _canvas.setTextColor(selected ? kAccent : kTextDim, rowBg);
        _canvas.drawString(String(idx + 1), 30, y + kRowH / 2);

        const int nameMax = runningHere ? kW - 42 - 64 : kW - 42 - 14;
        _canvas.setTextDatum(textdatum_t::middle_left);
        _canvas.setTextColor(selected ? kTextBright : kText, rowBg);
        _canvas.drawString(fitText(script.name, nameMax), 42, y + kRowH / 2);

        if (runningHere) {
            const int px = kW - 60;
            const int py = y + (kRowH - 20) / 2;
            _canvas.fillRoundRect(px, py, 48, 20, 5, kGreenBg);
            _canvas.fillCircle(px + 9, py + 10, 3, blinkOn ? kGreen : kGreenBg);
            _canvas.setTextColor(kGreen, kGreenBg);
            _canvas.drawString("RUN", px + 17, py + 10);
        }
    }

    if (count > kRows) {
        const int trackX = kW - 5;
        const int trackY = kListY + 4;
        const int trackH = kRows * kRowH - 8;
        _canvas.fillRect(trackX, trackY, 3, trackH, kLine);
        int thumbH = trackH * kRows / count;
        if (thumbH < 12) thumbH = 12;
        const int thumbY = trackY + (trackH - thumbH) * _scroll / (count - kRows);
        _canvas.fillRect(trackX, thumbY, 3, thumbH, kTextDim);
    }
}

void Ui::drawStatus(const UiModel& model, uint32_t now) {
    const MacroStatus& s = model.status;
    uint32_t bg = kPanel;
    if (s.state == MacroState::Running) bg = kGreenBg;
    if (s.state == MacroState::Error) bg = kRedBg;
    _canvas.fillRect(0, kStatusY, kW, kStatusH, bg);
    _canvas.drawFastHLine(0, kStatusY, kW, kLine);

    const int line1 = kStatusY + 12;
    const int line2 = kStatusY + 28;
    _canvas.setTextDatum(textdatum_t::middle_left);

    if (model.bindMode && model.setup && model.setup->open) {
        if (model.setup->editing) {
            const bool macroEdit = model.setup->group == Bindings::kMacroGroup;
            const String combo = macroEdit ? String("run macro, press again to stop") : Bindings::compose(model.setup->group, model.setup->keyIndex, model.setup->modIndex);
            String preview = combo.length() ? combo : String("unbound");
            if (combo.length() && !macroEdit) {
                preview += "   ";
                preview += model.setup->behavior == BindBehavior::Normal ? "hold" : Bindings::behaviorName(model.setup->behavior);
                if (model.setup->behavior == BindBehavior::Burst || model.setup->behavior == BindBehavior::ToggleBurst) preview += " " + String(model.setup->interval) + "ms";
            }
            _canvas.setTextColor(kAccent, bg);
            _canvas.drawString(preview, 12, line1);
            _canvas.setTextColor(kTextDim, bg);
            _canvas.drawString("A saves this button, B cancels", 12, line2);
        } else {
            _canvas.setTextColor(kText, bg);
            _canvas.drawString("Bind setup", 12, line1);
            _canvas.setTextColor(kTextDim, bg);
            _canvas.drawString("Pick a button, A edits it", 12, line2);
        }
        return;
    }
    if (model.bindMode) {
        _canvas.setTextColor(kText, bg);
        _canvas.drawString("Gamepad acts as a keyboard", 12, line1);
        _canvas.setTextColor(kTextDim, bg);
        _canvas.drawString(model.ble ? "START setup   hold START: USB" : "START setup   hold START: Bluetooth", 12, line2);
        return;
    }

    switch (s.state) {
        case MacroState::Running: {
            const bool blinkOn = (now / 500) % 2 == 0;
            _canvas.fillCircle(14, line1, 4, blinkOn ? kGreen : bg);
            _canvas.setTextColor(kTextBright, bg);
            _canvas.drawString(fitText(s.script, kW - 100), 26, line1);
            String detail = "line " + String(s.line) + "/" + String(s.totalLines);
            if (s.loopDepth > 0) {
                detail += "   loop " + String(s.loopIteration);
                if (s.loopCount > 0) detail += "/" + String(s.loopCount);
            }
            _canvas.setTextColor(kTextDim, bg);
            _canvas.drawString(detail, 26, line2);
            _canvas.setTextDatum(textdatum_t::middle_right);
            _canvas.setTextColor(kGreen, bg);
            _canvas.drawString(formatElapsed(now - s.startedMs), kW - 12, line1);
            break;
        }
        case MacroState::Finished:
            _canvas.setTextColor(kGreen, bg);
            _canvas.drawString("Done", 12, line1);
            _canvas.setTextColor(kText, bg);
            _canvas.drawString(fitText(s.script, kW - 120), 56, line1);
            _canvas.setTextColor(kTextDim, bg);
            _canvas.drawString("took " + formatElapsed(s.finishedMs - s.startedMs) + "   press A to run again", 12, line2);
            break;
        case MacroState::Stopped:
            _canvas.setTextColor(kAccent, bg);
            _canvas.drawString("Stopped", 12, line1);
            _canvas.setTextColor(kText, bg);
            _canvas.drawString(fitText(s.script, kW - 120), 80, line1);
            _canvas.setTextColor(kTextDim, bg);
            _canvas.drawString("at line " + String(s.line) + "   press A to run again", 12, line2);
            break;
        case MacroState::Error:
            _canvas.setTextColor(kRed, bg);
            _canvas.drawString("Error in " + fitText(s.script, 120) + ", line " + String(s.line), 12, line1);
            _canvas.setTextColor(kText, bg);
            _canvas.drawString(fitText(s.error, kW - 24), 12, line2);
            break;
        case MacroState::Idle:
        default:
            _canvas.setTextColor(kText, bg);
            _canvas.drawString("Pick a macro, press A to run it", 12, line1);
            _canvas.setTextColor(kTextDim, bg);
            if (model.ble) _canvas.drawString(model.outputReady ? "Keys go over Bluetooth" : "Bluetooth: pair Core Macro on the computer", 12, line2);
            else _canvas.drawString(model.usb ? "Keys go to the computer on USB" : "Plug USB into a computer first", 12, line2);
            break;
    }
}

int Ui::drawKeycap(const char* label, int x, int cy) {
    int width = _canvas.textWidth(label) + 10;
    if (width < 18) width = 18;
    _canvas.fillRoundRect(x, cy - 9, width, 18, 4, kKeycap);
    _canvas.setTextDatum(textdatum_t::middle_center);
    _canvas.setTextColor(kTextBright, kKeycap);
    _canvas.drawString(label, x + width / 2, cy);
    return width;
}

int Ui::drawHint(const char* cap, const char* text, int x, int cy) {
    x += drawKeycap(cap, x, cy) + 6;
    _canvas.setTextDatum(textdatum_t::middle_left);
    _canvas.setTextColor(kTextDim, kPanel);
    _canvas.drawString(text, x, cy);
    return x + _canvas.textWidth(text) + 14;
}

int Ui::drawArrowHint(bool vertical, const char* text, int x, int cy) {
    _canvas.fillRoundRect(x, cy - 9, 18, 18, 4, kKeycap);
    if (vertical) {
        _canvas.fillTriangle(x + 5, cy - 1, x + 13, cy - 1, x + 9, cy - 6, kTextBright);
        _canvas.fillTriangle(x + 5, cy + 2, x + 13, cy + 2, x + 9, cy + 7, kTextBright);
    } else {
        _canvas.fillTriangle(x + 8, cy - 4, x + 8, cy + 4, x + 3, cy, kTextBright);
        _canvas.fillTriangle(x + 10, cy - 4, x + 10, cy + 4, x + 15, cy, kTextBright);
    }
    x += 24;
    _canvas.setTextDatum(textdatum_t::middle_left);
    _canvas.setTextColor(kTextDim, kPanel);
    _canvas.drawString(text, x, cy);
    return x + _canvas.textWidth(text) + 14;
}

void Ui::drawBinds(const UiModel& model) {
    constexpr int rowH = 24;
    for (int i = 0; i < Bindings::kSlots; ++i) {
        const int y = kListY + i * rowH;
        const uint8_t bit = Bindings::buttonMask(i);
        const bool pressed = (model.padPressed & bit) != 0;
        const bool toggled = (model.padToggled & bit) != 0;
        const uint32_t rowBg = pressed ? kGreenBg : kBg;
        if (pressed) _canvas.fillRect(0, y, kW, rowH, kGreenBg);
        else _canvas.drawFastHLine(12, y + rowH - 1, kW - 24, kLine);

        String label(Bindings::buttonName(i));
        label.toUpperCase();
        _canvas.setTextDatum(textdatum_t::middle_left);
        _canvas.setTextColor(kAccent, rowBg);
        _canvas.drawString(label, 10, y + rowH / 2);

        const Binding* b = model.binds ? &model.binds->slot(i) : nullptr;
        const bool macro = b && b->script.length() > 0;
        const bool bound = macro || (b && b->codeCount > 0);
        _canvas.setTextColor(bound ? kText : kTextDim, rowBg);
        _canvas.drawString(bound ? fitText(macro ? "run " + b->script : b->keys, 150) : String("unbound"), 70, y + rowH / 2);

        if (bound) {
            String tag = macro ? "macro" : (b->behavior == BindBehavior::Normal ? "hold" : Bindings::behaviorName(b->behavior));
            if (toggled) tag += " ON";
            _canvas.setTextDatum(textdatum_t::middle_right);
            _canvas.setTextColor(toggled ? kGreen : kTextDim, rowBg);
            _canvas.drawString(tag, kW - 12, y + rowH / 2);
        }
    }
}

void Ui::drawSetupList(const UiModel& model) {
    constexpr int rowH = 24;
    for (int i = 0; i < Bindings::kSlots; ++i) {
        const int y = kListY + i * rowH;
        const bool selected = model.setup->row == i;
        const uint32_t rowBg = selected ? kRowSel : kBg;
        if (selected) {
            _canvas.fillRect(0, y, kW, rowH, kRowSel);
            _canvas.fillRect(0, y, 4, rowH, kAccent);
        } else {
            _canvas.drawFastHLine(12, y + rowH - 1, kW - 24, kLine);
        }
        String label(Bindings::buttonName(i));
        label.toUpperCase();
        _canvas.setTextDatum(textdatum_t::middle_left);
        _canvas.setTextColor(kAccent, rowBg);
        _canvas.drawString(label, 10, y + rowH / 2);
        const Binding& b = model.binds->slot(i);
        const bool macro = b.script.length() > 0;
        const bool bound = macro || b.codeCount > 0;
        _canvas.setTextColor(bound ? (selected ? kTextBright : kText) : kTextDim, rowBg);
        _canvas.drawString(bound ? fitText(macro ? "run " + b.script : b.keys, 150) : String("unbound"), 70, y + rowH / 2);
        if (bound) {
            String tag = macro ? "macro" : (b.behavior == BindBehavior::Normal ? "hold" : Bindings::behaviorName(b.behavior));
            if (b.behavior == BindBehavior::Burst || b.behavior == BindBehavior::ToggleBurst) tag += " " + String(b.intervalMs) + "ms";
            _canvas.setTextDatum(textdatum_t::middle_right);
            _canvas.setTextColor(kTextDim, rowBg);
            _canvas.drawString(tag, kW - 12, y + rowH / 2);
        }
    }
}

void Ui::drawSetupEdit(const UiModel& model) {
    const BindSetup& s = *model.setup;
    String title(Bindings::buttonName(s.row));
    title.toUpperCase();
    _canvas.setTextDatum(textdatum_t::middle_left);
    _canvas.setTextColor(kAccent, kBg);
    _canvas.drawString("Edit " + title, 12, kListY + 12);

    struct Field {
        const char* label;
        String value;
        bool enabled;
    };
    const char* keyName = Bindings::groupKey(s.group, s.keyIndex);
    const char* modName = Bindings::modifierName(s.modIndex);
    const bool hasGroup = s.group != 0;
    const bool macro = s.group == Bindings::kMacroGroup;
    const int scriptCount = model.scripts ? static_cast<int>(model.scripts->size()) : 0;
    String scriptName = scriptCount == 0 ? String("no macros") : (*model.scripts)[s.keyIndex < scriptCount ? s.keyIndex : 0].name;
    const Field fields[Bindings::kSetupFields] = {
        {"Group", String(Bindings::groupName(s.group)), true},
        {macro ? "Script" : "Key", macro ? fitText(scriptName, 170) : (hasGroup ? String(keyName) : String("-")), hasGroup},
        {"Modifier", macro ? String("-") : (modName[0] ? String(modName) : String("none")), !macro},
        {"Behavior", macro ? String("toggle") : (s.behavior == BindBehavior::Normal ? String("hold") : String(Bindings::behaviorName(s.behavior))), !macro},
        {"Interval", String(s.interval) + " ms", !macro && (s.behavior == BindBehavior::Burst || s.behavior == BindBehavior::ToggleBurst)},
    };
    constexpr int rowH = 24;
    for (int i = 0; i < Bindings::kSetupFields; ++i) {
        const int y = kListY + 24 + i * rowH;
        const bool selected = s.field == i && fields[i].enabled;
        const uint32_t rowBg = selected ? kRowSel : kBg;
        if (selected) {
            _canvas.fillRect(0, y, kW, rowH, kRowSel);
            _canvas.fillRect(0, y, 4, rowH, kAccent);
        }
        _canvas.setTextDatum(textdatum_t::middle_left);
        _canvas.setTextColor(fields[i].enabled ? kTextDim : kDotOff, rowBg);
        _canvas.drawString(fields[i].label, 14, y + rowH / 2);
        _canvas.setTextColor(fields[i].enabled ? (selected ? kTextBright : kText) : kDotOff, rowBg);
        if (selected) {
            _canvas.fillTriangle(104, y + rowH / 2, 110, y + rowH / 2 - 5, 110, y + rowH / 2 + 5, kAccent);
            _canvas.drawString(fields[i].value, 118, y + rowH / 2);
            const int vx = 118 + _canvas.textWidth(fields[i].value) + 8;
            _canvas.fillTriangle(vx + 6, y + rowH / 2, vx, y + rowH / 2 - 5, vx, y + rowH / 2 + 5, kAccent);
        } else {
            _canvas.drawString(fields[i].value, 118, y + rowH / 2);
        }
    }
}

void Ui::drawFooter(const UiModel& model) {
    _canvas.fillRect(0, kFooterY, kW, kFooterH, kPanel);
    _canvas.drawFastHLine(0, kFooterY, kW, kLine);
    const int cy = kFooterY + kFooterH / 2;

    if (!model.gamepad) {
        _canvas.setTextDatum(textdatum_t::middle_left);
        _canvas.setTextColor(kRed, kPanel);
        _canvas.drawString("Gamepad not found", 10, cy);
        _canvas.setTextDatum(textdatum_t::middle_right);
        _canvas.setTextColor(kTextDim, kPanel);
        _canvas.drawString("tap a row twice to run", kW - 10, cy);
        return;
    }

    int x = 10;
    if (model.bindMode && model.setup && model.setup->open) {
        if (model.setup->editing) {
            x = drawHint("A", "save", x, cy);
            x = drawHint("B", "cancel", x, cy);
            x = drawArrowHint(false, "change", x, cy);
            drawArrowHint(true, "field", x, cy);
        } else {
            x = drawHint("A", "edit", x, cy);
            x = drawHint("B", "back", x, cy);
            drawArrowHint(true, "button", x, cy);
        }
        return;
    }
    if (model.bindMode) {
        x = drawHint("START", "setup", x, cy);
        drawHint("SEL", "macros", x, cy);
        return;
    }
    x = drawHint("A", "run", x, cy);
    x = drawHint("B", "stop", x, cy);

    x = drawArrowHint(true, "pick", x, cy);
    drawHint("SEL", "bind", x, cy);
}
