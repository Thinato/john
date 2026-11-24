---
title: The harmonica profile is a 10-hole Richter harp: bends hard (weight 1), overblows and overdraws hard (weight 3)
tags: [instruments, harmonica, playability]
---

# Harmonica note table (Richter layout)

## Context
Changing `instruments/harmonica.json`, harmonica playability or analysis, or adding harmonica hints (hole numbers, bends).

## Rule
The table is relative to the harp's bottom note (blow 1), so it serves every key in `keys`. In semitones above it: blow/draw notes 0 2 4 7 11 12 14 16 17 19 21 23 24 26 28 29 31 33 36 are easy; draw bends 1 (hole 1), 5 6 (hole 2), 8 9 10 (hole 3), 13 (hole 4), 20 (hole 6) and blow bends 27 (hole 8), 30 (hole 9), 34 35 (hole 10) are hard with `how: bend`, weight 1; overblows 3 15 18 22 (holes 1, 4, 5, 6) and overdraws 25 32 (holes 7, 9) are hard with `how: overblow`, weight 3. No note in range is impossible.

## Why
Layout: C harp blow C E G C E G C E G C, draw D G B D F A B D F A (Wikipedia, "Richter-tuned harmonica"). Holes 1-6 bend on the draw, 7-10 on the blow, each down to a semitone above the other reed of the hole (so hole 5 and hole 7 have no full-semitone bend); an overblow (1-6) or overdraw (7-10) sounds about a semitone above the hole's higher reed (Wikipedia, "Overblow"). The owner asked to choose between "hard with a higher weight" and "impossible" for overblows: they are an advanced but real technique, so hard x3 (one overblow costs as much as three bends or half-holes) keeps songs that need one playable on paper without calling them easy. The hole 10 overdraw (C#7) is above the range.

## How to apply
Test both a C harp and an A harp (the `instrument_profiles` test in `tests/core/catalog.cpp` checks every offset). The bottom octave of a harp needs bends for the 4th and 6th of its major scale, so a melody there usually fits only an octave up: that is the real harp, not a bug. If a deeper bend should cost more (the 1.5-step bend on hole 3), raise its `weight` in the file and in the test.
