# Web version

Try it at <https://lisecki.dev/john/>.

## What it has

The viewer runs in the browser, with no server:

- the lessons, playback and the mic;
- Import song: ABC, MusicXML, or a photo, scan or PDF of sheet music (see [Songs and Import song](songs-and-import.md));
- a dozen starter songs from `demo/`: traditional and public-domain tunes in Beginner, Intermediate and Advanced, installed on the first visit.

Songs, techniques, custom instruments and settings are kept in the browser's storage (IndexedDB). The lessons are bundled.

There is no AI agent (MCP server) and no audio-file transcription on the web. For those, use the [desktop app](../README.md#get-it).

Sound starts on the first click. The mic needs `localhost` or HTTPS.

## Build it

Needs [Emscripten](https://emscripten.org/) (`emcc` on the PATH):

```sh
emcmake cmake -S . -B build-web
cmake --build build-web       # john.html, john.js, john.wasm, john.data, and omr/ (the image reader)
python3 -m http.server -d build-web 8000
```

Then open <http://localhost:8000/john.html>.

The web build's storage and page are `web/pre.js` and `web/shell.html`.

## The image reader

The image reader (`web/omr/`) reads photos, scans and PDFs of sheet music. It runs in a Web Worker, with a port of copisteria, plus a check of the first line's key (`checkFirstLineKey()` in `src/musicxml.cpp`).

- It needs no npm or bundler. At configure time, CMake downloads, into `build-web/omr/`, at pinned versions, and checks their SHA-256:
  - the copista-2m model (its ONNX export, mirrored on this repo's `models-v1` release);
  - ONNX Runtime Web;
  - PDF.js.
- Pages sends no COOP/COEP headers, so the reader uses WebGPU where the browser has it, and single-threaded WASM otherwise.
- Its licences are listed in the README's [License](../README.md#license) section.

For developers: [web-build-emscripten.md](../kb/notes/web-build-emscripten.md), [web-omr-build-time-downloads.md](../kb/notes/web-omr-build-time-downloads.md) and the other `web-*` notes in [`kb/notes/`](../kb/notes/).
