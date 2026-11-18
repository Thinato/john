---
title: The continuity filter tells a melody leap from accompaniment by whether the next note already sounds
tags: [transcription, audio, continuity-filter, accompaniment, leap]
---

# Continuity filter: melody leap vs accompaniment

## Context
Changing the melody continuity filter in `transcribe()` (`src/transcribe.cpp`), which moves octave errors and drops short notes well below the ±2 s median pitch as accompaniment.

## Rule
A short low note is kept as melody when the next note is back in the melody's range and that next pitch has under 0.25x the low note's salience while the low note sounds (the melody hasn't reached it yet). Accompaniment poking through has the melody sounding over it: in the synthetic tests the ratio is about 0.9 for a chord attack and under 0.1 for a real leap.

## Why
Issue #7: a low D before a leap up a tenth was dropped because the median sits in the high register. The issue's ideas don't separate the two cases: the Viterbi path has almost no gaps in a mix (and none in legato synth audio), so "starts after a rest" never fires, and accompaniment notes recur nearby as often as melody notes do.

## How to apply
To check a filter change, temporarily print each dropped note (time, length, pitch, median, the next-pitch ratio) and compare them on the synthetic tests and on `tmp/` recordings; remove the print before committing. On piano recordings a bass note played just before the melody note can pass the leap rule (L's theme gained about 30 low notes), so judge filter changes by ear too.
