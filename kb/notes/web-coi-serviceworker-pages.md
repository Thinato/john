---
title: coi-serviceworker makes the web build cross-origin isolated on Pages; it reloads once on the first visit, so pre.js delays IndexedDB then
tags: [web, emscripten, pages, coop, coep, service-worker, threads, indexeddb, omr]
---

# coi-serviceworker on GitHub Pages

## Context
Changing `web/coi-serviceworker.js`, `web/shell.html` or `web/pre.js`, or anything that needs `SharedArrayBuffer` / WASM threads on the web (#75: the image reader).

## Rule
GitHub Pages sends no COOP/COEP headers. `coi-serviceworker.js` (MIT; the brunombo fork's modified copy, changed again for John) registers a service worker that adds them to every same-origin response except `*.onnx`, `*.wasm` and `*.data`, and reloads the page once when it first registers. `web/pre.js` therefore waits 4 s before `FS.syncfs(true)` when the page is not isolated, is a secure context, has service workers and isn't controlled yet (a first visit); the reload normally comes first.

## Why
Without the wait, the first page opened and created the `/john` IndexedDB database just before the reload, and in Chrome the reloaded page's `indexedDB.open('/john')` then hung forever (no `blocked` event): John stayed a blank canvas until the next reload. Big files are left out because a 100 MB download streamed through a worker was aborted in Firefox (#74), and same-origin fetches need no COEP header; pages, scripts and worker scripts must keep going through it, or the workers aren't isolated and ONNX Runtime gets no threads.

## How to apply
Threads help only the WASM path (copista-2m measured 2.4 s vs 6.7 s per clean page on the main thread here; WebGPU about 1 s either way). Browsers that refuse isolation (Safari with COEP credentialless) just run single-threaded: everything works without it. To check: a fresh chrome-devtools `isolatedContext` shows `performance.getEntriesByType('navigation')[0].type == 'reload'` and `crossOriginIsolated` true after the first load. A devtools `navigate_page` reload is a hard reload and bypasses the worker (not isolated); use `location.reload()`. To test the non-isolated path, navigate with the init script `window.coi = { shouldRegister: () => false }`.
