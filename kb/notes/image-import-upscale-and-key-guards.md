---
title: Image import guards - upscale pages with staff space under 10 px 2x on a canvas; checkFirstLineKey gives line 1 the later key copisteria missed
tags: [import, omr, image, copisteria, key, musicxml, accuracy]
---

# Image import accuracy guards (upscaling, first-line key)

## Context
Changing `readPage()` in `web/omr/worker.js`, `checkFirstLineKey()` in `src/musicxml.cpp`, or the `<print new-system>` line John added to `web/omr/copisteria/write.js` (#75, measured in #74).

## Rule
Upscale: the worker measures the staff space with copisteria's `findStaves()` and draws the page 2x on an `OffscreenCanvas` (`imageSmoothingQuality = "high"`) when it is under 10 px and the result stays within 4096 px. Key: copisteria sometimes reads no key signature on the first line only. `write.js` now starts every later line with `<print new-system="yes"/>`; `checkFirstLineKey()` reports when the first line's key differs from one key that starts exactly on line 2 and holds to the end, and fixes it (line 1's notes without a written accidental, or one carried in the bar, take the later key) only when line 1 has no sharps or flats and has no written accidental that the later key would make unneeded (e.g. a written F sharp under a D major rest).

## Why
On O'Neill's 1903 scan (865 px, staff space 6.4 px) copista-2m read 74.7 % of pitches as is, 97.2 % after a 2x canvas or ImageMagick enlargement, but only 87.8 % after the port's own `resizeGrayLanczos()` 2x (it split the page into two parts). `findStaves()` reported 6.5 px again on an ImageMagick-enlarged copy and a second 2x read worse, hence the 4096 px cap. The missed key cost Drowsy Maggie 4 F#/C# notes (95.4 %); a first plan to fix only at a plain bar line failed there, because its first line is the whole A part and ends on a repeat. A real key change at line 2 exists (O'Neill 966: no key on line 1, one sharp from line 2), and it can't be told from a miss in the MusicXML; but John keeps one key per song anyway, and the warning always asks the user to compare with the original shown next to the sheet.

## How to apply
Measure any change with clean renders, a phone-like photo and the scan (see the PR of #75 for the table), scoring the imported song against the known one. Keep the fix in core, where `tests/core/musicxml.cpp` (the `musicxml` test, #75 part) covers fixed, repeat, written-sharp veto, own-key and nothing-to-report cases. Only the image import calls `checkFirstLineKey()`: a MusicXML file from a notation program states its keys correctly and must not be rewritten. A tied note (`<tie type="stop">`) keeps its pitch, since copisteria copies the first note's alteration to it. Not handled yet: copisteria reads each PDF page on its own, so a key missed on the first line of page 2 or later gives more than two key changes and only John's generic "key change ignored" warning.
