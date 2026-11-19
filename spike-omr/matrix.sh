#!/usr/bin/env bash
# Every reader x backend x hosting, one after another (memory is measured per run). Needs serve.mjs on $PORT
# (no headers) and, for "headers", on $PORT2 with --coop.   ./matrix.sh [hosting ...]
cd "$(dirname "$0")"
PORT=${PORT:-8740}
imgs=$(python3 -c "import json; print(' '.join(json.load(open('pages.json'))))")
for hosting in "${@:-none}"; do
  for reader in copista-2m copista-28m homr; do
    for prefer in webgpu wasm; do
      [ "$hosting" != none ] && [ "$prefer" = webgpu ] && continue  # isolation only matters for WASM threads
      port=$PORT; [ "$hosting" = headers ] && port=${PORT2:-8741}
      p=$prefer; [ "$reader" = homr ] && [ "$hosting" != none ] && p=wasm-threads  # homr caps at plain wasm unless asked
      node run.mjs $reader $p $hosting $port $imgs 2>&1 | grep -E '^\{|Error'
    done
  done
done
