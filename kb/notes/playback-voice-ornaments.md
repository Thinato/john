---
title: Each playback sound plays only the ornaments and breaths its instrument can; the pitch never follows the sound
tags: [audio, playback, synth, instruments, ornaments, breath, sound]
---

# Which ornaments each playback voice plays

## Context
Changing playback in `src/sound.cpp`: `perform(song, bpm, from, countIn, sound)` and `Synth::render`, or the GUI's Sound combo (`App::set.sound` in `src/gui_main.cpp`, "" = my instrument from Settings, shown "(mine)"; a saved setting since #18).

## Rule
`perform()` decides per sound (`Performance::sound`, a `SOUNDS` id; an instrument profile names its voice in `sound`) what is played: whistle plays everything; violin plays cuts, taps, rolls, crans, slides and vibrato but no breath gaps; guitar plays finger ornaments (as hammer-ons/pull-offs) and slides, no vibrato or breaths; electric guitar adds vibrato; harmonica plays breaths, slides (bends) and vibrato but no finger ornaments; piano plays none of them (staccato and tonguing gaps still apply to every voice). Grace notes, turns and shakes (#86) play on every voice. Before the voice filter, the note's marks go through `marks(e.techniques, player)`: hidden marks aren't played and equivalents are (a whistle tune on my piano plays grace notes and turns; see technique-marks-follow-my-instrument). `Performance::octave` always comes from the player's instrument (`perform`'s `player`: Settings' instrument in the GUI since #90, else the song's), not from the sound.

## Why
Cuts, taps, rolls and crans are finger ornaments: a fiddle and a guitar have an equivalent, a piano key can't bend into a note and a harmonica player doesn't finger holes. Breath marks only mean something on wind instruments. Issue #15 settled that switching the sound changes only the timbre, so a whistle player choosing the "guitar" sound still hears the notes an octave above written.

## How to apply
Piano, guitar and electric guitar get a 12 ms gap at the end of every note (`plucked` in `perform()`) so the next note is a new attack; without it the decaying envelope just continues and fast notes blur into one. The timbres are a few fixed harmonics each (ponytail-marked); change them by ear and keep the "playback sounds" test in `tests/test_core.cpp` passing (YIN pitch, decay, no clipping).
