# Changelog

Each `## [x.y.z]` section becomes the release notes when `kFirmwareVersion` in `src/config.h` is bumped to that version and pushed to `main`.

## [1.2.0]

### Highlights

- **Bluetooth LE output.** Keys, mouse and media keys can now go over Bluetooth instead of the USB cable. The device shows up as "Core Macro"; pair it like any Bluetooth keyboard. Your choice is remembered across reboots, and the web page keeps working over USB either way.
- **Device menu.** Hold Select for about a second to change mode, output (USB or Bluetooth), screen-off time and brightness right on the device.
- **Bind presets.** Save whole sets of bindings and switch between them. On the device, hold Start in Bind mode to save, load, overwrite or delete presets. On the web page, presets can be loaded, renamed, deleted and edited directly without loading them first.

### Improvements

- The bind editor on the device shows the actual character next to symbol keys (for example `-  minus`), and a new "shifted" group offers `! @ # $ % ^ & * ( ) _ + { } | : " ~ < > ?`.
- The web page has a "Keys go out over" setting and its device mirror shows whether USB or Bluetooth is in use.
- Each version bump publishes a GitHub release with a ready-to-flash image and these notes.

### Changes

- Hold Start no longer switches the output; use the Select menu or the web page instead. A short Start press still opens bind setup in Bind mode.

## [1.1.0]

- New binding behaviors: toggle burst (one press starts repeating the key, the next press stops it) and run macro (one press runs a saved macro, the next press stops it).
- The device always starts in Macro mode, and no binding fires on its own after boot or after switching modes.
- Keyboard output is safe when a macro and a binding send keys at the same time.

## [1.0.0]

- First release: USB macro pad firmware for the M5Stack CoreS3 with the Faces Gamepad3, a script language with nested and endless loops, a Web Serial manager, Bind mode with hold, burst and toggle bindings, and on-device binding setup.
