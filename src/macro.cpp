#include "macro.h"

#include <ctype.h>
#include <stdlib.h>

#include "config.h"
#include "hid.h"

namespace {

bool fail(ParseError& err, uint16_t line, const String& message) {
    err.line = line;
    err.message = message;
    return false;
}

bool parseInt(const String& text, int32_t& out) {
    if (text.length() == 0) return false;
    char* end = nullptr;
    const long value = strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') return false;
    out = static_cast<int32_t>(value);
    return true;
}

bool parseDuration(const String& raw, int32_t& ms) {
    String s = raw;
    s.trim();
    s.toLowerCase();
    float scale = 1.0f;
    if (s.endsWith("ms")) {
        s.remove(s.length() - 2);
    } else if (s.endsWith("s")) {
        s.remove(s.length() - 1);
        scale = 1000.0f;
    }
    s.trim();
    if (s.length() == 0) return false;
    char* end = nullptr;
    const float value = strtof(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || value < 0) return false;
    ms = static_cast<int32_t>(value * scale + 0.5f);
    return true;
}

bool parsePair(const String& text, int32_t& a, int32_t& b) {
    String s = text;
    s.replace(',', ' ');
    s.trim();
    const int sp = s.indexOf(' ');
    if (sp < 0) return false;
    String first = s.substring(0, sp);
    String second = s.substring(sp + 1);
    first.trim();
    second.trim();
    return parseInt(first, a) && parseInt(second, b);
}

bool isComment(const String& trimmed) {
    if (trimmed.startsWith("#") || trimmed.startsWith("//")) return true;
    String head = trimmed.substring(0, 4);
    head.toUpperCase();
    return head == "REM" || head == "REM ";
}

}

void MacroRunner::begin() {
    if (_lock == nullptr) _lock = xSemaphoreCreateMutex();
}

uint16_t MacroRunner::countLines(const String& source) {
    uint16_t count = source.length() > 0 ? 1 : 0;
    for (size_t i = 0; i < source.length(); ++i) {
        if (source[i] == '\n' && i + 1 < source.length()) ++count;
    }
    return count;
}

bool MacroRunner::parseKeys(const String& arg, Instr& instr, String& err) {
    return hid::parseCombo(arg, instr.keys, sizeof(instr.keys), instr.keyCount, err);
}

