---
title: Copy the key signature exactly: E minor with two sharps is K:Edor, not K:Em
tags: [abc, import, key, mode, accidentals, sheet-music]
---

# Match K: to the key signature, including the mode

## Context
Transcribing sheet music (photos, PDFs) into ABC for `import_abc`.

## Rule
The `K:` field sets the accidentals for every bar, so it must reproduce the printed key signature, not just the tonic. A tune that sounds like E minor but has F# and C# in its signature is E dorian: `K:Edor` (2 sharps). `K:Em` gives only F#, so every C silently becomes C natural.

## Why
The Pied Piper transcription first used `K:Em`; the sheet's C# notes came out as C natural without any warning, because the importer trusts the key signature.

## How to apply
Count the sharps/flats on the sheet and pick the mode that has that many for the tonic: maj, min (aeolian), dor, mix, lyd, phr, loc all work in `K:`. After import, spot-check a few accidentals in `get_song` against the image.
