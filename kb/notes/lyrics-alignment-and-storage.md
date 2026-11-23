---
title: Lyrics are stored per note (lyric, lyricHyphen, lyricHeld); ABC w: aligns to the notes since the previous w:
tags: [lyrics, abc, musicxml, song-format, gui]
---

# Lyrics: alignment and storage

## Context
Touching lyrics (#87): ABC `w:`/`W:` in `importAbc` (`src/abc.cpp`), MusicXML `<lyric>` in `importMusicXml`, `toAbc` (`src/transcribe.cpp`), the song JSON (`toJson`/`fromJson`), `listing()`, or the lyrics pass in `drawScore`.

## Rule
Each note `Event` carries the first verse only: `lyric` (the syllable), `lyricHyphen` (the word goes on to the next syllable) and `lyricHeld` (no syllable of its own: the previous one is held over it, drawn as an extender line). JSON keys `lyric`, `lyric_hyphen`, `lyric_held` are written only when set, and ABC `W:` lines go to `Song::words` (key `words`), so songs without lyrics save exactly as before. A `w:` line aligns to the notes from the end of the previous `w:` line (the start of the tune for the first), so one `w:` can cover several music lines; a `w:` with no music since the previous one is a second verse and is dropped with a warning. Rests and tied continuation notes (`tiedFrom()`) take no syllable; only having more syllables than notes warns, fewer is fine.

## Why
`w:` lines are recorded with their offset in the body text (`wLines`) and applied when the body parser reaches that offset, because the header loop collects the whole body before parsing it. A lone `-` (after a space or another `-`) is a syllable of its own in ABC 2.1: it skips a note and keeps the word going; a `-` after `_` or `*` is only a separator.

## How to apply
`toAbc` writes a `w:` line under every music line once the song has any lyric (even an empty `w:`), otherwise a later line's syllables would slide onto earlier notes on re-import; it skips tied continuations exactly as the importer does, so keep the two in step through `tiedFrom()`. In the GUI, lyrics sit under the note names (`lyricTop`), and a system only grows (by 2.4 staff spaces) when it has syllables; slot widths only grow for notes with a syllable, so sheets without lyrics lay out as before.
