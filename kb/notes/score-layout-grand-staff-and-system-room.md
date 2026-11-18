---
title: Score systems have per-system heights (sysY) and piano gets a grand staff; place things with staffTop/yOf, never sys * sysH
tags: [gui, score, layout, staff, piano, grand-staff, drawScore]
---

# Score layout: grand staff and system room

## Context
Changing `drawScore` in `src/gui_main.cpp`: anything placed relative to a line of music (notes, ledger lines, charts, note names, hover boxes, volta brackets, the playback cursor).

## Rule
Systems no longer share one height. Each system's top is `sysY[sys]` (height `sysHOf(sys)`); `staffTop(sys)` is the treble staff's top line, `yOf(sys, step, bass)` the y of a diatonic step on either staff, and `bottom` in the event loop is the lowest staff's bottom line. The extra room comes from core `systemRoom()` (in steps: 6 above the top line and 6 below the bottom line fit, 4 above a whistle fingering chart). Piano songs (`grandStaff()`) draw a treble and a bass staff joined by a brace; `onBassStaff()` puts notes below middle C (MIDI 60) on the bass staff, and `Slot::bass` records it.

## Why
Issue #25: notes far from the staff (piano A0..C8, guitar low E3) overlapped the note names and the next system. Today's spacing is kept when nothing is far, so whistle sheets look exactly as before; any code still using `origin.y + sys * sysH` lands in the wrong place on a grown system.

## How to apply
Use `sysY`/`staffTop`/`yOf(..., sl.bass)` for new drawing and hit-testing. A note's staff decides its stem direction (middle line step 34 treble, 22 bass), its ledger lines (from bottom line 30 or 18) and its beam group: beams break when the staff changes, there are no cross-staff beams. Rests, techniques, breath marks and tuplet numbers stay on the treble staff. The bass key signature is the treble one two octaves (14 steps) lower. If something new hangs below the notes, raise `systemRoom`'s `free` argument for that case rather than adding fixed space, or every sheet changes.
