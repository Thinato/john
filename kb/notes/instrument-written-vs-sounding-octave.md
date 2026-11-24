---
title: Notes are stored at written pitch; they sound in the PLAYER's instrument octave (whistle +1, guitar -1), playback and mic included
tags: [instruments, pitch, octave, whistle, guitar, playback, mic, transcription]
---

# Written vs sounding pitch per instrument

## Context
Any code that turns a stored note (MIDI number) into a frequency or back: playback, the mic tuner and play-along grading, audio transcription, range checks.

## Rule
`Event::midi` is the written pitch. Convert with `soundingFreq(midi, octave)` / `writtenMidiFromFreq(hz, octave)` using the octave of the instrument that plays it: +1 for the tin whistle (D whistle bottom D is written D4, sounds D5), -1 for guitar and electric guitar, 0 for violin, piano and harmonica. In the GUI that is my instrument (`myInstrument(a.set)`, #90): `perform(..., sound, myInstrument(a.set))` sets `Performance::octave` from it, and the mic reads the heard pitch back with the same octave, so grading compares written with written. Never hard-code the +12.

## Why
Whistle and guitar music are conventionally written an octave away from how they sound so the notes sit on the staff, and players read fiddle and whistle sheets at the same written pitch. So a whistle player on a violin-written tune sounds an octave above written, and that must count as right (#90); with the song's octave the mic would grade every note "octave too high".

## How to apply
Use `Instrument` from `catalog.hpp` (range `low..high`, `octave`, `plays()`) for anything pitch-related. `perform()` without `player` uses the song's instrument (tests, and anything without a Settings instrument). The "played on my instrument" part of the `playback_sounds` test in `tests/core/sound.cpp` renders a violin-written A4 on a D whistle and checks 880 Hz and the written read-back. Changing a song's "Written for" keeps the written notes; transpose by 12 if the user wants the same sound.
