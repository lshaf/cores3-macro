# Core Macro

USB macro pad firmware for the M5Stack CoreS3 with a Faces Gamepad3 controller. The device acts as a USB keyboard and mouse. Scripts live on the device's flash and are written and managed from a browser page over the same USB cable (Web Serial). On the device you pick a macro with the d-pad, press **A** to run it, **B** to stop it.

## Hardware

- M5Stack CoreS3 (ESP32-S3, 16 MB flash, PSRAM)
- M5Stack Faces Gamepad3 (I2C `0x08` on the internal bus, pins G12/G11)
- USB-C cable to the computer that should receive the keystrokes

## Downloads

- Web manager, hosted: https://lshaf.github.io/cores3-macro/ (Chrome or Edge, click Connect).
- Firmware binaries: every push to `main` builds them as a workflow artifact; tags `v*` publish a GitHub release with `core-macro-full.bin` (flash at 0x0), the separate parts, and `littlefs.bin` with the sample macros.

Flash a release without PlatformIO:

```sh
pip install esptool
esptool --chip esp32s3 write-flash 0x0 core-macro-full.bin
```

## Build and flash

PlatformIO with the pioarduino platform (Arduino core 3.2.1). The toolchain is pinned in `platformio.ini`.

```sh
pio run -e m5stack-cores3 -t upload
pio run -e m5stack-cores3 -t uploadfs
```

The second command is optional: it writes the sample macros from `data/macros/` to the device. It replaces everything on the storage partition, so run it only on a fresh device.

The firmware enumerates as a composite USB device: a CDC serial port plus HID keyboard, mouse and media keys. That TinyUSB port does not react to esptool's usual reset, so `scripts/touch1200.py` runs before every upload: it opens the port at 1200 baud, which makes the running firmware reboot into download mode, then flashes on the port that appears. Close the web page (it holds the port) before uploading. If the touch fails, hold the reset button on the CoreS3 for about three seconds to enter download mode and run the upload again.

## Web manager

Open `web/index.html` in Chrome or Edge (desktop) and click **Connect**, then choose the CoreS3 serial port. From there you can create, edit, rename, delete, run and stop macros, see the live device state (a mirror of the device screen), and change the screen-off timeout and brightness.

The page is a single file with no build step. It talks newline-delimited JSON over the serial port; the protocol is described below.

## On the device

- **Up / Down** move the highlight, **Left / Right** jump a page.
- **A** runs the highlighted macro. **B** stops whatever is running.
- The touch screen works too: tap a row to highlight it, tap it again to run, tap the status card to stop.
- The screen turns off after 30 seconds without a button press (configurable from the web page). Macros keep running with the screen off. The first press after that only wakes the screen.
- Everything held by a macro (keys, mouse buttons) is released when it ends or is stopped.

## Device menu

Hold **Select** for about a second to open the menu: Mode (macro / bind), Output (USB / Bluetooth), Screen off, Brightness. Up/Down picks a row, Left/Right changes it, B closes. A short Select press still flips the mode directly.

## Output: USB or Bluetooth

Keys go out over USB by default. Switch to Bluetooth LE in the device menu or from the web page settings. In Bluetooth mode the device advertises as "Core Macro"; pair it from the computer or phone like any BLE keyboard. The header dot reads USB or BLE and lights when the link is ready. The choice is remembered across reboots. The serial port and web page keep working over USB in both modes.

## Bind mode

Press **Select** on the gamepad to switch between Macro mode and Bind mode (the header shows a BIND tag). In Bind mode the gamepad is a keyboard: each of Up, Down, Left, Right, A and B sends one key combo. Start and Select are never bound. Each binding has a behavior:

- **Hold** (`normal`): the key is pressed while the button is held.
- **Burst**: the key is tapped repeatedly every N ms while the button is held.
- **Toggle**: one press holds the key, the next press releases it.
- **Toggle burst** (`toggleburst`): one press starts tapping the key every N ms, the next press stops it.
- **Macro** (`macro` with a `script` name): one press runs that macro, the next press stops it. Other buttons keep sending their keys while it runs.