bool MacroRunner::compile(const String& source, std::vector<Instr>& out, ParseError& err) {
    out.clear();
    std::vector<uint16_t> loopStack;
    uint16_t lineNo = 0;
    size_t pos = 0;

    while (pos <= source.length()) {
        const int nl = source.indexOf('\n', pos);
        String line = nl < 0 ? source.substring(pos) : source.substring(pos, nl);
        pos = nl < 0 ? source.length() + 1 : static_cast<size_t>(nl) + 1;
        ++lineNo;
        line.replace("\r", "");

        String trimmed = line;
        trimmed.trim();
        if (trimmed.length() == 0 || isComment(trimmed)) continue;

        int sp = -1;
        for (size_t i = 0; i < trimmed.length(); ++i) {
            if (trimmed[i] == ' ' || trimmed[i] == '\t') {
                sp = static_cast<int>(i);
                break;
            }
        }
        String cmd = sp < 0 ? trimmed : trimmed.substring(0, sp);
        String arg = sp < 0 ? String() : trimmed.substring(sp + 1);
        String argTrim = arg;
        argTrim.trim();
        cmd.toUpperCase();

        Instr in;
        in.line = lineNo;
        String keyErr;

        if (cmd == "DELAY" || cmd == "SLEEP" || cmd == "WAIT") {
            if (!parseDuration(argTrim, in.a)) return fail(err, lineNo, "DELAY needs a time like 500, 250ms or 1.5s");
            in.op = Op::Delay;
        } else if (cmd == "STRING" || cmd == "TYPE" || cmd == "TEXT") {
            in.op = Op::Text;
            in.a = 0;
            in.text = arg;
        } else if (cmd == "STRINGLN" || cmd == "TYPELN" || cmd == "TEXTLN") {
            in.op = Op::Text;
            in.a = 1;
            in.text = arg;
        } else if (cmd == "KEY" || cmd == "TAP" || cmd == "PRESS") {
            if (!parseKeys(argTrim, in, keyErr)) return fail(err, lineNo, keyErr);
            in.op = Op::KeyTap;
        } else if (cmd == "HOLD" || cmd == "KEYDOWN") {
            if (!parseKeys(argTrim, in, keyErr)) return fail(err, lineNo, keyErr);
            in.op = Op::KeyHold;
        } else if (cmd == "RELEASE" || cmd == "KEYUP") {
            if (argTrim.length() == 0) {
                in.op = Op::ReleaseAll;
            } else {
                if (!parseKeys(argTrim, in, keyErr)) return fail(err, lineNo, keyErr);
                in.op = Op::KeyRelease;
            }
        } else if (cmd == "LOOP" || cmd == "REPEAT") {
            in.op = Op::LoopStart;
            in.a = -1;
            String count = argTrim;
            count.toLowerCase();
            if (count.length() > 0 && count != "forever" && count != "inf" && count != "infinite" && count != "always") {
                if (!parseInt(count, in.a) || in.a < 0) return fail(err, lineNo, "LOOP needs a count, or nothing to loop forever");
            }
            loopStack.push_back(static_cast<uint16_t>(out.size()));
        } else if (cmd == "END" || cmd == "ENDLOOP" || cmd == "DONE") {
            if (loopStack.empty()) return fail(err, lineNo, "END without a matching LOOP");
            const uint16_t start = loopStack.back();
            loopStack.pop_back();
            in.op = Op::LoopEnd;
            in.b = start;
            out[start].b = static_cast<int32_t>(out.size());
        } else if (cmd == "CLICK") {
            uint8_t button = 0;
            if (!hid::mouseButtonFor(argTrim.length() ? argTrim.c_str() : "left", button)) return fail(err, lineNo, "CLICK needs left, right or middle");
            in.op = Op::MouseClick;
            in.a = button;
        } else if (cmd == "MOUSE_PRESS" || cmd == "MOUSEDOWN") {
            uint8_t button = 0;
            if (!hid::mouseButtonFor(argTrim.length() ? argTrim.c_str() : "left", button)) return fail(err, lineNo, "MOUSE_PRESS needs left, right or middle");
            in.op = Op::MousePress;
            in.a = button;
        } else if (cmd == "MOUSE_RELEASE" || cmd == "MOUSEUP") {
            uint8_t button = 0;
            if (!hid::mouseButtonFor(argTrim.length() ? argTrim.c_str() : "left", button)) return fail(err, lineNo, "MOUSE_RELEASE needs left, right or middle");
            in.op = Op::MouseRelease;
            in.a = button;
        } else if (cmd == "MOUSE" || cmd == "MOUSE_MOVE" || cmd == "MOVE") {
            if (!parsePair(argTrim, in.a, in.b)) return fail(err, lineNo, "MOUSE needs two numbers: dx dy");
            in.op = Op::MouseMove;
        } else if (cmd == "SCROLL" || cmd == "WHEEL") {
            if (!parseInt(argTrim, in.a)) return fail(err, lineNo, "SCROLL needs a number (negative scrolls down)");
            in.op = Op::MouseScroll;
        } else if (cmd == "MEDIA" || cmd == "CONSUMER") {
            uint16_t usage = 0;
            if (!hid::consumerCodeFor(argTrim.c_str(), usage)) return fail(err, lineNo, "Unknown media key: " + argTrim);
            in.op = Op::Consumer;
            in.a = usage;
        } else if (cmd == "TYPE_DELAY" || cmd == "TYPEDELAY" || cmd == "STRING_DELAY") {
            if (!parseDuration(argTrim, in.a)) return fail(err, lineNo, "TYPE_DELAY needs a time in ms");
            in.op = Op::TypeDelay;
        } else if (cmd == "DEFAULT_DELAY" || cmd == "DEFAULTDELAY") {
            if (!parseDuration(argTrim, in.a)) return fail(err, lineNo, "DEFAULT_DELAY needs a time in ms");
            in.op = Op::DefaultDelay;
        } else {
            if (!parseKeys(trimmed, in, keyErr)) return fail(err, lineNo, "Unknown command: " + cmd);
            in.op = Op::KeyTap;
        }
        out.push_back(in);
    }

    if (!loopStack.empty()) return fail(err, out[loopStack.back()].line, "LOOP without a matching END");
    return true;
}

