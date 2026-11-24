---
title: transcribe instrument = what the sheet is for; recording_instrument = who plays the melody, and only it limits the pitch search
tags: [transcription, audio, instruments, accompaniment, pitch-search]
---

# instrument vs recording_instrument in transcription

## Context
Changing `transcribe()` (`src/transcribe.cpp`), `TranscribeOptions` (`src/transcribe.hpp`) or the `transcribe_audio` tool, or deciding which instrument option to pass.

## Rule
`instrument` is the instrument the song is written for: it only picks the octave fit, the automatic transposition when no octave fits (see transcription-transpose-before-folding.md) and folding into its range. `recordingInstrument` (`recording_instrument` in the tool) is the instrument playing the melody in the recording: when set, semitone salience outside its sounding range (`low`/`high` + 12 * `octave`, ±2 semitones, clamped to the search range, C3..E7 since #40) is zeroed after the octave-ghost suppression, before the normalisation and the Viterbi. Unset = the full search range, byte-identical to before #10. The whistle uses the D whistle (the tool has no whistle key option).

## Why
Piano recordings are often transcribed into whistle sheets, so the written-for instrument must not limit what is heard (settled in #10). On Concerning Hobbits the unlimited search flips to the louder A-chord accompaniment (A3, C#4, E4) between whistle notes and puts the whole draft an octave too high; limited to the whistle it keeps the whistle line and goes silent where other instruments carry the melody.

## How to apply
Pass `recording_instrument` only when the solo instrument is known. The limit can't remove accompaniment harmonics that fall inside the range: in the synthetic test (whistle tune under chords 8 dB louder) the limited search finds every note but still adds low D and G blips (harmonics of the G chord) at chord attacks and in melody rests, and at 13 dB louder it lost 11 of 26 melody notes too. A fix for that would need a harmonic model of the out-of-range notes, not a narrower range.