Entering Bind mode stops any running macro and releases all keys; leaving it releases everything again. The mode survives a reboot. Bindings are stored in `/binds.json`.

Bindings can be set from the web page (Bindings section) or on the device: in Bind mode press **Start** to open setup. Hold **Start** to open presets: the first row saves the current bindings as a new preset (named Preset N, rename it from the web page), A loads the highlighted preset, Right twice overwrites it with the current bindings, Left twice deletes it. The active preset is remembered across reboots; presets live in `/presets/`. Up/Down picks a button, **A** edits it, **B** goes back. In the editor Up/Down moves between Key, Modifier, Behavior and Interval, Left/Right changes the value (hold for auto-repeat), **A** saves that button, **B** cancels. Setting Key to none and Modifier to none unbinds the button. Group "macro" turns the Key row into a script picker. While setup is open the gamepad sends nothing to the computer.

## Script language

One command per line. Commands are case-insensitive. Lines starting with `#`, `//` or `REM` are comments.

| Command | What it does |
| --- | --- |
| `DELAY 500` | Wait. Milliseconds by default; `250ms` and `1.5s` also work. |
| `STRING hello` | Type the rest of the line. `STRINGLN` adds Enter at the end. |
| `KEY ctrl+c` | Press and release a combination. Keys are joined with `+` or spaces. A line that is only a combination (`ENTER`, `ALT+F4`) does the same. |
| `HOLD shift` | Press keys and keep them held until `RELEASE shift`. `RELEASE` alone lets go of everything. |
| `LOOP 5` … `END` | Repeat the block five times. `LOOP` with no number repeats until stopped. Loops nest. `LOOP 0` skips the block. |
| `CLICK right` | Mouse click: `left` (default), `right` or `middle`. `MOUSE_PRESS` and `MOUSE_RELEASE` hold a button. |
| `MOUSE 40 -10` | Move the mouse by dx dy pixels. |
| `SCROLL -3` | Mouse wheel; negative scrolls down. |
| `MEDIA playpause` | Media key: `play`, `pause`, `playpause`, `next`, `prev`, `stop`, `mute`, `volup`, `voldown`, `brightnessup`, `brightnessdown`, `eject`, `calculator`, `email`, `browser`, `home`, `back`, `forward`, `refresh`, `search`, `sleep`. |
| `TYPE_DELAY 20` | Delay between typed characters for following `STRING` lines. |
| `DEFAULT_DELAY 100` | Delay added after every following command. |

Key names: `ctrl`, `shift`, `alt`, `gui` (`win`, `cmd`, `meta`, `super`), right-hand variants `rctrl` `rshift` `ralt` `rgui` `altgr`, `enter`, `esc`, `tab`, `space`, `backspace`, `delete`, `insert`, `home`, `end`, `pageup`, `pagedown`, `up`, `down`, `left`, `right`, `f1` to `f24`, `capslock`, `printscreen`, `scrolllock`, `pause`, `numlock`, `menu`, `kp0` to `kp9`, `kpenter`, `kpplus`, `kpminus`, `kpasterisk`, `kpslash`, `kpdot`, `minus`, `equal`, `lbracket`, `rbracket`, `backslash`, `semicolon`, `quote`, `grave`, `comma`, `period`, `slash`, and any single letter, digit or one of `- = [ ] \ ; ' ` , . /`.

`STRING` uses the US keyboard layout of the host; characters that need a different layout will come out wrong.

Example:

```
# Alt-tab, then paste a message five times
KEY alt+tab
DELAY 500
LOOP 5
  STRINGLN Ping from Core Macro
  DELAY 1s
