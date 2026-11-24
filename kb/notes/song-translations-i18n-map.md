---
title: Bundled songs carry their own translations in an "i18n" map keyed by the exact English; songText() shows them, an edited text falls back to English
tags: [i18n, translation, lessons, demo, songs, portuguese, gui]
---

# Song translations (the song's "i18n" map)

## Context
Showing or changing a song's title, notes or note comments in the viewer, and adding or editing a lesson (`lessons/`) or demo song (`demo/`) (#47). Code: `Song::i18n` in `src/song.hpp`, `songText()` in `src/i18n.hpp`, `toJson()`/`fromJson()` in `src/song.cpp`, the display sites in `src/gui_main.cpp` (song list titles in `poll()`, `detailsPanel()` title and notes, the note tooltip, the comment box).

## Rule
A song file may have `"i18n": {"pt-BR": {"<English text>": "<translation>"}}` covering its title, notes and comments. `Song` keeps the map and `toJson()`/`fromJson()` round-trip it, so MCP and GUI edits keep it. The GUI shows `songText(s, english)`: the entry for the language `setLanguage()` loaded, else the English. The MCP server never calls `setLanguage()`, so it always shows the English. The test fails if any bundled lesson or demo song lacks a pt-BR entry for its title, notes or any comment.

## Why
Keying by the exact English (gettext style, like `tr()`) needs no event indices: when the agent or the user changes a comment, the old entry simply stops matching and the new text shows as written, so a translation can't go stale or land on the wrong note. Translations inside the same file keep the once-per-id install working (no second copy per language).

## How to apply
After changing any English title, notes or comment in `lessons/` or `demo/`, add the matching Portuguese entry (old entries for removed text can be deleted). Use the terms of `i18n/pt-BR/` (flauta irlandesa, digitação, semínima, colcheia, andamento, golpe de língua, ornaments as "corte (cut)", "rolo (roll)", "deslize (slide)"). Note letters stay letters (D grave, F# agudo) as in the technique descriptions; keys use Do Re Mi ("Ré maior") as `keyName()` does. Tune names known in English stay English; the title entry then equals the English. The GUI comment box shows the translation, so *Save comment* writes only when the box differs from `songText()` of the stored comment: saving it unchanged would replace the English with the Portuguese and lose the translation. A real edit replaces the comment (the user's own text) and the old entry stops matching. The song list caches titles per file mtime, so `applyLanguage` in `main()` resets those mtimes and forces a poll when the language changes. Changes reach only new installs (see lessons-install-once-per-user).
