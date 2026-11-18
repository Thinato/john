---
title: In ABC, !breath! marks the note BEFORE it, not the note it is written on
tags: [abc, import, breath, techniques, transcription]
---

# !breath! attaches to the previous note

## Context
Writing ABC for `import_abc` (sheet photos, thesession.org, audio transcriptions, lessons) with breath marks.

## Rule
The importer puts a `!breath!` decoration on the most recently parsed note (`src/abc.cpp`, the `t == "breath"` branch), because a breath is taken after a note. Write it right before the note that follows the breath, or before a rest: `A2 B2 !breath! c4` breathes after B.

## Why
Every other decoration (`!roll!`, `!slide!`, `{g}` cuts) attaches to the next note, so it is natural to write `B2!breath!`-style or put it on the wrong note. Breaths ended up one note late in several songs and lessons before this was noticed.

## How to apply
After importing anything with breaths, check `get_song`: the `breath` technique must sit on the last note before the pause. Fix misplaced ones with `set_techniques` (remove + add) rather than re-importing.
