---
title: Bundled lessons and demo songs are installed once per id; edits to an existing one don't reach users who have it
tags: [lessons, demo, install, data, songs, web]
---

# Bundled songs install once, by id

## Context
Adding or changing files in `lessons/` (desktop and web) or `demo/` (web starter songs, #20).

## Rule
`installSongs(from)` in `src/song.cpp` copies each `<from>/<id>.json` into the user's song folder only if `<id>` is not yet listed in `<folder name>-installed.txt` in the data folder (`lessons-installed.txt`, `demo-installed.txt`). A song the user deleted stays deleted, and editing an already-installed one changes nothing for existing users, except lyrics: added later to a bundled song, they reach a user's copy that has the same notes and no lyrics (`backfillLyrics()`, see lyrics-alignment-and-storage). Each folder has its own marker, so adding a new bundled folder installs it even for users who already have the lessons.

## Why
Users may delete or change songs; re-copying them on every start would undo that. The marker name comes from the folder name so the lessons keep using the `lessons-installed.txt` that existing users already have.

## How to apply
New lesson or demo song: add a new file with a new id. Fixing one: changing the file only helps new users; to push a fix to everyone, give it a new id (and say so in the PR). Don't rename the `lessons/` or `demo/` folders or their bundle paths (`/lessons`, `/demo` on the web): that changes the marker and re-installs everything. Every bundled lesson and demo song must load and fit its instrument: `tests/test_core.cpp` checks this.
