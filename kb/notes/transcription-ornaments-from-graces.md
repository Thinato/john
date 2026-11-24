---
title: Transcription finds cuts, taps and rolls in the salience, not the Viterbi path; graces are 3-4 frames and the path often skips them
tags: [transcription, audio, ornaments, cut, tap, roll, note-cleanup, synth]
---

# Ornaments from grace notes in transcription

## Context
Changing the ornament pass in `transcribe()` (`src/transcribe.cpp`, the `Grace` detection after the blip merge and the tagging after the re-articulation split, `RawNote::orn` / `QNote::orn`), or the playback graces in `perform()` (`src/sound.cpp`) that the "ornaments the synth plays" part of the `transcription` test in `tests/core/transcribe.cpp` relies on.

## Rule
A grace is under `minF` frames (60 ms) inside a merged note: a pitch +1..+7 (cut) or -1..-4 (tap) reaching 0.8x the note's salience and once 0.9x the frame's strongest pitch (a semitone grace must lead and reach 1.3x the note). The main pitch must stay at 0.25x or more during it, and lead alone (grace under 0.4x) within `minF` frames on both sides. A cut then a tap within 0.4 s inside one note = roll; in a note already marked (a cut on a repeated note, then a roll on the same pitch with no new attack) the roll is split off one cut-to-tap gap before its cut. Another grace near a note's start tags that note. One further in splits the note there (a cut between repeated notes). Only a whistle draft keeps the techniques; the report's `ornaments` line always lists them, located with `positions()`.

## Why
Issue #12. The synth's graces last 35 ms (cut +4, tap -2 semitones; roll graces at 1/3 and 2/3 of the note). At the 11.6 ms hop and 93 ms window that is 3-4 frames, the main note's salience dips to about 0.45x, and the Viterbi path takes a cut but usually stays on the main pitch through a tap. So the old pipeline never wrote grace notes. They showed up as extra splits at salience dips instead: a long roll came back as three notes. On the piano recordings in `tmp/`, half of the first draft's hits were semitone neighbours of about 1.0x (leakage) or had the main note vanish during the "grace" (another note's attack). The 1.3x semitone rule, the 0.25x floor and "main leads on both sides" cut fuze from 21 hits to 6.

## How to apply
Expect false candidates on piano and dense mixes. Candidates after the rules above: fuze 6, L's theme 2, Concerning Hobbits 3, with the same note counts as before (±1). A cut right after a different pitch is usually missed, by design (the main pitch must be on both sides). Rolls slower than 75 bpm (eighth over 0.4 s) come back as a cut and a tap on split notes (ponytail-marked). No crans: the synth's cran cuts don't form a pattern the pass knows. When tuning, print each grace (time, pitch, interval, frames, main dip) and compare the synthetic test at 70..140 bpm with the `tmp/` counts. Remove the print before committing.
