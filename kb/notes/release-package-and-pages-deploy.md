---
title: Release packages find lessons/ and fonts/ next to the executable via bundled(); Pages and releases deploy only from CI
tags: [release, packaging, ci, web, pages, lessons, fonts]
---

# Release packages and the Pages deploy

## Context
Changing `.github/workflows/release.yml` or `pages.yml`, adding a runtime file the desktop apps need (lessons, fonts, data), or touching `bundled()`/`lessonsDir()` in `src/song.cpp` or the font loading in `main()` of `src/gui_main.cpp` (#21).

## Rule
`JOHN_LESSONS`, `JOHN_I18N`, `JOHN_FONT` and `JOHN_TEXT_FONT` are absolute build-time paths (source tree, build dir). A downloaded app has neither, so `bundled(rel, built)` first looks for `rel` in the running executable's folder (`_NSGetExecutablePath` on macOS, `/proc/self/exe` on Linux) and uses the build-time path only when that file is missing. The release zip/tar.gz (`john-macos`, `john-linux`) is a flat folder: `john`, `john-mcp`, `lessons/`, `i18n/` (`i18nDir()`), `fonts/` (Bravura.otf, NotoSans-Regular.ttf and their OFL license texts). The macOS build is universal (arm64 + x86_64) with a macOS 11 minimum. Pages publishes `build-web/` with `john.html` copied to `index.html`.

## Why
Before #21 a copied binary installed no lessons and fell back to ImGui's default font, with no music glyphs. Dev builds still work unchanged because `build/` has no `lessons/` or `fonts/` folder. On the web the executable folder is empty, so the bundled `/lessons` and `/fonts` paths are used. Without `CMAKE_OSX_DEPLOYMENT_TARGET`, the binary would require the runner's macOS version.

## How to apply
A new runtime file goes in the Package step of `release.yml` and is looked up with `bundled()`. Windows has no executable-folder lookup yet (#50). Tags (`v*`) create releases, and pushes to `main` deploy Pages. Never push a tag, create a release or run either workflow by hand from an agent. A manual run of `release.yml` builds and uploads artifacts without making a release. Pages needs Settings → Pages → Source: GitHub Actions, set once. To test packaging locally, extract the job's `run:` steps from the YAML with `python3 -c "import yaml"`. Build in the scratchpad, then unpack the archive elsewhere and hide the build folder. Add a package-only marker lesson to `lessons/` and check that `john-mcp`'s `list_songs` shows it. Linux can be checked the same way in `docker run ubuntu:24.04`, and `rhysd/actionlint` (Docker) lints the workflows. A matrix value holding `;` (e.g. `CMAKE_OSX_ARCHITECTURES`) must be quoted inside the value, or bash splits the command there.
