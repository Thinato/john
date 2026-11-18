---
title: Audio transcriptions need checking against the pitch-track dump; known misses are quiet notes and wide leaps
tags: [transcription, audio, mp3, debugging, continuity-filter]
---

# Reviewing an audio transcription

## Context
Fixing or tuning `transcribe_audio` output (`src/transcribe.cpp`) on real recordings in `tmp/`.

## Rule
Don't trust the draft for individual notes. Run with `JOHN_TRANSCRIBE_DEBUG=<file>` and inspect the per-frame track (time, chosen MIDI, confidence, top 3 candidates) around doubtful spots, filtering to the instrument's range. Two known misses: quiet notes (salience around 0.27) are dropped, and the continuity filter can delete a real note at the start of a wide leap (#7).

## Why
On the fuze piano intro the user heard two missing D's that the draft didn't show; both were visible in the dump. The detected tempo was also wrong without a hint (125 instead of 70, #8).

## How to apply
When the user says a note is missing or wrong, look at that time window in the dump before editing the ABC. Pass `bpm` when the song's tempo is known. Every change to the pipeline must still pass the synthetic mix test in `tests/test_core.cpp`.
