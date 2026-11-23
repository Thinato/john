---
title: transposeToKey picks the octave by rangeCost, ties go to Song::transposed nearest 0; harp keys G..B sit below C
tags: [transpose, key, octave, harmonica, instruments, range, gui]
---

# Play in key: octave choice and harmonica keys

## Context
Changing `transposeToKey()` (`src/analyze.cpp`), `transpose()` and `Song::transposed` (`src/song.cpp`), the harmonica's range and keys in `instruments/harmonica.json`, or the Play in key / Harmonica key pickers in `detailsPanel()` (#67).

## Rule
`transposeToKey(s, tonic, in)` (its choice is `keyShift()`, shared with `playability()`, see playability-song-list-my-instrument) moves by the smallest interval (-5..+6) to the new tonic, then tries that shift +-12 and +-24 and keeps the one with the lowest (out of range, `RangeCost::hard()`, |s.transposed + shift|, |shift|). `transpose()` adds every shift to `Song::transposed` (saved as `"transposed"` only when not 0), so a manual `update_song transpose` counts too. Harmonica keys are `HARP_KEYS`: G, Ab, A, Bb and B harps start below C4 (A harp = A3, 57), Db to F# above it, each 36 semitones wide; the profile is written for the C harp ("Harmonica", 60..96), other keys move it by their place in its `keys` list (lowest first, see instrument-profile-files), and `"harp"` is only saved when not "C".

## Why
The owner wants "pick the original key" to restore the exact original sheet. With only "smallest shift" as the tie-break, a song that fits in two octaves (any short tune on a 3-octave harp) came back an octave off after D -> G -> D. Counting `transposed` from the original makes the tie go back to it. C harp songs and untransposed songs must stay byte-identical to the files written before #67.

## How to apply
Anything that changes a song's notes by a transposition should go through `transpose()` so `transposed` stays right; code that rewrites notes another way (a new import, folding) should reset it to 0. The picker marks the key `tonicPc(key) - transposed` as "(original)". The GUI calls `transposeToKey` inside `editSong()` and returns right after (see gui-edit-may-reload-song). Switching a song to harmonica in the GUI sets its harp to `Settings::harp`; `import_abc` defaults to it too.
