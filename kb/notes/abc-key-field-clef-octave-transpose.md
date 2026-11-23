---
title: ABC K: options - clef names are not the key, octave= and clef +8/-8 shift notes, transpose= is ignored
tags: [abc, import, key, clef, octave, transpose]
---

# K: field options: clef, octave=, transpose=

## Context
Importing ABC whose `K:` line carries more than the key, e.g. `K:G bass`, `K:Am clef=bass3 octave=-1`, `K:D treble+8 transpose=-2`. Parsed in the header `K:` branch of `importAbc` in `src/abc.cpp`.

## Rule
Clef names (treble, bass, alto, tenor, baritone, perc, none, optionally with a staff line digit like `bass3`) are stripped from the key, bare or as `clef=`. They don't change pitches: ABC pitches are absolute. `octave=N` and a clef's `+8`/`-8` suffix add N (or ±1) octaves to every note, applied in `readPitch`. `transpose=` (playback-only in ABC 2.1), `middle=` and `stafflines=` are dropped without a warning. An unknown `clef=` value is warned about and ignored.

## Why
Before #81 every `key=value` token was dropped and the rest glued together, so `K:G bass` became the key "Gbass" and fell back to D major, and `octave=-1` bass parts came out an octave high.

## How to apply
The shift is baked into the stored `Event::midi`, and `toAbc` writes only `K:<key>`, so a save/re-export/re-import keeps the pitches (no double shift). Mid-tune `K:` lines and inline `[K:...]` are still ignored with a warning (one key per song), including any clef/octave options in them. Whether `+8`/`-8` should move pitches is debated between ABC tools; John follows the issue's decision that it does.
