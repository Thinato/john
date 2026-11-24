---
title: Transcription triplets need every off-beat start within 0.04 beat of a third; swing is the median second-eighth position
tags: [transcription, audio, rhythm, triplets, swing, quantize]
---

# Triplets and swing in transcribe_audio

## Context
Changing the quantize step in `fitMeter` (`src/transcribe.cpp`), the `TRIPLET`/`SWING_*` constants, the triplet events in the events stage, or `toAbc`'s `(3` output (#2).

## Rule
Grid cells are 4 ticks (a third of a sixteenth), so a beat holds sixteenths (3 cells) and eighth triplets (4 cells). Only in simple meters, with the onset lag removed first: a beat is a triplet when it has at least 2 off-beat note starts and every one is within `TRIPLET` = 0.04 beat of 1/3 or 2/3. A tune is swung when at least 8 beats have exactly one off-beat start between 0.4 and 0.8, the median of those (the swing point s) is 0.57 to 0.72, and under a quarter of them are below 0.55. Swung beats are warped piecewise-linearly (s goes to 1/2), then rounded to sixteenths. A triplet beat is always written as three tied or separate 16-tick events (`tuplet = 3`, `tupletQ = 2`, `tupletStart` on the first), never through `pieces()`. The meter fit scores a plain sixteenth quantization (`quantize(false)`), so triplets and swing never change the meter choice or its scores.

## Why
A margin on the average distance to each grid (triplet vs sixteenth) didn't work: on the real recordings in `tmp/`, chance pairs of off-beat starts such as 0.38/0.62 beat beat the sixteenth grid by as much as the synthetic triplets did (0.05 to 0.08), giving 7 to 11 false triplet beats per song. The per-start tolerance keeps all 6 synthetic triplets (worst start 0.033 off) and leaves 1, 1 and 0 triplet beats on the three recordings. A beat with only one off-beat start is never a triplet, so a swung pair (start at about 2/3) goes to the swing count instead. A pickup that starts inside a triplet beat would leave the group short, so that beat stays straight.

## How to apply
Straight drafts must stay byte-identical: compare `toAbc` plus the report on the `tmp/` recordings (auto, 4/4, 3/4, 6/8) against a build from main before and after a change. The 6/8 runs must not change at all. Check with the "eighth triplets" part of the `transcription` test in `tests/core/transcribe.cpp`. Known limits: without `meter`, the synthetic triplet and swung tunes, and even a quarter-note tune with two triplets, are detected as 6/8 by the meter fit's subdivision part, as they were before #2. A swung tune at 120 bpm with `bpm` given lost its beats (tempo 117) and found no swing. Swing is not detected in 6/8 or 9/8.
