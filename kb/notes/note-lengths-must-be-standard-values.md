---
title: Note lengths must be standard (optionally dotted) values; split odd ones with ties
tags: [notation, durations, gui, abc, ticks]
---

# Only standard note values render correctly

## Context
Writing or editing notes whose length is not a plain, dotted or double-dotted value, e.g. `E10` with `L:1/16` (120 ticks, ten sixteenths).

## Rule
The viewer draws each note as one glyph (`valueOf` in `src/gui_main.cpp`); an odd length is shown as the next shorter value, so the sheet looks wrong while playback is right. Write odd lengths as tied standard values (`E8-E2`) or a note plus a rest (`E8 z2`).

## Why
A 120-tick note in a lesson rendered as a half note with the wrong rhythm. Durations are ticks with 192 per whole note; only powers of two, ×1.5 and ×1.75 map to a single glyph.

## How to apply
When generating ABC (transcriptions, lessons, edits), make every length a standard value in the bar's beat grouping. Check odd values in `get_song`'s length column: it names the value ("dotted quarter"); anything else needs splitting. Transcription drafts already do this in `pieces()` (`src/transcribe.cpp`, #3), for notes and rests alike: a value starting off the beat stops at the next beat; one on a beat may span whole beats, or a beat and a half in simple time (the dotted quarter), but never crosses the middle of a 4-beat bar (4/4, 12/8) unless it starts the bar. Follow the same rules when splitting by hand.
