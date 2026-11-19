---
title: Check GUI changes with JOHN_SNAPSHOT and convert the whole TGA; sips cropping is unreliable
tags: [gui, testing, snapshot, sips, verification]
---

# Verifying the GUI with snapshots

## Context
Checking a GUI change visually (no screenshots of the user's screen are allowed).

## Rule
Run `JOHN_HOME=<tmp> JOHN_SNAPSHOT=<tmp>/s.tga ./build/john`, then `sips -s format png <tmp>/s.tga --out <tmp>/s.png` and look at the full image. Don't crop with `sips -c`: its offsets picked the wrong region. To show a particular song, put only that song in the temporary `JOHN_HOME/songs` and list every bundled lesson id in `JOHN_HOME/lessons-installed.txt` so lessons aren't copied in.

## Why
Snapshot mode renders a few frames and exits, with no way to click; an empty data folder otherwise fills with the bundled lessons, and the crop step wasted time showing the wrong area.

## How to apply
For a song-specific check, create it with `john-mcp` (`import_abc`) into the temp `JOHN_HOME` first, or edit its JSON there (e.g. change `instrument`) between snapshots. Snapshot mode can't click, so to see a GUI toggle in its other state (e.g. "Instrument hints" off), flip the `App` member's default for a local build only, snapshot, then revert it and rebuild. To prove a layout change leaves a sheet unchanged, compare before/after with `magick compare -metric AE a.png b.png diff.png` and look at `diff.png`: the hover highlight and tooltip follow wherever the real mouse is, so they can differ between runs even when the score is identical. To snapshot the result of Import audio (or other work on a worker thread), patch locally: fill the `App` fields, trigger the button once, and take the snapshot only after a few frames with `!a.busy`, not at frame 5. An `AlwaysAutoResize` window sizes itself from the previous frame, so text that appears in the frame the future finishes is clipped until the next frames.
