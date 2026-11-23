---
title: Instruments are JSON profiles (instruments/, data folder); built-ins must give the same results as before #85
tags: [instruments, playability, catalog, settings, mcp]
---

# Instrument profiles: files, catalog, same results

## Context
Adding or changing an instrument, `struct Instrument`/`NoteLevel` in `src/john.hpp`, the catalog code in `src/song.cpp` (`parseInstrument`, `findInstrument`, `instruments()`, `saveInstrument`), `rangeCost()` in `src/analyze.cpp`, the Settings instrument editor in `src/gui_main.cpp`, or the MCP tools `list_instruments` / `define_instrument` / `delete_instrument` (#85).

## Rule
One JSON file per instrument; the file name is the id. Built-ins are `instruments/<id>.json` (found by `instrumentsDir()` like `lessons/` and `i18n/`, bundled on the web at `/instruments`); custom ones are `<data folder>/instruments/<id>.json` and a custom file with a built-in's id replaces it in place. Fields: `name`, `low`/`high` (written pitch, `"D4"` or a MIDI number), `octave`, `key`, `keys` + `keyed_name` (keyed instruments: the whistle/harp key moves the range by its pitch class above `keys[0]`), `notes` (semitones above `low` -> `"hard"`/`"impossible"` or `{level, how, weight}`; missing = easy), `techniques`, `sound` (a `SOUNDS` voice), `hints`, `order`. A file that fails `parseInstrument()` is skipped with a message in `instrumentWarnings()` (shown in Settings and by `list_instruments`). `rangeCost()` reads `Instrument::level()`: `out` counts notes out of range or impossible, `hard()` adds `weight * 10` per hard note.

## Why
The owner's decisions on #85: files can be shared, a broken one only drops itself, and one whistle/harp profile serves every key because the table is relative to the bottom note. The whistle's table reproduces `finger()`: half-holes weight 1, the cross-fingered C weight 0.2, so `hard()` is still `half * 10 + cross * 2`. The owner required the same results for every built-in except the harmonica; `tests/test_core.cpp` compares the old `findInstrument()`/`rangeCost()` (copied into the test) with the new ones over every whistle key and a pitch sweep. `RangeCost::weight` defaults to `half * 10 + cross * 2` so the existing literal `RangeCost{out, half, cross}` checks still mean the same.

## How to apply
Never hard-code an instrument list again: use `instrumentIds()` for instrument pickers and `SOUNDS` for playback voices (synth voices stay code). Keys only exist exactly as listed in `keys` (lowercase `"d"` is no longer a whistle key; `instrumentOf` falls back to the D whistle anyway). The catalog is cached behind a mutex: the GUI's `poll()` and every MCP tool call run `reloadInstruments()`, and the GUI judges every song again when `instruments()` changes. Songs write `instrument_name` only for non-built-in ids (`toJson` looks it up), so a deleted custom instrument shows "Written for: <name> (deleted)" (`instrumentLabel()`, the GUI's `writtenFor()`) and plays as the D whistle; a deleted Settings instrument becomes the D whistle in `loadSettings()` and in the GUI's delete button. Gate fingering charts on `Instrument::hints`, which means tin whistle charts fingered on `key`. Changing a built-in's table changes playability for everyone: update the same-results test only if the owner asked for the change.
