---
title: Transcription transposes before folding, only past 10% folded, with a weighted cost
tags: [transcription, range, transpose, whistle]
---

# Automatic transposition in transcription

## Context
Changing the "key, octave fit, events" stage of `transcribe()` (`src/transcribe.cpp`), the `transpose` option (`TranscribeOptions`, `transcribe_audio`), or the weights in `RangeCost` (`src/john.hpp`, shared with `analyze()`).

## Rule
When the best octave still folds more than 10% of the note time (and `transpose` is unset), the shifts -5..+6 are tried before folding. Each is costed as `analyze()` ranks transpositions (`rangeCost()`: half*10 + cross*2), except that a note out of range costs 20, not 1000. Ties go to the smallest shift. Other instruments only count notes out of range. A given `transpose`, 0 included, is used as is. Below the threshold nothing changes, so the draft stays byte-identical.

## Why
Folding moves single notes by an octave and distorts the melody, and a later `update_song transpose` can't undo it (#9). The plan said "about 5%" and "most in range first". On the `tmp/` recordings both went wrong. Concerning Hobbits with `recording_instrument` whistle is a correct D major draft and folds 5.9% (its top notes), so at 5% it would have moved to another key. On real recordings most of the out-of-range part is octave errors and accompaniment. Strict "most in range first" picked keys like G# minor for fuze (205 half-holes vs 135 at the recording's pitch) and F major for unlimited Hobbits. Weights 10 to 20 per out note gave the same picks: G major for Hobbits (6 half-holes), B minor for fuze (33), none for L's theme.

## How to apply
When changing the threshold or the weights, compare every `tmp/` recording and instrument against main. As of #9, 16 of 18 drafts were identical and only the two unlimited whistle drafts moved. The report's `transposed ... (X -> Y)` line keeps the `key:` line at the recording's key. `importAudio()` copies the transposed line into the song notes.
