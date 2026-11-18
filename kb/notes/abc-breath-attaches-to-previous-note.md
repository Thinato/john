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
Every other decoration (`!roll!`, `!slide!`, `{g}` cuts) attaches to the note after it, so it's natural to write `!breath!B2` meaning "breathe after B". That puts the breath on the note before B instead. Breaths ended up on the wrong note in several songs and lessons before this was noticed.

## How to apply
After importing anything with breaths, check `get_song`: the `breath` technique must sit on the last note before the pause. Fix misplaced ones with `set_techniques` (remove + add) rather than re-importing.