bool MacroRunner::check(const String& source, ParseError& err) {
    std::vector<Instr> program;
    return compile(source, program, err);
}

bool MacroRunner::start(const String& name, const String& source, ParseError& err) {
    std::vector<Instr> program;
    stop();
    if (!compile(source, program, err)) {
        xSemaphoreTake(_lock, portMAX_DELAY);
        _status.state = MacroState::Error;
        _status.script = name;
        _status.error = err.message;
        _status.line = err.line;
        _status.totalLines = countLines(source);
        _status.loopIteration = 0;
        _status.loopCount = 0;
        _status.loopDepth = 0;
        _status.startedMs = millis();
        _status.finishedMs = _status.startedMs;
        ++_status.generation;
        xSemaphoreGive(_lock);
        return false;
    }

    _program = std::move(program);
    _stopRequested = false;
    _typeDelayMs = 0;
    _defaultDelayMs = 0;

    xSemaphoreTake(_lock, portMAX_DELAY);
    _status.state = MacroState::Running;
    _status.script = name;
    _status.error = "";
    _status.line = 0;
    _status.totalLines = countLines(source);
    _status.loopIteration = 0;
    _status.loopCount = 0;
    _status.loopDepth = 0;
    _status.startedMs = millis();
    _status.finishedMs = 0;
    ++_status.generation;
    xSemaphoreGive(_lock);

    _taskAlive = true;
    if (xTaskCreatePinnedToCore(taskEntry, "macro", 12288, this, 1, &_task, 0) != pdPASS) {
        _taskAlive = false;
        _task = nullptr;
        err.line = 0;
        err.message = "Could not start macro task";
        finish(MacroState::Error);
        xSemaphoreTake(_lock, portMAX_DELAY);
        _status.error = err.message;
        xSemaphoreGive(_lock);
        return false;
    }
    return true;
}

void MacroRunner::stop() {
    if (!_taskAlive) return;
    _stopRequested = true;
    const uint32_t started = millis();
    while (_taskAlive && millis() - started < 3000) vTaskDelay(pdMS_TO_TICKS(5));
    if (_taskAlive) {
        TaskHandle_t stale = _task;
        _task = nullptr;
        _taskAlive = false;
        if (stale != nullptr) vTaskDelete(stale);
        if (xSemaphoreGetMutexHolder(_lock) == stale) {
            vSemaphoreDelete(_lock);
            _lock = xSemaphoreCreateMutex();
        }
        finish(MacroState::Stopped);
    }
    hid::releaseAll();
}

MacroStatus MacroRunner::snapshot() const {
    xSemaphoreTake(_lock, portMAX_DELAY);
    MacroStatus copy = _status;
    xSemaphoreGive(_lock);
    return copy;
}

void MacroRunner::taskEntry(void* self) {
    auto* runner = static_cast<MacroRunner*>(self);
    runner->run();
    runner->_task = nullptr;
    runner->_taskAlive = false;
    vTaskDelete(nullptr);
}

void MacroRunner::updateProgress(uint16_t line, const std::vector<Frame>& frames) {
    xSemaphoreTake(_lock, portMAX_DELAY);
    _status.line = line;
    if (frames.empty()) {
        _status.loopDepth = 0;
        _status.loopIteration = 0;
        _status.loopCount = 0;
    } else {
        const Frame& top = frames.back();
        _status.loopDepth = static_cast<uint8_t>(frames.size());
        _status.loopIteration = top.iteration;
        _status.loopCount = top.total;
    }
    xSemaphoreGive(_lock);
}

