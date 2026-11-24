---
title: Bundled lessons and demo songs install once per id, then update by content hash only while the user's copy is untouched
tags: [lessons, demo, install, data, songs, web]
---

# Bundled songs: install once, smart update

## Context
Adding or changing files in `lessons/` (desktop and web) or `demo/` (web starter songs, #20), or touching `installSongs()` in `src/song.cpp`.

## Rule
`installSongs(from)` keeps one marker per bundled folder in the data folder (`lessons-installed.txt`, `demo-installed.txt`), one line per id: `id<TAB>hash`, the FNV-1a hash (`songHash()`) of the canonical JSON (`toJson(fromJson(file))` with the id set, so whitespace and key order don't count) of the bundled version last installed. A new id is copied and recorded. For an id already listed: a deleted copy stays deleted; a copy equal to the current bundle just records its hash; a copy still equal to the recorded hash (untouched) is replaced by the new bundled version; any other copy is the user's and only gets lyrics added since (`backfillLyrics()`, see lyrics-alignment-and-storage). Lines from before the hash (`id` alone) count as changed unless the copy equals the current bundle. Each folder has its own marker, so a new bundled folder installs even for users who already have the lessons.

## Why
Users may delete or change songs; re-copying everything would undo that. The owner wanted bundled songs to update on every deploy (2026-11-24), so untouched copies follow the bundle while edited ones are never guessed at. The marker name comes from the folder name so the lessons keep using the `lessons-installed.txt` that existing users already have.

## How to apply
Fixing a bundled song: just change the file; untouched copies update on the next start. Copies the user changed (techniques, key, notes) keep their version: to push a note fix to them too, give the song a new id (and say so in the PR). Don't rename the `lessons/` or `demo/` folders or their bundle paths (`/lessons`, `/demo` on the web): that changes the marker and re-installs everything. Writes inside `installSongs` skip the per-file `persist()` (`writeJson(..., sync = false)`) and it persists once at the end, only if something changed; a second run rewrites nothing (tested). Every bundled lesson and demo song must load and fit its instrument: `tests/test_core.cpp` checks this.
