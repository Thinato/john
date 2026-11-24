---
title: Technique marks shown and played come from marks(e.techniques, myInstrument); the song keeps every mark
tags: [techniques, ornaments, instruments, gui, playback, analyze, mcp, settings]
---

# Technique marks per instrument (marks(), instead)

## Context
Drawing, listing, playing or suggesting technique marks: `drawScore`, the note tooltip and the legend in `src/gui_main.cpp`, `perform()` in `src/sound.cpp`, `analyze()` in `src/analyze.cpp`, `Instrument::instead` and `marks()`/`hiddenMarks()` in `src/catalog.cpp` (#86).

## Rule
`marks(techniques, in)` decides what a note shows on `in`: a mark the profile lists stays, one in `in.instead` becomes its equivalent (duplicates merged: a piano's cut and roll on one note are grace + turn), `breath` always stays, a technique that isn't built-in always stays (`builtinTechnique()`: a default id that `techniques.json` doesn't redefine, so a user's own `grace` from before #86 is custom), the rest goes to `hidden`. The GUI passes `myInstrument(a.set)` unless `Settings::allMarks` (Show all marks) is on; `perform()` always uses the player's marks (Show all marks is display only); `analyze(s, player)` offers each candidate as the player shows it (`offer()`), whistle text unchanged. `instead` targets must be listed in `techniques` and keys must not be (`parseInstrument()`); `saveInstrument()` checks every technique id in `techniques` and `instead` against `loadTechniques()`, so the Settings editor and `define_instrument` share one check.

## Why
Owner's plan on #86: the profile owns the link, a tiny equivalents table (piano cut/tap/double_cut -> grace, rolls/cran -> turn; harmonica cut/tap -> grace, roll/short_roll -> shake), hide the rest with a count in the legend, breath stays as a phrase mark for every instrument, and the song file never changes so switching back restores the original marks. Custom techniques always show because no built-in profile can list a technique the user defined; hiding them would make `define_technique` look broken.

## How to apply
Never draw or play `e.techniques` directly: go through `marks()`. `builtinTechnique()` reads the custom ids cached by the last `loadTechniques()` (no file read per note): the GUI's `poll()` and every MCP tool call refresh it, `saveCustomTechnique()` adds to it. The fallback whistle (no `whistle.json`) lists every built-in but grace/turn/shake, like the file. Grace/turn/shake notes last `Voice::graceLen` = min(35 ms, a quarter of the audible note), so on short notes the main note is still heard. The song editor (details panel checkboxes, `set_techniques`, `get_song`) still edits and lists the raw marks. The legend lists only techniques that show as themselves on my instrument (or all with Show all marks), and names breath "Phrase end (lift)" when my instrument doesn't list breath (the note tooltip too); the hidden count is not shown with Show all marks on. A new equivalent technique needs a catalog entry (symbol, description, pt-BR), playback in `perform()` and a test that renders it (YIN on a buffer, never the speakers). The editor drops `techniques` ids and `instead` entries the catalog doesn't know (a shared profile; `saveInstrument()` would refuse them and the editor can't show them), `instead` entries whose target is no longer listed, and offers no row for breath or custom techniques. Agents store the written-for instrument's technique (the GUIDE says so), never the equivalent.
