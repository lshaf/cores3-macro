#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <vector>

enum class MacroState : uint8_t { Idle, Running, Finished, Stopped, Error };

struct MacroStatus {
    MacroState state = MacroState::Idle;
    String script;
    String error;
    uint16_t line = 0;
    uint16_t totalLines = 0;
    int32_t loopIteration = 0;
    int32_t loopCount = 0;
    uint8_t loopDepth = 0;
    uint32_t startedMs = 0;
    uint32_t finishedMs = 0;
    uint32_t generation = 0;
};

struct ParseError {
    uint16_t line = 0;
    String message;
};

class MacroRunner {
public:
    void begin();
    static bool check(const String& source, ParseError& err);
    bool start(const String& name, const String& source, ParseError& err);
    void stop();
    bool running() const { return _taskAlive; }
    MacroStatus snapshot() const;

private:
    enum class Op : uint8_t {
        Delay,
        Text,
        KeyTap,
        KeyHold,
        KeyRelease,
        ReleaseAll,
        LoopStart,
        LoopEnd,
        MouseMove,
        MouseScroll,
        MousePress,
        MouseRelease,
        MouseClick,
        Consumer,
        TypeDelay,
        DefaultDelay,
    };

    struct Instr {
        Op op = Op::Delay;
        uint16_t line = 0;
        int32_t a = 0;
        int32_t b = 0;
        uint8_t keys[8] = {};
        uint8_t keyCount = 0;
        String text;
    };

    struct Frame {
        uint16_t startIndex;
        int32_t remaining;
        int32_t total;
        int32_t iteration;
    };

    static bool compile(const String& source, std::vector<Instr>& out, ParseError& err);
    static bool parseKeys(const String& arg, Instr& instr, String& err);
    static uint16_t countLines(const String& source);
    static void taskEntry(void* self);
    void run();
    void updateProgress(uint16_t line, const std::vector<Frame>& frames);
    void finish(MacroState state);
    bool sleepInterruptible(uint32_t ms);

    std::vector<Instr> _program;
    MacroStatus _status;
    SemaphoreHandle_t _lock = nullptr;
    TaskHandle_t _task = nullptr;
    volatile bool _stopRequested = false;
    volatile bool _taskAlive = false;
    uint32_t _typeDelayMs = 0;
    uint32_t _defaultDelayMs = 0;
};
