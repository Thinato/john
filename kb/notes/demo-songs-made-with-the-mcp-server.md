---
title: Demo songs (demo/) are made with john-mcp from public-domain sources and installed on the web only
tags: [demo, songs, web, mcp, abc, thesession, copyright]
---

# Making the demo songs

## Context
Adding or fixing a starter song in `demo/` (#20). The folder is bundled only in the web build (`--preload-file .../demo@/demo` in `CMakeLists.txt`) and installed by `installSongs("/demo")` in `main()` of `src/gui_main.cpp`; desktop users have the agent instead.

## Rule
Each file is a song saved by `john-mcp`, not hand-written JSON: run the built server with `JOHN_HOME` set to a temp folder, drive it over stdin JSON-RPC (one request per line, as in `tests/mcp_smoke.sh`), `import_abc` (with `id`, `source`, `instrument`, `folder`), `update_song` tempo, `analyze_song`, `set_techniques` with a short "why" comment on every ornamented note, `update_song` notes, then copy `songs/<id>.json`. Only traditional or public-domain melodies; `source` names the thesession.org setting URL or the composer and date.

## Why
Going through the server gives exactly the format, ids and positions the app expects, and follows the agent's own GUIDE (musical judgment, few ornaments). The temp `JOHN_HOME` also receives the lessons on start, so copy only the demo ids into `demo/`. Copyrighted material must never be committed; modern arrangements of old tunes count as copyrighted.

## How to apply
Check the result with `get_song` before copying: a breath goes on the last note before the pause (on a tied note, the second half), see `abc-breath-attaches-to-previous-note.md`. For harmonica, write the tune in the octave the C diatonic plays without bends (holes 4-6 are C5..A5); C4..B4 needs bends on holes 2 and 3. After changing `demo/`, rebuild the web build: the bundle is made at link time.
