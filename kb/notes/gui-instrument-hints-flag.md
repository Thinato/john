---
title: In the GUI, the local `whistle` flag means "show whistle hints"; range markers come from Instrument::plays
tags: [gui, hints, fingering, instruments, out-of-range]
---

# Instrument hints vs out-of-range markers in the GUI

## Context
Code in `src/gui_main.cpp` that draws fingering charts or fingering text (`drawScore`, the note tooltip, `detailsPanel`, the mic readout in `transportBar`), or adds a new hint.

## Rule
Each of those spots computes `whistle = instrumentOf(song).id == "whistle" && a.set.hints`, so `whistle` means "show the whistle hints", not "the song is for a whistle". Out-of-range markers (red note, "Out of range for the ...") use `inst.plays(midi)` in the non-hint branch, and they stay when hints are off.

## Why
The "Instrument hints" checkbox (#16) hides fingerings without hiding range problems. That works because the whistle's `Instrument` range (bottom note to +23) equals the range where `finger()` returns `ok`, so the non-hint branch flags the same notes in red. For instruments without hints the checkbox is disabled and drawn unticked (a local `shown` copy), while `a.set.hints` keeps the user's choice for the next whistle song.

## How to apply
Gate any new hint (fingering, "blow harder", hand positions) on that same `whistle && a.set.hints` value, never on the instrument alone. When hints are added for another instrument, update the checks and the checkbox's `noHints` condition next to "Follow agent". If the whistle range or `finger()` ever changes, keep the two ranges equal, or out-of-range notes will show differently with hints on and off. The choice is a saved setting (`Settings::hints`, #18); the Settings window has its own plain checkbox for it.
