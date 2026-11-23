---
title: coi-serviceworker (COOP/COEP on Pages for WASM threads) was tried for the image reader and dropped; don't re-add it without solving its costs
tags: [web, emscripten, pages, coop, coep, service-worker, threads, indexeddb, omr, decision]
---

# coi-serviceworker: tried and dropped

## Context
Wanting `SharedArrayBuffer` or WASM threads on the web build, e.g. to speed up the image reader (`web/omr/worker.js`, #75), on GitHub Pages, which can't send COOP/COEP headers.

## Rule
The web build is not cross-origin isolated, by the owner's decision (PR #79). The reader runs the detector on WebGPU where the browser has it and everything else on single-threaded WASM; ONNX Runtime Web checks `crossOriginIsolated` itself and uses one thread, with no setting or console error needed. Don't add coi-serviceworker (or any service worker that rewrites headers) back without answering the costs below.

## Why
It was built and measured in #75, then dropped:
- It reloads the page once on every first visit (the service worker must control the page before it is isolated), and again after a hard reload.
- That reload broke the app: IndexedDB opened just before it stayed locked for the reloaded page in Chrome (`indexedDB.open('/john')` never answered, not even `blocked`), so John stayed a blank canvas. The workaround was a fixed 4 s wait before loading IndexedDB on first visits, a timing guess.
- Safari was never tested (the stock script asks it for COEP credentialless, which it may not support), and devtools reloads bypass the worker, which made testing confusing.
- The gain is on the WASM path only: copista-2m took 2.4 s with 4 threads against 6.7 s with one per clean page (main-thread measurement), while WebGPU, the usual path in Chrome, takes about 1 s either way.

## How to apply
If threads are ever needed, prefer a host that can send the headers itself, or test the first visit, IndexedDB, Safari and Firefox before shipping. Measure a page on the WASM path with `navigator.gpu` hidden (a worker started with a flag that deletes it) and compare against the WebGPU time before deciding it is worth it.
