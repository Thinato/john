---
title: The web build keeps data in IndexedDB via IDBFS; every file write must end in persist(), and the frame loop must never block
tags: [web, emscripten, storage, indexeddb, gui, build]
---

# Web build (emscripten)

## Context
Changing anything the browser version of the viewer uses (#19): `emcmake cmake -S . -B build-web && cmake --build build-web` builds only `john` (no `john-mcp`, no tests) to `build-web/john.html/.js/.wasm/.data`. Files: the `if(EMSCRIPTEN)` blocks in `CMakeLists.txt`, `web/pre.js`, `web/shell.html`, `dataDir()`/`persist()` in `src/song.cpp`, and the `__EMSCRIPTEN__` blocks in `main()` of `src/gui_main.cpp`.

## Rule
On the web `dataDir()` is `/john`, an IDBFS folder that `web/pre.js` mounts and loads from IndexedDB before `main()` runs. Files there live in memory until `persist()` (`FS.syncfs(false)`) copies them to IndexedDB; `writeJson()` (so `saveSong`, `saveSettings`, `saveCustomTechnique`) and `installLessons()` call it. Fonts and `lessons/` are bundled in `john.data` at `/fonts` and `/lessons` (`--preload-file`). The frame loop is the `step` lambda in `main()`: the desktop calls it in a `while`, the web hands it to `emscripten_set_main_loop_arg`.

## Why
Without the sync, a save only changes memory and is lost on reload. The browser owns the event loop: a blocking loop freezes the tab, and the contrib.glfw3 port's `glfwWaitEvents`/`glfwWaitEventsTimeout` do nothing, so they don't process input or canvas resizes either. That's why `main()` forces `live = true` (always `glfwPollEvents()`) on the web. CMake removes repeated link options, so a second `--preload-file` disappeared until the options were wrapped in `"SHELL:..."`.

## How to apply
A new code path that writes or deletes a file in `dataDir()` must call `persist()` afterwards. Don't add blocking waits, threads or sleeps to the GUI loop, and don't rely on `JOHN_HOME`, `JOHN_SNAPSHOT` or build-time absolute paths on the web. A new bundled file needs a `"SHELL:--preload-file src@/dst"` link option. Audio is miniaudio's Web Audio backend; the device opens on the Play/Mic click and miniaudio resumes the context on the next click if the browser started it suspended. To test: `python3 -m http.server -d build-web 8000`, open `http://localhost:8000/john.html` in a chrome-devtools page with an `isolatedContext` (fresh IndexedDB), take a page screenshot (not the screen), click by dispatching `MouseEvent`s (mousemove, mousedown, mouseup with `clientX/clientY`) on `#canvas`, and read files with `FS.readFile('/john/settings.json', {encoding: 'utf8'})` before and after a reload. Never click Play or Mic in automated checks. Wait about 250 ms between the dispatched events: with shorter gaps a click was sometimes missed because no frame ran in between. Text or controls that only make sense with the agent (john-mcp) or a data folder on disk are hidden with `#ifdef __EMSCRIPTEN__` ("Follow agent", the empty-list text, the Data folder line, Settings → New songs, #44); on the web the sidebar shows a "Get the desktop app" link there instead, opened with `emscripten_run_script("window.open(...)")`. New agent-related UI needs the same guard. To see the empty-list text, `FS.unlink` the files in `/john/songs` from the page without calling `persist()`, and reload to get them back.
