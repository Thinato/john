---
title: ABC K: options - clef names are not the key, octave= shifts written notes, clef +8/-8 and transpose= are ignored
tags: [abc, import, key, clef, octave, transpose]
---

# K: field options: clef, octave=, transpose=

## Context
Importing ABC whose `K:` line carries more than the key, e.g. `K:G bass`, `K:Am clef=bass3 octave=-1`, `K:D treble-8 transpose=-2`. Parsed in the header `K:` branch of `importAbc` in `src/abc.cpp`.

## Rule
Clef names (treble, bass, alto, tenor, baritone, perc, none, optionally with a staff line digit like `bass3` and a `+8`/`-8` suffix) are stripped from the key, bare or as `clef=`. They don't change pitches: ABC pitches are absolute. `octave=N` changes what the letters mean, so it adds N octaves to every written note (applied in `readPitch`). A clef's `+8`/`-8` and `transpose=` only change the sounding pitch, so they are ignored without a warning, like `middle=` and `stafflines=`. An unknown `clef=` value is warned about and ignored.

## Why
Before #81 every `key=value` token was dropped and the rest glued together, so `K:G bass` became the key "Gbass" and fell back to D major, and `octave=-1` bass parts came out an octave high. The owner decided that `+8`/`-8` must not shift: in ABC it means the same as `transpose=12` plus a small 8 drawn on the clef (abc2xml treats it as display only too). John stores written pitch and gets the sounding octave from the instrument (see instrument-written-vs-sounding-octave.md).

## How to apply
The `octave=` shift is baked into the stored `Event::midi`, and `toAbc` writes only `K:<key>`, so a save/re-export/re-import keeps the pitches (no double shift). Mid-tune `K:` lines and inline `[K:...]` are still ignored with a warning (one key per song), including any clef/octave options in them.
