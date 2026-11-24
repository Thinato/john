---
title: Pitch tracks live in dataDir()/tracks/<id>.json; the strip maps time to staff through the beat sync (song tick, seconds), not seconds per bar
tags: [transcription, import-audio, pitch-track, gui, score, storage]
---

# Pitch track: where it lives and how the strip lines up

## Context
Changing `PitchTrack` / `Transcription::track` (`src/transcribe.hpp`, filled in `transcribe()` in `src/transcribe.cpp`), `importAudio()` / `loadTrack()` / `trackTick()` in `src/import_audio.cpp`, or the pitch strip in `drawScore()` (`src/gui_main.cpp`).

## Rule
`PitchTrack::points` is the Viterbi melody line per analysis frame (time in seconds of the trimmed audio, sounding MIDI or 0, salience; the same values `JOHN_TRANSCRIBE_DEBUG` dumps). `PitchTrack::sync` holds one `(song tick, seconds)` pair per beat of the winning meter fit. A song tick counts from the start of the first event, in event order. `trackTick()` interpolates linearly between beats (extrapolating at the ends). `importAudio()` writes the track compactly to `dataDir()/tracks/<id>.json` before it saves the song. `open()` in the GUI loads it into `App::track`. The strip adds `sp * 5` to a system's height only when a track exists. A frame lands on the event whose ticks contain it and is placed from that event's head (`Slot::cx`) to the next note's or rest's head, or to the end of the line. Unclear notes use the same mapping: tied notes count as one, and their mean salience below 0.35 (the report's threshold for unclear bars) gets the comment.

## Why
The staff isn't proportional to time (slot widths grow with the square root of length, bars take room), and the beat tracker's beats drift on real music, so a fixed tempo can't place frames. The sync uses the same beats the notes were quantized to (grid cell q = beat q/g, a sixteenth = 12 ticks, song start = `clean.front().q0`), so the strip agrees with the notes as well as the transcription itself does: 96% of voiced frames land on a note of the same pitch class in the synthetic test.

## How to apply
If transcription changes how events are emitted, keep song tick 0 at the first quantized note, or move the sync with it. The `aligned()` check in `tests/core/transcribe.cpp` fails otherwise. Edits that change the song's ticks (delete_bars, an agent re-import with `overwrite`) leave the old track misaligned. Nothing rewrites tracks, so delete the file if that matters. Notes that transcription sustains through a decay tail or an accompaniment chord (#6) have low salience, so they get the unclear comment. In the synthetic test that marks the tune's rest in bar 4, which the draft fills with G: that's intended, not a false alarm. Tracks only exist on the desktop. The web never has a `tracks/` folder, so the strip code is harmless there.
