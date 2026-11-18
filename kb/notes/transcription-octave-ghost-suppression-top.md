---
title: The octave-ghost suppression stops at C6 even though the pitch search goes to E7; extending it shifts real notes an octave
tags: [transcription, audio, pitch-search, octave-errors, whistle]
---

# Octave-ghost suppression vs the top of the pitch search

## Context
Changing the pitch search range (`LO`/`HI` in `src/transcribe.cpp`) or the octave-ghost suppression after the semitone salience in `transcribe()`.

## Rule
The search covers C3..E7 (#40), but only states up to C6 have half of the octave above subtracted (`m + 12 <= 96 - LO`), as when the search stopped at C7. The states above C6 aren't suppressed. Changing that boundary changes which octave real notes land in, so check it on the real recordings, not only the synthetic tests.

## Why
The suppression takes ~0.4x off every note (its 2nd harmonic sits an octave up), and the top octave of states has nothing above it, so it isn't penalised. With the plain `m + 12 < M` and the search raised to E7, C#6..E6 (the whistle's second-octave D and E) got suppressed by D7..E7, their own harmonics. Concerning Hobbits with `recording_instrument` whistle then flipped its octave fit and came out a whole octave high (225 of 269 notes moved), and L's theme and fuze gained 140 to 160 frames of octave-up errors above C7. Computing the octave above from extra fine bins, so that the top octave is suppressed too, flipped Hobbits the same way. The synthetic tests passed in every version.

## How to apply
When moving `HI` or touching the suppression, compare the `tmp/` recordings with and without `recording_instrument` against main. Compare the report lines (octave, notes moved), the per-frame dump (`JOHN_TRANSCRIBE_DEBUG`: count frames whose pitch changed by ±12 or now sit above the old top), and the drafts' notes aligned by pitch class (octave moves vs other changes). With the current boundary, raising the top to E7 left the three recordings' notes almost unchanged (0 to 2 octave moves).
