---
title: The song list shows what fits the Settings instrument (playability()); written pitches are compared and the whistle is fingered on the instrument's key
tags: [playability, song-list, instruments, range, transpose, gui, mcp, settings]
---

# Playability: songs that fit my instrument

## Context
Changing `playability()`, `fits()`, `PLAYABLE_PERCENT` or `rangeCost()` (`src/analyze.cpp`), the song list filter and the "Play in G major" button in `src/gui_main.cpp`, `Settings::instrument`/`whistle`/`harp`/`allSongs`, or `list_songs`/`analyze_song` in `src/mcp_main.cpp` (#84).

## Rule
A song's `instrument` is only what it was written for ("Written for" in the details panel). The user's instrument is `Settings::instrument` + `whistle` + `harp` (`myInstrument()` in the GUI, `playsOn()` in the MCP server). `playability(song, in)` compares the song's written pitches with `in`'s range. `rangeCost()` reads `in`'s note table (#85), which for a whistle is moved to `in.key`, not `song.whistle`. "Fits" means 0 notes out and `hard() * 10 < notes * PLAYABLE_PERCENT`, so a half-hole counts as one hard note and a cross-fingered C as a fifth. Import song's range hint uses the same constant. If shift 0 fits, the verdict is AsWritten. Otherwise each of the 12 tonics gets its octave from `keyShift()` (shared with `transposeToKey`), and the best is chosen by: fewest out, then fits, then nearest key (an octave shift counts as distance 0), then fewest hard notes.

## Why
The owner's idea: songs are melodies, so a fiddle tune that fits a D whistle must show up for a whistle player. Players read fiddle and whistle sheets at the same written pitch, so the comparison uses written pitch (the instrument's `octave` is ignored). Before this, `rangeCost` used `song.whistle`. That was correct only while the song's own instrument was passed, and it would judge a song for a C whistle player with D-whistle fingerings. Without the "nearest key" rule, a G tune that is an octave too low for the whistle came back as "in D" instead of "an octave up".

## How to apply
The GUI caches the verdict in `App::Meta::play`. It is computed again in `poll()` when the file's mtime changes, and for every song when `playFor` (instrument + whistle + harp) changes, because the list is drawn every frame. Never call `playability()` per frame for the whole list. The details-panel button uses the cached verdict only while `meta.mtime == a.songMtime`, so a second click before the next poll can't transpose twice. An OtherKey verdict with `shift % 12 == 0` is the same key an octave away: show "octave up/down", not "in <same key>". Bundled lessons and demo songs must be AsWritten on their own instrument (tested), or they would get a badge. Old `settings.json` files may still have `"songFilter"`; it is ignored, and the list defaults to "Playable on my ...". Since #85 `rangeCost` reads the profile's note table (see instrument-profile-files); harmonica songs are judged with real bends (harmonica-richter-layout).
