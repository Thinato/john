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
'{"jsonrpc":"2.0","id":14,"method":"tools/call","params":{"name":"delete_bars","arguments":{"id":"smoke-reel","first":2}}}' \
'{"jsonrpc":"2.0","id":15,"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Fiddle\nM:4/4\nL:1/4\nK:G\nG,A,B,C|","instrument":"violin"}}}' \
'{"jsonrpc":"2.0","id":16,"method":"tools/call","params":{"name":"update_song","arguments":{"id":"fiddle","instrument":"kazoo"}}}' \
'{"jsonrpc":"2.0","id":17,"method":"tools/call","params":{"name":"analyze_song","arguments":{"id":"fiddle"}}}' \
'{"jsonrpc":"2.0","id":18,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","recording_instrument":"kazoo"}}}' \
'{"jsonrpc":"2.0","id":19,"method":"tools/call","params":{"name":"list_songs","arguments":{}}}' \
'{"jsonrpc":"2.0","id":20,"method":"tools/call","params":{"name":"list_songs","arguments":{"instrument":"violin"}}}' \
'{"jsonrpc":"2.0","id":21,"method":"tools/call","params":{"name":"list_songs","arguments":{"instrument":"kazoo"}}}' \
'{"jsonrpc":"2.0","id":22,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","meter":"C"}}}' \
'{"jsonrpc":"2.0","id":23,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","meter":"C|"}}}' \
'{"jsonrpc":"2.0","id":24,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","meter":"4"}}}' \
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
check 'Deleted 2 note'
check '"name":"delete_bars"'
check 'instrument: Violin'
check "unknown instrument 'kazoo'"
check 'Range on the Violin'
check '"enum":\["whistle","violin"'
check "unknown recording_instrument 'kazoo'"
check 'smoke-reel - Smoke Reel (D major, 4/4, D whistle)'
check 'fiddle - Fiddle (G major, 4/4, Violin)'
reply() { printf "%s\n" "$OUT" | grep "\"id\":$1,"; }
reply 20 | grep -q 'fiddle - Fiddle' || { echo "violin filter lost fiddle"; exit 1; }
! reply 20 | grep -q 'smoke-reel' || { echo "violin filter kept smoke-reel"; exit 1; }
reply 21 | grep -q "unknown instrument 'kazoo'" || { echo "kazoo filter not rejected"; exit 1; }
! reply 22 | grep -q 'meter must' && ! reply 23 | grep -q 'meter must' || { echo "C or C| rejected as meter"; exit 1; }
reply 24 | grep -q 'meter must be N/D' || { echo "bad meter not rejected"; exit 1; }
[ "$(printf "%s\n" "$OUT" | wc -l | tr -d ' ')" = 24 ] || { echo "expected 24 replies"; exit 1; }
echo "mcp smoke passed"
