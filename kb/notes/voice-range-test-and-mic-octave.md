---
title: Voices are profiles with "voice": true; the mic folds a singer's octave (judgeNote), Find my range writes my-voice (sungNote, myVoice)
tags: [instruments, voice, singing, mic, range, settings, gui, web, lyrics]
---

# Singing voices: profiles, Find my range, the mic's octave rule

## Context
Touching the voice profiles (`instruments/voice-*.json`), `Instrument::voice`, mic grading in `practiceUpdate()` (`src/gui_main.cpp`), `heardCents()`/`judgeNote()`/`sungNote()`/`myVoice()` in `src/sound.cpp`, or the Find my range window (`rangeWindow()`, `noteStaff()`) (#87).

## Rule
A voice is an ordinary profile with `"voice": true` (soprano, alto, tenor and bass presets; tenor and bass have `octave: -1` like the guitar, and the sheet stays in treble clef). Code asks `in.voice`, never the id: `my-voice` and agent-defined voices (`define_instrument` takes `voice`) carry the flag. The mic always goes through `judgeNote(heard, want, tolerance, myInstrument)`: for a voice `heardCents()` folds the distance to the nearest octave of the note, so an octave off is Right and never Octave (orange). The "(octave too high/low)" readout is hidden for voices. Find my range records one gated YIN pitch per frame over `VOICE_MIN_HZ..VOICE_MAX_HZ` for 3 s, and `sungNote()` (median, nullopt under a third voiced) and `myVoice()` (at least a fifth, written in the preset's octave, the preset's techniques/instead/sound) do the maths. Save calls `saveInstrument()` and sets `Settings::instrument = "my-voice"`.

## Why
Owner's plan on #87: singers pick their own octave, other instruments keep "wrong octave"; the core does the maths and the GUI only records. `my-voice` stores the English name "My voice" (the MCP server reads the file) and the GUI translates it through `instName()`'s `tr(in.name)`. A singer breathes whatever the sound, so `perform()` plays breath gaps when the player is a voice (the presets use the violin sound; there is no singing synth).

## How to apply
Test voice behaviour with rendered buffers and `judgeNote()`, never the mic: the "singing voices" block in `tests/test_core.cpp` renders vibrato takes framed like the GUI and a tenor's A3 through `Synth`. The main loop stays live while `a.audio.micOn()`, so a take gets a pitch every frame (the desktop loop otherwise waits for events). To snapshot the range window, patch locally: set `showRange`, `rangeFrom`, `rangeNote[]`, `rangeTake`/`rangeStart` and skip `a.audio.latest()`, then revert. The lyric highlight follows `a.cursor` back over held/tied notes and bar lines to the syllable being sung; fake `a.cursor` after `practiceUpdate()` to snapshot it. The same-results test (#85) skips `voice-*` ids, and the catalog size asserts count them.
