---
title: A new setting is one Settings field, its name in the JSON macro in song.cpp, and one line in settingsWindow
tags: [settings, gui, mcp, storage, preferences]
---

# Adding a setting (settings.json)

## Context
Adding or changing a user preference (#18): `struct Settings` in `src/john.hpp`, `loadSettings()`/`saveSettings()` in `src/song.cpp`, `settingsWindow()` and `App::set` in `src/gui_main.cpp`.

## Rule
Add the field with its default to `Settings`, add its name to the `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT` list next to `loadSettings()`, and add one widget line to `settingsWindow()`. The GUI reads and edits `a.set.<field>` directly; the main loop saves `a.set` whenever it differs from `a.saved` and no widget is being dragged, so there is no per-setting save code. The MCP server calls `loadSettings()` itself (today only `import_abc`, for the default instrument).

## Why
The macro gives missing keys their default and ignores unknown keys. Any parse or type error (garbage file, `"hints": "yes"`) makes `loadSettings()` return all defaults, as the issue asked. After parsing it repairs values the GUI could crash or misbehave on: an unknown `sound` or `instrument` id (the transport's sound combo dereferences `findInstrument`), and zoom, tempo and volume outside the slider ranges.

## How to apply
A field holding an `INSTRUMENTS` id, or a number that layout or audio divides by, needs a repair line in `loadSettings()` too, and the test in `tests/test_core.cpp` should cover it. A control in the main window for the same preference must edit `a.set.<field>`, not a copy, or the two drift apart (the sidebar hints/zoom, transport sound/metronome/volume/tolerance and the *Reset marks* right-click popup all do). Values that live in the `Audio` object (metronome, volume) are copied from `a.set` once per frame in `main()`. The theme is applied in `main()` when `a.set.light` changes. To snapshot the Settings window, set `App::showSettings = true` locally; other values can be put in `JOHN_HOME/settings.json` with no rebuild.
