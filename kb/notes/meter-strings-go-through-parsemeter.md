---
title: Meter strings are validated by parseMeter (N/D with D 2, 4, 8, 16; C and C|); meterTicks stays lenient
tags: [meter, mcp, abc, transcription]
---

# Parsing a meter string

## Context
Adding anything that takes a meter from outside (a tool argument, ABC `M:`, a file).

## Rule
Call `parseMeter` (`src/theory.cpp`): it accepts `N/D` (1 <= N <= 32, D in 2, 4, 8, 16), maps `C` to `4/4` and `C|` to `2/2`, and returns "" for anything else (`none`, `4`, `4/3`). `meterTicks` and `compoundMeter` stay lenient (garbage becomes a whole note) because they run on already-loaded songs; they are not validators. ABC import keeps `C`/`C|` in `Song::meter` (the viewer draws the glyph from it) and warns, assuming 4/4, on an unreadable `M:`. `transcribe()` needs the normalized `N/D` (`fitMeter` reads the denominator after the `/`), so `transcribe_audio` normalizes before decoding the audio.

## Why
`meter: "C"` passed the old `meterTicks > 0` check and crashed `transcribe` (division by zero) because `meterTicks` accepts anything.

## How to apply
Validate at the boundary with `parseMeter`, before expensive work. `update_song` has no meter argument.
