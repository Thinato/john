---
title: Notes are stored at the instrument's written pitch; whistle sounds an octave up, guitar an octave down
tags: [instruments, pitch, octave, whistle, guitar, playback, mic, transcription]
---

# Written vs sounding pitch per instrument

## Context
Any code that turns a stored note (MIDI number) into a frequency or back: playback, the mic tuner, audio transcription, range checks.

## Rule
`Event::midi` is the written pitch for the song's instrument. Convert with `soundingFreq(midi, octave)` / `writtenMidiFromFreq(hz, octave)` using `instrumentOf(song).octave`: +1 for the tin whistle (D whistle bottom D is written D4, sounds D5), -1 for guitar and electric guitar, 0 for violin, piano and harmonica. Never hard-code the +12.

## Why
Whistle and guitar music are conventionally written an octave away from how they sound so the notes sit on the staff. Before instruments existed (#14) the +12 was baked in everywhere; switching a song to violin would otherwise play and grade it an octave off.

## How to apply
Use `Instrument` from `john.hpp` (range `low..high`, `octave`, `plays()`) for anything pitch-related. Changing a song's instrument keeps the written notes, so it may sound in another octave; transpose by 12 if the user wants the same sound.
