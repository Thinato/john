#!/bin/sh
# Drives john-mcp over stdio: handshake, import, edit, analyze. Fails on any missing expected output.
set -e
BIN="$1"
export JOHN_HOME="$(mktemp -d)"
trap 'rm -rf "$JOHN_HOME"' EXIT
# separate (#5): a Demucs stub that only makes noise, and 1 s of silence (8 kHz 16-bit mono WAV) to run it on
STUB="$JOHN_HOME/demucs stub"
printf '#!/bin/sh\necho stub noise on stdout; echo stub noise on stderr >&2; exit 1\n' > "$STUB"
chmod +x "$STUB"
SILENCE="$JOHN_HOME/silence.wav"
{ printf 'RIFF\244\076\000\000WAVEfmt \020\000\000\000\001\000\001\000\100\037\000\000\200\076\000\000\002\000\020\000data\200\076\000\000'
  head -c 16000 /dev/zero; } > "$SILENCE"
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
'{"jsonrpc":"2.0","id":25,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","transpose":13}}}' \
'{"jsonrpc":"2.0","id":26,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","transpose":0}}}' \
'{"jsonrpc":"2.0","id":27,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","separate":"drums"}}}' \
"{\"jsonrpc\":\"2.0\",\"id\":28,\"method\":\"tools/call\",\"params\":{\"name\":\"transcribe_audio\",\"arguments\":{\"path\":\"$SILENCE\",\"separate\":\"vocals\"}}}" \
'{"jsonrpc":"2.0","id":29,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","rubato":"yes"}}}' \
'{"jsonrpc":"2.0","id":30,"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","rubato":true}}}' \
'{"jsonrpc":"2.0","id":31,"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Harp\nM:4/4\nL:1/4\nK:A\nA,B,CD|","instrument":"harmonica","harp":"A"}}}' \
'{"jsonrpc":"2.0","id":32,"method":"tools/call","params":{"name":"analyze_song","arguments":{"id":"harp"}}}' \
'{"jsonrpc":"2.0","id":33,"method":"tools/call","params":{"name":"update_song","arguments":{"id":"harp","harp":"H"}}}' \
'{"jsonrpc":"2.0","id":34,"method":"tools/call","params":{"name":"import_musicxml","arguments":{"xml":"<score-partwise><work><work-title>Smoke XML</work-title></work><part id=\"P1\"><measure><attributes><divisions>2</divisions><key><fifths>1</fifths></key></attributes><note><pitch><step>G</step><octave>4</octave></pitch><duration>2</duration></note><note><pitch><step>B</step><octave>4</octave></pitch><duration>2</duration><chord/></note></measure></part></score-partwise>","instrument":"violin","folder":"Beginner"}}}' \
'{"jsonrpc":"2.0","id":35,"method":"tools/call","params":{"name":"import_musicxml","arguments":{"path":"/no/such.mxl"}}}' \
'{"jsonrpc":"2.0","id":36,"method":"tools/call","params":{"name":"import_musicxml","arguments":{"xml":"X:1\nT:a\nK:D\nABc|"}}}' \
'{"jsonrpc":"2.0","id":37,"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Sung\nM:3/4\nL:1/4\nK:D\nD E F|A3|]\nw:Ev-er_ on\nW:After the tune."}}}' \
| JOHN_DEMUCS="$STUB" "$BIN")
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
check 'instrument: A harmonica'
check 'Range on the A harmonica (playable A3 .. A6)'
check "unknown harmonica key 'H'"
reply() { printf "%s\n" "$OUT" | grep "\"id\":$1,"; }
reply 20 | grep -q 'fiddle - Fiddle (G major, 4/4, Violin): fits as written' || { echo "violin playability lost fiddle"; exit 1; }
reply 20 | grep -q 'Playability on the Violin:' && reply 20 | grep -q 'smoke-reel - Smoke Reel (D major, 4/4, D whistle): fits as written' || { echo "songs are filtered by their instrument"; exit 1; }
reply 19 | grep -q 'fiddle - Fiddle (G major, 4/4, Violin): fits an octave up (transpose +12)' || { echo "no playability on the Settings whistle"; exit 1; }
reply 6 | grep -q 'Playability on the D whistle: fits as written' || { echo "analyze_song has no playability"; exit 1; }
reply 21 | grep -q "unknown instrument 'kazoo'" || { echo "kazoo filter not rejected"; exit 1; }
! reply 22 | grep -q 'meter must' && ! reply 23 | grep -q 'meter must' || { echo "C or C| rejected as meter"; exit 1; }
reply 24 | grep -q 'meter must be N/D' || { echo "bad meter not rejected"; exit 1; }
reply 2 | grep -q '0 keeps the recording' || { echo "transcribe_audio has no transpose option"; exit 1; }
reply 25 | grep -q 'transpose must be a whole number within -12..12' || { echo "transpose 13 not rejected"; exit 1; }
! reply 26 | grep -q 'transpose must' || { echo "transpose 0 rejected"; exit 1; }
reply 2 | grep -q 'only short sections of copyrighted recordings' || { echo "transcribe_audio has no separate option"; exit 1; }
reply 27 | grep -q 'separate must be' || { echo "separate drums not rejected"; exit 1; }
reply 28 >/dev/null || { echo "no reply to the separate call"; exit 1; }
reply 2 | grep -q '"rubato":{"description":"the tempo drifts' || { echo "transcribe_audio has no rubato option"; exit 1; }
reply 29 | grep -q 'rubato must be true or false' || { echo "rubato yes not rejected"; exit 1; }
! reply 30 | grep -q 'rubato must' || { echo "rubato true rejected"; exit 1; }
reply 34 | grep -q "Saved as 'smoke-xml'" && reply 34 | grep -q 'warning: chords reduced to their top note' || { echo "import_musicxml failed"; exit 1; }
reply 34 | grep -q 'instrument: Violin' && reply 34 | grep -q 'B4' || { echo "import_musicxml lost the instrument or the top note"; exit 1; }
reply 35 | grep -q 'cannot read /no/such.mxl' || { echo "missing .mxl not reported"; exit 1; }
reply 36 | grep -q 'not MusicXML' || { echo "ABC passed as MusicXML not rejected"; exit 1; }
reply 37 | grep -q 'lyric \\"Ev-\\"' && reply 37 | grep -q 'lyric _' && reply 37 | grep -q 'words (W:): After the tune.' || { echo "import_abc lost the lyrics"; exit 1; }
case $OUT in *"stub noise"*) echo "demucs output reached the MCP stdout"; exit 1 ;; esac
CR=$(printf '\r')  # replies end in a bare \n (Windows text-mode stdout would add \r)
case $OUT in *"$CR"*) echo "reply contains \\r"; exit 1 ;; esac
[ "$(printf "%s\n" "$OUT" | wc -l | tr -d ' ')" = 37 ] || { echo "expected 37 replies"; exit 1; }
echo "mcp smoke passed"
