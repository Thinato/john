---
title: In the GUI, hints, range markers, playback and the mic follow my instrument (myInstrument), not the song's; the `whistle` flag means "show whistle hints"
tags: [gui, hints, fingering, instruments, out-of-range, settings, playback, mic]
---

# Instrument hints vs out-of-range markers in the GUI

## Context
GUI code that draws fingering charts or fingering text (`drawScore` and the note tooltip in `src/gui_score.cpp`, `detailsPanel` and its legend in `src/gui_panels.cpp`, the mic readout in `transportBar` in `src/gui_practice.cpp`), the "Instrument hints" checkbox (`songList()`), the Sound combo, the mic pitch range, or adds a new hint.

## Rule
Everything about playing uses `myInstrument(a.set)` (Settings' instrument + whistle + harp, #90), never `instrumentOf(song)`: each hint spot computes `whistle = me.hints && a.set.hints` (`Instrument::hints`, from the profile, #85) and fingers with `finger(me.key, midi)`, not `song.whistle`. Out-of-range markers (red note, "Out of range for the ...") use `me.plays(midi)` in the non-hint branch, and they stay when hints are off. `instrumentOf(song)` is only for what describes the sheet: "Written for", the song's "Harmonica key" picker, the header line, Play in key's octave choice (`transposeToKey`), the import range hint, the song list's Meta.

## Why
The owner's model (#84, #90): songs are melodies and the song's instrument is only "written for", so a whistle player opening a fiddle tune gets whistle fingerings, whistle range, a whistle sound and whistle-octave grading. The "Instrument hints" checkbox (#16) hides fingerings without hiding range problems. That works because the whistle's `Instrument` range (bottom note to +23) equals the range where `finger(me.key, ...)` returns `ok`, so the non-hint branch flags the same notes in red. When my instrument has no hints the checkbox is disabled and drawn unticked (a local `shown` copy), while `a.set.hints` keeps the choice for when I switch back to a whistle.

## How to apply
Gate any new hint (fingering, "blow harder", hand positions) on that same `whistle && a.set.hints` value from my instrument, never on the song's instrument. When hints are added for another instrument, set `hints` in its profile and teach the hint spots its charts (today `hints` means tin whistle charts). The harmonica's 1st/2nd position line shows for my harp (`a.set.harp`; `Instrument::key` is empty for harmonicas). If the whistle range or `finger()` ever changes, keep the two ranges equal. The choice is a saved setting (`Settings::hints`, #18); the Settings window has its own plain checkbox for it. The MCP `get_song` listing (`listing()` in song.cpp) and `analyze_song`'s range report still use the song's instrument: they describe the sheet for the agent editing it. `analyze_song`'s technique candidates (#86) and the marks drawn on the sheet follow my instrument (technique-marks-follow-my-instrument).
