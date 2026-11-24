---
title: Web image import runs in web/omr (omr.js + a module Worker); C++ polls omrState() and imports /omr/result.musicxml from MEMFS
tags: [web, emscripten, import, omr, image, pdf, worker, cache, em_js]
---

# Web image import: the Worker and the Emscripten bridge

## Context
Changing the web Import song window's image/PDF path (#75): `pickFile()`, `omrState()`, `omrCancel()`, `importRead()`, `originalPanel()` in `src/gui_import.cpp`, or `web/omr/omr.js` (main thread) and `web/omr/worker.js` (the reader).

## Rule
`pickFile()` (EM_JS) writes ABC/MusicXML picks to `/picked` as before and hands images and PDFs to `import('./omr/omr.js')` with `FS`. `omr.js` renders pages (pdf.js for PDFs, `createImageBitmap` for images), sends them one at a time to the module Worker `worker.js`, and keeps a plain `state` object (`stage`: idle/open/download/start/read/done/error, page, pages, ratio, loaded, total, error code). C++ reads it every frame as JSON through `omrState()` (malloc'd by `stringToNewUTF8`, so `free()` it). When done, `omr.js` writes `/omr/result.musicxml` and `/omr/original` (uint32 width, height, then RGBA, the first page at most 2048 px) to MEMFS; `abcWindow()` imports the file through `checkFirstLineKey()` and `importText()`, makes a GL texture of the original for the new song ids (this session only), and calls `omrCancel()`, which also resets `state` to idle. Error codes, not messages, cross the bridge, so C++ translates them with `tr()`.

## Why
The reader is JavaScript (copisteria's port on onnxruntime-web), so it can't live in the C++ module; files in MEMFS and one JSON poll keep the bridge as small as the `/picked` path (no exported functions, no callbacks into C++). An EM_JS body is a C string, so backslashes are eaten: a regex like `/\.pdf$/` broke the build ("Invalid regular expression flag"); use string methods there. `input.click()` must run inside the ImGui click, so the module is imported after the pick, in `onchange`.

## How to apply
The worker is terminated after each import (frees about 1 GB of WASM memory) and on Cancel; the next import loads the model again from the Cache API (cache `john-omr-<manifest version>`, older ones deleted), so only the first import downloads. Progress counts uncompressed bytes against the sizes in `omr/manifest.json`, which is why the window shows a percentage and the fixed "about 22 MB" (gzipped on Pages). To test in chrome-devtools: stub `HTMLInputElement.prototype.click` as in import-song-abc-picker-paste-drop, or call `(await import('/omr/omr.js')).read([file], FS)` directly; hook `FS.writeFile` to capture the MusicXML, and read `johnOmr.state`. Never trigger the native dialog.
