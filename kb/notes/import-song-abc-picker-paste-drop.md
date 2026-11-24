---
title: Import song (ABC or MusicXML) goes through importSongs; the web file picker writes bytes to /picked; browser paste already works
tags: [gui, web, abc, import, emscripten, clipboard]
---

# Import song: ABC paste, file picker, drop

## Context
Changing the viewer's Import song window (`abcWindow()` and `importText()` in `src/gui_import.cpp`, #68, #73), the MCP `import_abc`/`import_musicxml` tools (`src/mcp_songs.cpp`), or how multi-tune ABC is split (`importSongs()` in `src/abc.cpp`).

## Rule
The window, `import_abc` and `import_musicxml` call `importSongs(data, like)` (`src/abc.cpp`). MusicXML (`isMusicXml()`: zip bytes or a `<score-partwise>` root) becomes one song through `importMusicXml()`; ABC text it splits the text at each `X:` line (the field lines before the first `X:` are the ABC file header and go in front of every tune), runs `importAbc()` on each, copies instrument, whistle, harp, folder and (if set) source from `like`, gives each a `uniqueId()` and saves it. A tune without a `K:` line or without notes is skipped with a warning; if none is left it throws. On the web, "Open a file..." calls the EM_JS `pickFile()`, which writes each chosen ABC/MusicXML file's bytes (a `Uint8Array`, so a zipped `.mxl` survives) to `/picked/<name>` in MEMFS; `abcWindow()` imports and deletes whatever is there each frame. Images and PDFs go to the sheet-music reader instead (see web-image-import-worker-bridge). Ctrl/Cmd+V into the paste box works in the browser with no extra code. Every import from the window (paste, files, drops, images/PDF) is for the instrument picked at its top: `importLike()` fills `like` from `App::abcInst`/`abcWhistle`/`abcHarp` (session only; empty = Settings' instrument and harp).

## Why
`importAbc()` alone reads only the first tune. Without the `K:` check any text "has notes" (the letters a-g are notes), so pasted prose became a garbage song. Writing to the virtual FS avoids exporting C functions or allocating strings from JS. The contrib.glfw3 port lets the Cmd/Ctrl+C/V/X keydowns through to the browser (no `preventDefault`), listens for the document `paste` event and stores its text as the clipboard; the paste event lands before the next frame, which is when ImGui handles the queued V key and calls `glfwGetClipboardString()`. After importing several tunes, `importText()` raises `App::newestSeen` to their file times, or `poll()` (Follow agent, always on on the web) would jump to the last tune instead of the first.

## How to apply
Desktop drag and drop has one GLFW callback (`gDropped`): `abcWindow()` runs before `importWindow()` and takes only `.abc`, `.musicxml`, `.xml` and `.mxl` files, everything else (mp3, wav, ...) stays for Import audio. Keep that order if either window changes. `input.click()` needs the click's user activation; an ImGui button runs one frame after the mouse event, which is within it. To test in chrome-devtools without the native dialog, replace `HTMLInputElement.prototype.click` with a function that saves `this`, click the button, then set `input.files` from a `DataTransfer` holding a `File` and call `input.onchange()`. To test paste, dispatch on `#canvas` a `keydown` with `metaKey` (Mac) or `ctrlKey` and `code: 'KeyV'`, then a `ClipboardEvent('paste', {clipboardData})` on `document`, then the `keyup`. Check `FS.readdir('/john/songs')` and the IndexedDB store `/john` to see that `saveSong()` persisted.
