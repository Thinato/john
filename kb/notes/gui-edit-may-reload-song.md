---
title: In the GUI, editSong() can reload the song, so references into it may be stale afterwards
tags: [gui, imgui, editing, references, crash]
---

# Don't keep using song references after editSong()

## Context
GUI code in `src/gui_main.cpp` that changes the current song from inside a loop (combos, checkboxes, technique lists).

## Rule
`editSong()` first reloads the song from disk if the file changed (the agent may have edited it), and `open()` can even leave `App::song` empty. After calling it, stop iterating over the song's data: copy values you need into locals before the call, and `break` out of the loop right after.

## Why
`App::song` is a `std::optional<Song>`; a reload replaces its contents, and a deleted file empties it, so `Song&`, `Event&` and iterators taken earlier can point at changed or destroyed data.

## How to apply
Follow the existing pattern: `std::string pick = f;` before `editSong(a, [&](Song& x) { ... pick ... });` then `break;`. The Folder, Instrument and technique-checkbox widgets do this.
