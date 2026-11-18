---
title: Viewer text goes through tr()/trf()/L() with the English as key; i18n/<code>.json adds a language; MCP stays English because only the GUI calls setLanguage()
tags: [i18n, translation, language, gui, mcp, portuguese]
---

# Translating the viewer (tr, i18n/*.json)

## Context
Adding or changing any text the viewer shows (`src/gui_main.cpp`, and the core text it displays: `keyName()`, `durationName()`, `defaultTechniques()`, instrument and folder names), or adding a language (#23). Code: the "viewer language" section of `src/john.hpp`, its implementation at the end of `src/song.cpp`, files in `i18n/`.

## Rule
The English text is the key: `tr("Play")` returns the translation from the current `i18n/<code>.json`, or the English when there is none. `trf("%s (song)", name)` translates then formats (printf, checked by the compiler against the English). A widget label uses `L("Folder")` (see gui-imgui-id-translated-labels). A language is one file `i18n/<code>.json` (`"English": "translation"` pairs plus `"@language": "its own name"` for the Settings combo); desktop reads it from `i18n/` next to the executable (release package) or else the source tree (`bundled("i18n", JOHN_I18N)`), the web bundles `i18n/` with a `--preload-file`. The MCP server stays English because `setLanguage()` and `setSolfege()` are global state that only `main()` in the GUI calls; until then `tr()` returns its argument and `noteName()` gives letters.

## Why
gettext style needs no ids and no English file, and a missing entry can't break the UI. Core functions like `keyName()` and `durationName()` are shared with the MCP server (`listing()`, `analyze()`, `list_songs`), so they translate through `tr()` instead of the GUI translating at every call site.

## How to apply
New viewer text: wrap the literal in `tr()`/`trf()`/`L()` and add the Portuguese to `i18n/pt-BR.json`; the core test fails if a literal `tr("...")`, `trf("...")` or `L("...")` in `gui_main.cpp`/`song.cpp` has no entry, if a value is empty, or if its `%` conversions differ from the key's. Never pass `tr(...)` as the only argument of a printf-style ImGui call (`Text`, `SetTooltip`, `TextDisabled`): use `"%s", tr(...)`, since a `%` in a translation would be read as a format. Text built from data (folder names, `MODE_FMT`, duration names) is translated with `tr(variable)`: add those keys by hand. Automatic language: `systemLanguage()` reads `LC_ALL`/`LANG`, on macOS falls back to `CFLocaleCopyPreferredLanguages()` when both are empty (apps started from Finder get no `LANG`; this is why CoreFoundation is linked to `john_core` on Apple), and on the web reads `navigator.language`; `matchLanguage()` maps the result to a file (exact code, then the same base language, else "en") and is what the test covers, since the CF value can't be faked in a test. To try the macOS path: `env -u LANG -u LC_ALL ./build/john -AppleLanguages '(pt-BR)'`. Note names and key names are separate settings: `setSolfege(notes, keys)` from `Settings::solfege` and `Settings::solfegeKeys`. Keys (`keyName()`, and `tonicName()` for the whistle key in instrument names) use Do Re Mi only when a non-English file is loaded (`setLanguage()` returned true), so English always says "D major". The Settings checkbox for keys is hidden in English (`App::english`). Never call `setLanguage()`/`setSolfege()` from `mcp_main.cpp` or core code. Song titles, notes and comments are user data and are not translated. Lessons and demo songs in other languages are separate songs, not `tr()` entries (#47).
