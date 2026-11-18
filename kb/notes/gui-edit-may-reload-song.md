---
title: In the GUI, editSong() can reload the song, so references into it may be stale afterwards
tags: [gui, imgui, editing, references, crash]
---

# Don't keep using song references after editSong()

## Context
GUI code in `src/gui_main.cpp` that changes the current song from inside a loop (combos, checkboxes, technique lists).

## Rule
`editSong()` first reloads the song from disk if the file changed (the agent may have edited it), and `open()` can even leave `App::song` empty when the file was deleted. After calling it, don't touch anything taken from the song earlier in the frame: copy the values you need into locals before the call, then leave the loop, and don't use `Song&` references held by the enclosing panel for the rest of the frame (return, or re-check `a.song`).

## Why
`App::song` is a `std::optional<Song>`; a reload replaces its contents, and a deleted file empties it, so `Song&`, `Event&` and iterators taken earlier can point at changed or destroyed data.

## How to apply
Pattern: `std::string pick = f;` before `editSong(a, [&](Song& x) { ... pick ... });`, then `break;`. The Instrument combo and the technique checkboxes do this. `detailsPanel` still has two gaps (the Folder combo doesn't break, and `Song& s` is used after the combos), tracked in #30.
