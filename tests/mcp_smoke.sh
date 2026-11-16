#!/bin/sh
# Drives john-mcp over stdio: handshake, import, edit, analyze. Fails on any missing expected output.
set -e
BIN="$1"
export JOHN_HOME="$(mktemp -d)"
trap 'rm -rf "$JOHN_HOME"' EXIT
OUT=$(printf '%s\n' \
'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}' \
'{"jsonrpc":"2.0","method":"notifications/initialized"}' \
'{"jsonrpc":"2.0","id":2,"method":"tools/list"}' \
'{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Smoke Reel\nM:4/4\nL:1/8\nK:D\nA2FA A2dB|A4 d4|]"}}}' \
'{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"set_techniques","arguments":{"id":"smoke-reel","edits":[{"index":4,"add":["roll"],"comment":"long A"}]}}}' \
'{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"set_techniques","arguments":{"id":"smoke-reel","edits":[{"index":0,"add":["nope"]}]}}}' \
'{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"analyze_song","arguments":{"id":"smoke-reel"}}}' \
'{"jsonrpc":"2.0","id":7,"method":"server/discover"}' \
'{"jsonrpc":"2.0","id":8,"method":5}' \
'{"jsonrpc":"2.0","id":9,"method":"initialize","params":null}' \
'{"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"[1 AB|","whistle":"Dm"}}}' \
'{"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"update_song","arguments":{"id":"smoke-reel","transpose":-2000000000}}}' \
'{"jsonrpc":"2.0","id":12,"method":"tools/call","params":{"name":"get_song","arguments":{"id":5}}}' \
'{"jsonrpc":"2.0","id":13,"method":"ping"}' \
| "$BIN")
check() { printf "%s\n" "$OUT" | grep -q "$1" || { echo "missing: $1"; echo "$OUT"; exit 1; }; }
check '"protocolVersion":"2025-06-18"'
check '"name":"define_technique"'
check "Saved as 'smoke-reel'"
check 'Updated 1 note'
check "unknown technique 'nope'"
check 'best transpositions'
check '"code":-32601'
check "unknown whistle key 'Dm'"
check 'transpose must be within'
check 'bad argument type'
check '"id":13,"jsonrpc":"2.0","result":{}'
[ "$(printf "%s\n" "$OUT" | wc -l | tr -d ' ')" = 13 ] || { echo "expected 13 replies"; exit 1; }
echo "mcp smoke passed"
