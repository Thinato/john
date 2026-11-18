---
title: Meter detection tries 4/4, 3/4, 6/8, 9/8 on both tempo searches' beats; subdivision decides simple vs compound
tags: [transcription, audio, meter, tempo, beat-tracking]
---

# Meter detection in transcribe_audio

## Context
Changing or tuning the meter choice in `transcribe()` (`src/transcribe.cpp`, `fitMeter`), which runs when `TranscribeOptions::meter` is empty (the tool's `meter` not passed).

## Rule
Each meter is fitted on the beats of both tempo searches (`tempoOf(false)` centred on 110, `tempoOf(true)` on 80), and keeps the better fit. A fit's score is the mean of four parts: subdivision (of the note starts between beats, the share on halves/quarters for 4/4 and 3/4, on thirds for 6/8 and 9/8), note length starting on a beat, accent (note length on beat 1 versus the other beats), and motif (same pitch 1, 2 or 4 bars later). Subdivision separates simple from compound; motif separates 4/4 from 3/4, and 6/8 from 9/8. A passed `meter` uses only its own tempo search, as before #1.

## Why
The meter sets the beat level (a 6/8 beat is a dotted quarter), and each tempo search can lock onto either level: on a synthetic jig the quarter-centred search found the dotted quarter while the dotted-quarter one found a wrong period, so fitting each meter on its own search's beats picked 4/4. Pitch-track note starts lag the onset-flux beats by about half the analysis window (~45 ms), so the subdivision part subtracts the median lag first; without that, every on-beat note looked off the beat.

## How to apply
Check changes with the synthetic tests (jig 6/8, waltz 3/4, the 4/4 mix) and a tempo sweep (110, 150, 190 bpm, auto and given). Known failures: a fast waltz (190) felt in one comes out 6/8 with two waltz bars per bar; a fast reel (190) whose tempo search locks onto 3 eighths comes out 9/8 or 6/8; with `bpm` given, a jig of straight eighths can tie 3/4 and 6/8. On real recordings the global bar phase drifts, so accent is near 0 and 4/4 vs 3/4 rests on motif with margins of 0.01 to 0.03: treat close scores as "check by ear". Scores are around 0.3 on real songs and 0.5 to 0.75 on clean synthetic tunes.