END
```

## Serial protocol

115200 baud, one JSON object per line in each direction. Requests carry an `id` that the response echoes. Responses have `"ok": true` or `"ok": false` with an `error` message. The device also pushes events without an `id`.

| Request | Response |
| --- | --- |
| `{"cmd":"ping"}` | device, fw, usb, gamepad, plus state fields |
| `{"cmd":"list"}` | `scripts: [{name, size}]`, `selected` |
| `{"cmd":"get","name":"x"}` | `name`, `content` |
| `{"cmd":"put","name":"x","content":"..."}` | `valid`, and `error` + `line` when the script has a syntax error (it is still saved) |
| `{"cmd":"del","name":"x"}` | |
| `{"cmd":"rename","from":"a","to":"b"}` | |
| `{"cmd":"run","name":"x"}` | state fields |
| `{"cmd":"stop"}` | state fields |
| `{"cmd":"select","name":"x"}` | moves the highlight on the device |
| `{"cmd":"status"}` | state fields |
| `{"cmd":"check","content":"..."}` | `valid`, `error`, `line` without saving |
| `{"cmd":"config","set":{"screenTimeout":30,"brightness":150,"transport":"usb"}}` | `screenTimeout` (seconds, 0 = never), `brightness` (5–255), `transport` (`usb` or `ble`) |
| `{"cmd":"info"}` | `fsUsed`, `fsTotal`, `uptime`, `freeHeap`, `scripts` |
| `{"cmd":"binds"}` | `mode`, `binds: [{button, keys, script, mode, interval}]` for up, down, left, right, a, b; a macro binding has `mode` `macro` and the `script` name |
| `{"cmd":"binds","set":[{"button":"a","keys":"ctrl+c","mode":"burst","interval":100}, ...]}` | saves all bindings; `mode` is `normal`, `burst` or `toggle`; errors name the button |
| `{"cmd":"mode","set":"bind"}` | switches between `macro` and `bind`; without `set` just reads |
| `{"cmd":"presets"}` | `presets: [{name}]`, `active` |
| `{"cmd":"presets","action":"save"\|"load"\|"delete","name":"x"}` and `{"cmd":"presets","action":"rename","from":"a","to":"b"}` | manage presets; each returns the list |

State fields: `state` (`idle`, `running`, `finished`, `stopped`, `error`), `script`, `line`, `total`, `loop`, `loopCount` (-1 = forever), `loopDepth`, `elapsed` (ms), `error`, `usb`, `gamepad`.

State fields also carry `mode` (`macro` or `bind`), `transport` (`usb` or `ble`) and `ready` (true when the current link can take keys).

Events: `{"type":"state", ...state fields}` on every transition and about five times per second while running; `{"type":"selected","name":"x"}` when the highlight moves on the device; `{"type":"mode","mode":"bind"}` on a mode switch; `{"type":"pad","pressed":mask,"toggled":mask}` in Bind mode when buttons change (bits: up 1, down 2, left 4, right 8, a 16, b 32); `{"type":"binds","mode":...,"binds":[...]}` after bindings change on the device; `{"type":"presets","presets":[...],"active":"x"}` after presets change.

Script names: 1–32 characters, letters, digits, space, `_ - . ( )`. Scripts are stored as `/macros/<name>.txt` on LittleFS, 24 KB maximum each.

## Layout

```
src/
  main.cpp        creates the App
  app.cpp/.h      input, serial dispatch, screen power, state fan-out
  macro.cpp/.h    script compiler and the FreeRTOS runner task
  hid.cpp/.h      keyboard / mouse / consumer control dispatcher (USB or BLE), key name table
  ble_hid.cpp/.h  Bluetooth LE HID device (NimBLE)
  gamepad.cpp/.h  Faces Gamepad3 polling with auto-repeat
  bindings.cpp/.h Bind mode: button to key combo table and hold/burst/toggle engine
  storage.cpp/.h  LittleFS scripts and config
  serial_api.*    line reader task and JSON transport
  ui.cpp/.h       320x240 screen rendering
web/index.html    the manager page (Web Serial)
data/macros/      sample scripts for `uploadfs`
```
