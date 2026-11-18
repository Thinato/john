---
title: Audio transcriptions need checking against the pitch-track dump; known misses are quiet notes and wide leaps
tags: [transcription, audio, mp3, debugging, continuity-filter]
---

# Reviewing an audio transcription

## Context
Fixing or tuning `transcribe_audio` output (`src/transcribe.cpp`) on real recordings in `tmp/`.

## Rule
Don't trust the draft for individual notes. Run with `JOHN_TRANSCRIBE_DEBUG=<file>` and inspect the per-frame track (time, chosen MIDI, confidence, top 3 candidates) around doubtful spots. The dump's MIDI numbers are the recording's sounding pitch: for a whistle subtract 12 (for a guitar add 12) to compare with `get_song`, and add any octave shift the report mentions. Two known misses: quiet notes (salience around 0.27) are dropped, and the continuity filter can delete a real note at the start of a wide leap (#7).

## Why
On the fuze piano intro the user heard two missing D's that the draft didn't show; both were visible in the dump. The detected tempo was also wrong without a hint (125 instead of 70); since #8 the report's tempo line says so (confidence low, with an "also plausible" tempo).

## How to apply
When the user says a note is missing or wrong, look at that time window in the dump before editing the ABC. Pass `bpm` when the song's tempo is known, or when the tempo line says confidence low or medium and the note lengths look wrong. Every change to the pipeline must still pass the synthetic mix test in `tests/test_core.cpp`.
