---
title: Long work started from the GUI runs in std::async and is polled each frame; the language and solfege stay frozen while it runs
tags: [gui, threading, transcription, import-audio, i18n]
---

# Long work in the GUI (Import audio)

## Context
Adding anything to `src/gui_main.cpp` that can take more than a frame (transcription, decoding, scanning many files). Today only Import audio (#4) does: `importWindow()` starts `importAudio()` from `src/import_audio.cpp`.

## Rule
Start the work with `std::async(std::launch::async, ...)` into a `std::future` member of `App` and set `App::busy`. Check it with `wait_for(0)` at the top of the window function every frame, then call `get()` inside `try` so the worker's exception becomes a message. Copy every input into the lambda (the path as a `std::string`, not `App::importPath`), and don't let the worker touch `App` or ImGui. While `App::busy` is set, `main()` doesn't call `setLanguage()`/`setSolfege()`, because the worker reads them through `tr()`, `keyName()` and `noteName()`.

## Why
`glfwWaitEventsTimeout(0.5)` idles the loop, so a finished future is noticed within half a second without forcing `live` frames. The language map and the solfege flags are plain globals: switching them while a worker translates is a data race. `setSolfege()` used to run every frame, so it had to move behind the same guard. The future's destructor waits for the work to finish, so quitting during a transcription closes the window but the process stays until the work is done (a few seconds).

## How to apply
Desktop only: emscripten's libc++ has no threads, so the `<future>` include, the members and the window sit behind `#ifndef __EMSCRIPTEN__` (`App::busy` is shared so `main()` compiles on both). Core functions the worker calls must only write their own files through `writeJson()` (write then rename), and must not call `loadSettings()` for options the GUI already holds: pass them in.
