---
title: Tempo confidence comes from the rival autocorrelation peak; real music is rarely "high" and the runner-up is not always the true tempo
tags: [transcription, audio, tempo, beat-tracking]
---

# Tempo confidence in transcribe_audio

## Context
Reading or tuning the report's tempo line from `transcribe()` in `src/transcribe.cpp` (onset autocorrelation, then Ellis beat tracking).

## Rule
Confidence is `high` when the best weighted autocorrelation peak has no rival above 0.6 of it, `low` when a rival peak more than ~15% away reaches 0.85 or the peak strength (`r[arg] / r[0]`) is under 0.3, else `medium`. The runner-up is reported as "also plausible", but it is a hint, not the answer.

## Why
On the real recordings in `tmp/`, every rival ratio was 0.68 to 0.99 (only the synthetic steady-beat test is clearly high, 0.48), and peak strength was 0.5 to 0.85 on whole songs, so the old "weak beat below 0.15" hint never fired. The fuze piano intro (true 70 bpm) is detected as 125 with the runner-up at 83, not 70: low confidence was the useful signal, not the runner-up.

## How to apply
When retuning the thresholds, print `tempoConf`, `rival` and `rivalBpm` per 30 s window of the `tmp/` recordings and keep the synthetic mix test at "confidence high" (asserted in `tests/test_core.cpp`) and the irregular-lengths test off "high". Don't promise the agent the runner-up is right; tell it to compare versions or use the song's known tempo. Without a `meter`, the tempo line (and its confidence) comes from whichever tempo search gave the winning meter fit, so it can differ from a run with the meter passed (see transcription-meter-detection.md). At confidence low the line also suggests `rubato: true`; with `rubato` it gives the median and range instead, with no confidence (see transcription-rubato-local-tempo.md).
