---
title: The web build downloads the copista-2m model, onnxruntime-web and pdf.js at configure time, SHA-256 checked, into build-web/omr; Pages copies omr/
tags: [web, build, cmake, pages, ci, omr, dependencies, licences]
---

# Build-time downloads of the image reader

## Context
Changing the `if(EMSCRIPTEN)` image-reader block of `CMakeLists.txt` (`omr_download`, `omr_file`, `omr_npm`), bumping a pinned version, or `.github/workflows/pages.yml` / the `web` job in `ci.yml` (#75).

## Rule
No model or runtime binary is in git. `omr_download(url sha256)` fetches into `build-web/downloads/<sha256>` with `EXPECTED_HASH` (a mismatch stops configure), so a new pin downloads again and a stale file can't survive. The model is the ONNX export in `huggingface.co/pauldubois98/musicocr-models` at a pinned commit (KobiMusic publishes only PyTorch weights); onnxruntime-web and pdfjs-dist come from the npm registry tarballs, and only the files the reader loads are extracted (`ort.min.mjs` uses the JSEP `.wasm`, which serves both WebGPU and WASM). `omr/manifest.json` (sizes and a version made of the model commit and ORT version) is written at configure time; the `omr` target copies `web/omr/` on every build. `pages.yml` copies `build-web/omr` into the site; the `web` CI job checks those files exist and that `omr.js`/`worker.js` parse.

## Why
The owner chose to ship the 16 MB model with the site, fetched only on first use. Sizes users download once: model 15.9 MB (14.7 MB gzipped, ONNX barely compresses), ORT WASM 28.3 MB (6.6 MB gzipped), scripts about 0.2 MB, pdf.js about 0.6 MB gzipped only for PDFs. The npm package of onnxruntime-web has no licence text and `github.com/microsoft/onnxruntime` answered 404 in November 2026, so its MIT licence and ThirdPartyNotices (v1.30.0, from jsDelivr's GitHub mirror) are vendored in `web/omr/licenses/`; the copisteria port and KobiMusic's NOTICE files are vendored too, all indexed by `web/omr/NOTICE.txt`.

## How to apply
To bump a version: change the URL/version and the SHA-256 (`shasum -a 256` of the downloaded file), update `web/omr/NOTICE.txt`, and re-measure accuracy (the port pins copisteria `c685f28`, the same commit the ONNX export used). If the model must move to a repo the owner controls, only `OMR_MODELS` changes (same files, same hashes). Don't add npm or a bundler: the worker imports the copied files by relative path, since import maps don't apply in workers.
