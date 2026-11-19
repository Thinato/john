# OMR spike for issue #74 (throwaway, never merged)

A test page that reads sheet-music images in the browser with **homr-web** (npm `homr-web` 0.6.2, AGPL-3.0) or
**copisteria**'s JavaScript port (`src/pipeline/` of brunombo/OpticalMusicRecognition at `93ee338`, copista-2m or
copista-28m exported to ONNX), and the scripts that measured them. Results: the comment on issue #74.

Nothing here is part of John. Models, images, `vendor/` and `node_modules/` are not committed (`.gitignore`).

## Set up (everything stays in this folder and `../tmp/`)

```bash
npm install
# copisteria port + ONNX export (uv; ~0.6 GB of Python wheels, cached under ../tmp/omr/uvcache)
git clone https://github.com/brunombo/OpticalMusicRecognition vendor/OpticalMusicRecognition
git -C vendor/OpticalMusicRecognition checkout 93ee338c196d2aef0d8eccfb86c4cec8446e743f
(cd vendor/OpticalMusicRecognition && UV_CACHE_DIR=$PWD/../../../tmp/omr/uvcache uv run scripts/models/export_copista.py)
mkdir -p public/models && ln -s $PWD/vendor/OpticalMusicRecognition/public/models public/models/copista
# homr-web models (330 MB, hash-checked), laid out as <baseUrl>/<sha256>/<file>
git clone https://github.com/jymen/homr-web ../tmp/omr/src/homr-web
(cd ../tmp/omr/src/homr-web && HOMR_WEB_MODELS_RELEASE=models-homr0.7.0 ./tools/download-models.sh)
for f in ../tmp/omr/src/homr-web/models/*.onnx; do h=$(shasum -a 256 $f | cut -d' ' -f1); mkdir -p public/models/homr/$h; ln -s $PWD/$f public/models/homr/$h/; done
```

## Test pages (public domain only)

- `make-images.sh <toabc> <out>`: John's demo songs as ABC (`toabc` = `john::toAbc(john::fromJson(json))` over
  `libjohn_core.a`, six lines), beamed by `beam.py`, engraved by Verovio (`render.mjs`), rasterised by headless
  Chrome; a two-tune page, a vector PDF, and a degraded "phone photo" (ImageMagick).
- The scan: page 190 of O'Neill's *Music of Ireland* (Chicago, 1903; archive.org item `oneills-1850`), five jigs.
  Ground truth: the music21 corpus ABC of tunes 1017-1021, imported with `john-mcp import_abc`.
- `ln -s <out> public/img; ln -s <scan dir> public/scan`; `pages.json` maps each page to its ground-truth songs.

## Run

```bash
npx vite build
node serve.mjs 8740            # like GitHub Pages: no COOP/COEP (add --coop on another port to compare)
./matrix.sh none coi            # every reader x backend; results/<reader>-<backend>-<hosting>/
python3 table.py                # scores (score.py) and timings as markdown
BROWSER=firefox node run.mjs copista-2m wasm coi 8740 /img/clean-the-kesh.png   # the same in Firefox
python3 score_dir.py <dir>      # score the output of copisteria's Python pipeline (--out dir)
```

`?coi=1` loads `coi-serviceworker.js` (MIT, the fork's modified copy that leaves model downloads alone). The file
picker on the page reads one image or PDF by hand.