void MacroRunner::finish(MacroState state) {
    xSemaphoreTake(_lock, portMAX_DELAY);
    _status.state = state;
    _status.finishedMs = millis();
    ++_status.generation;
    xSemaphoreGive(_lock);
}

bool MacroRunner::sleepInterruptible(uint32_t ms) {
    const uint32_t started = millis();
    while (!_stopRequested) {
        const uint32_t elapsed = millis() - started;
        if (elapsed >= ms) return true;
        const uint32_t left = ms - elapsed;
        vTaskDelay(pdMS_TO_TICKS(left < 20 ? left : 20));
    }
    return false;
}

void MacroRunner::run() {
    std::vector<Frame> frames;
    size_t pc = 0;
    const size_t count = _program.size();

    while (pc < count && !_stopRequested) {
        const Instr& in = _program[pc];
        updateProgress(in.line, frames);

        switch (in.op) {
            case Op::Delay:
                sleepInterruptible(static_cast<uint32_t>(in.a));
                break;
            case Op::Text:
                for (size_t i = 0; i < in.text.length() && !_stopRequested; ++i) {
                    hid::typeChar(in.text[i]);
                    if (_typeDelayMs) sleepInterruptible(_typeDelayMs);
                }
                if (in.a == 1 && !_stopRequested) hid::typeChar('\n');
                break;
            case Op::KeyTap:
                for (uint8_t i = 0; i < in.keyCount; ++i) hid::pressKey(in.keys[i]);
                vTaskDelay(pdMS_TO_TICKS(cfg::kKeyTapHoldMs));
                for (uint8_t i = in.keyCount; i-- > 0;) hid::releaseKey(in.keys[i]);
                break;
            case Op::KeyHold:
                for (uint8_t i = 0; i < in.keyCount; ++i) hid::pressKey(in.keys[i]);
                break;
            case Op::KeyRelease:
                for (uint8_t i = in.keyCount; i-- > 0;) hid::releaseKey(in.keys[i]);
                break;
            case Op::ReleaseAll:
                hid::releaseAll();
                break;
            case Op::LoopStart:
                if (in.a == 0) {
                    pc = static_cast<size_t>(in.b) + 1;
                    continue;
                }
                frames.push_back({static_cast<uint16_t>(pc), in.a, in.a, 1});
                break;
            case Op::LoopEnd: {
                if (frames.empty()) break;
                Frame& frame = frames.back();
                const bool again = frame.remaining < 0 || --frame.remaining > 0;
                if (again) {
                    ++frame.iteration;
                    pc = static_cast<size_t>(frame.startIndex) + 1;
                    vTaskDelay(1);
                    continue;
                }
                frames.pop_back();
                break;
            }
            case Op::MouseMove:
                hid::mouseMove(in.a, in.b);
                break;
            case Op::MouseScroll:
                hid::mouseScroll(in.a);
                break;
            case Op::MousePress:
                hid::mousePress(static_cast<uint8_t>(in.a));
                break;
            case Op::MouseRelease:
                hid::mouseRelease(static_cast<uint8_t>(in.a));
                break;
            case Op::MouseClick:
                hid::mouseClick(static_cast<uint8_t>(in.a));
                break;
            case Op::Consumer:
                hid::consumerTap(static_cast<uint16_t>(in.a));
                break;
            case Op::TypeDelay:
                _typeDelayMs = static_cast<uint32_t>(in.a);
                break;
            case Op::DefaultDelay:
                _defaultDelayMs = static_cast<uint32_t>(in.a);
                break;
        }

        ++pc;
        if (_defaultDelayMs && in.op != Op::LoopStart && in.op != Op::LoopEnd) sleepInterruptible(_defaultDelayMs);
    }

    hid::releaseAll();
    finish(_stopRequested ? MacroState::Stopped : MacroState::Finished);
}
