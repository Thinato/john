---
title: Transcription sustains notes through their decay tail; a rest needs the pitch to stop, or a gap over a beat after the tail
tags: [transcription, audio, piano, rests, note-cleanup, quantize]
---

# Sustaining decaying notes instead of writing rests

## Context
Changing note cleanup or quantizing in `transcribe()` (`src/transcribe.cpp`): the tail loop after the note confidences, `RawNote::ring` / `QNote::ring`, and the gap closing in `fitMeter`'s quantize step.

## Rule
After the continuity filter, each note is extended through the frames after it where its pitch is still the strongest (`S == frameMax`, up to the next note). A note whose tail lasts at least `minF` is `ring`: the quantizer then also closes what is left of its gap when that is at most a beat (`g` cells); other gaps close only at one sixteenth. A note released into a real rest has a tail of 2 to 4 frames (the synth's release), so its rest stays.

## Why
Issue #6: piano and guitar notes fade under the Viterbi's voicing level long before the next note, so the draft was full of "note + rest" pairs. In the per-frame dump of the fuze piano intro the previous pitch stays the top candidate for 0.1 to 1 s into each gap, then the recording's hiss (top candidates around MIDI 98/99) takes over. A plain "gap under a beat" rule wouldn't do: those gaps are 0.6 to 2.4 s at 70 bpm, and the synthetic whistle and piano melodies with real rests have eighth-note gaps that must stay.

## How to apply
Check changes with the "piano notes fade" test in `tests/test_core.cpp` (a 40 bpm piano melody of half and whole notes, and a piano melody with eighth rests). On `tmp/`, count rests before and after: fuze went from 43 to 20 rest events (1524 to 852 ticks), Concerning Hobbits 59 to 50, L's theme 11 to 11, with the same notes. The gaps left in the fuze intro are over a beat after the tail fades into hiss; a length rule can't tell those from a real rest.
