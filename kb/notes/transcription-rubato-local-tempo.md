---
title: Rubato beat tracking follows a Viterbi path of local periods, not per-window peaks
tags: [transcription, tempo, beats, rubato]
---

# Rubato in transcribe_audio

## Context
Changing or tuning `rubato` (`TranscribeOptions::rubato`, the `transcribe_audio` option) in `transcribe()` (`src/transcribe.cpp`): `winR`, `localPeriod` and the `RUBATO_*` constants, used by the beat DP in `fitMeter` (#11).

## Rule
The onset envelope's autocorrelation is computed in windows of `RUBATO_WIN` = 4 s every `RUBATO_STEP` = 0.5 s (normalized by the number of products). Each window scores a lag by its autocorrelation over the window's peak, times a log-normal prior around the song's period (`RUBATO_SPREAD` = 1 octave). A Viterbi path through the windows charges `RUBATO_JUMP` = 50 per squared octave of change between windows. The path's lags (parabolic sub-frame peak) are interpolated to every frame, and the beat DP scores each step against that local period with `RUBATO_TIGHT` = 30 instead of 100. The tempo line gives the median beat as the song tempo and the 10th to 90th percentile as the range. Off, `P[t]` is the global period everywhere, so drafts are byte-identical.

## Why
Taking each window's best lag, even with a prior around the song's period (0.5 octave) and a median filter, failed on the synthetic 80 -> 50 -> 80 bpm tune. In the slow part a run of eighths has as much autocorrelation at the eighth as at the beat, so a few seconds of beats went to double tempo. The Viterbi path stays on the beat level through it. On that tune, window 4 s with jump 50 tracked both the slow sweep and a twice-as-fast sweep (80 -> 50 in 16 beats), where 6 to 8 s windows smeared the fast one. Jump 20 vs 50 and tightness 30 vs 60 changed little there. Rubato that dips to 40 bpm still fails: that is the slowest lag searched (`rHi`), so the path takes the eighth level there.

## How to apply
Check changes with the "ritardando and accelerando" test in `tests/test_core.cpp` and a sweep of the tempo curve (centre/depth 65/15, 60/20, 90/25, 100/10, one and two cycles). Also compare the `tmp/` recordings with and without `rubato` against main: without it, nothing may change except the tempo line's rubato hint, which only shows at confidence low. Those recordings are close to steady: with rubato their median stays within 1 bpm, but the range widens at intros and endings (the fuze piano intro wanders between about 120 and 160). With `meter` 6/8, L's theme jumped to a faster beat level (median 199). So rubato is opt-in, for music that really drifts.
