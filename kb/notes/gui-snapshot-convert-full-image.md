---
title: Snapshot the GUI with scripts/snap.sh and JOHN_SNAPSHOT_* (open windows, hover, cursor, fake mic), never by patching
tags: [gui, testing, snapshot, sips, verification, mic, range, lyrics]
---

# Verifying the GUI with snapshots

## Context
Checking a GUI change visually (no screenshots of the user's screen are allowed), including windows and states you would normally reach by clicking: Settings, the instrument editor, Find my range, Import song, a note's tooltip, the play-along cursor, mic marks.

## Rule
`scripts/snap.sh <out.png> [VAR=value...]` runs `build/john` with `JOHN_SNAPSHOT`, a fresh data folder (bundled lessons plus the `demo/` songs, at the same path every run: `$TMPDIR/john-snap/<out name>`) unless you pass `JOHN_HOME=<dir>`, and converts the whole `.tga` (kept next to the PNG) with `sips`. The `JOHN_SNAPSHOT_*` variables are read once in `snapSetup()` (`src/gui_main.cpp`), only when `JOHN_SNAPSHOT` is set:

| Variable | Effect |
|---|---|
| `JOHN_SNAPSHOT_SONG=<id>` | open this song (and stop following the newest one) |
| `JOHN_SNAPSHOT_OPEN=a,b` | open windows: `settings`, `instrument-editor:<id>` (as Settings' Edit), `range-test` (for my instrument), `import` (Import song), `import-audio` |
| `JOHN_SNAPSHOT_HOVER=<event>` | moves the fake mouse onto that note: highlight and tooltip as if hovered |
| `JOHN_SNAPSHOT_CURSOR=<event>` | the play-along cursor and the lyric highlight on that event |
| `JOHN_SNAPSHOT_MIC=<Hz,...>` | with `range-test`: the low and high take's pitch (`sungNote()`). Otherwise play-along with the mic "on": the k-th Hz grades the k-th note played (`judgeNote()`, 0 = silence), the last is the "hearing" readout. The real mic is never opened. |
| `JOHN_SNAPSHOT_FRAMES=<n>` | write frame n instead of 5 (worker threads, auto-resizing windows) |

Event indexes are positions in the song's `events` array (`get_song`, or the JSON).

## Why
Snapshot mode can't click, so PRs #95, #96 and #98 patched the code to open windows, hover or fake the mic. The real mouse used to leak in: the hover highlight and tooltip followed wherever the cursor was, so two runs differed (#101). In snapshot mode the GLFW backend's input callbacks are not installed and every frame overrides the polled cursor (`io.AddMousePosEvent`), so the same command gives a byte-identical `.tga` (same data folder path: it is printed in the image) and no variable means the same image as before.

## How to apply
- Settings: `snap.sh s.png JOHN_SNAPSHOT_OPEN=settings`. Import song / audio: `OPEN=import` / `OPEN=import-audio`.
- Instrument editor: put a profile in `<home>/instruments/<id>.json` (instrument-profile-files), then `JOHN_HOME=<home> JOHN_SNAPSHOT_OPEN=settings,instrument-editor:<id>`.
- Find my range: a home whose `settings.json` is `{"instrument":"voice-alto"}`, then `JOHN_HOME=<home> JOHN_SNAPSHOT_OPEN=range-test JOHN_SNAPSHOT_MIC=174.6,698.5` (F3, F5). States reached only by Sing or Save (listening, errors) still need a local patch.
- Tooltip: `JOHN_SNAPSHOT_SONG=the-silver-spear JOHN_SNAPSHOT_HOVER=6`. Lyric highlight: `JOHN_SNAPSHOT_SONG=down-by-the-sally-gardens JOHN_SNAPSHOT_CURSOR=11`.
- Play-along marks: `JOHN_SNAPSHOT_SONG=the-silver-spear JOHN_SNAPSHOT_MIC=440,370,440,880,440,466,0,494`.
- Other settings (language, theme, hints off, my instrument): write them in `<home>/settings.json` and pass `JOHN_HOME`.
- Prove a change leaves an image unchanged with `cmp` on the two `.tga` files; for a real difference, `magick compare -metric AE a.png b.png diff.png`. Don't crop with `sips -c`: its offsets picked the wrong region.
- A new window or state worth snapshotting gets its own word or variable in `snapSetup()`/`snapFrame()`, plus a line here, rather than a patch. Only hook points go in other `gui_*.cpp` files (like `App::snapHover` in `drawPaper()`).
