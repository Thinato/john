# separate (#5): a Demucs stub that only makes noise, and 1 s of silence (8 kHz 16-bit mono WAV) to run it on.
# Its noise must not reach the MCP stdout (the runner checks every line is JSON-RPC).
STUB="$JOHN_HOME/demucs stub"
printf '#!/bin/sh\necho stub noise on stdout; echo stub noise on stderr >&2; exit 1\n' > "$STUB"
chmod +x "$STUB"
export JOHN_DEMUCS="$STUB"
SILENCE="$JOHN_HOME/silence.wav"
{ printf 'RIFF\244\076\000\000WAVEfmt \020\000\000\000\001\000\001\000\100\037\000\000\200\076\000\000\002\000\020\000data\200\076\000\000'
  head -c 16000 /dev/zero; } > "$SILENCE"

call tools '"method":"tools/list"'
expect '0 keeps the recording'
expect 'only short sections of copyrighted recordings'
expect '"rubato":{"description":"the tempo drifts'
call bad-recording-instrument '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","recording_instrument":"kazoo"}}'
expect "unknown recording_instrument 'kazoo'"
# C and C| are meters too
call meter-c '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","meter":"C"}}'
refute 'meter must'
call meter-cut '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","meter":"C|"}}'
refute 'meter must'
call bad-meter '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","meter":"4"}}'
expect 'meter must be N/D'
call transpose-13 '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","transpose":13}}'
expect 'transpose must be a whole number within -12..12'
call transpose-0 '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","transpose":0}}'
refute 'transpose must'
call separate-drums '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","separate":"drums"}}'
expect 'separate must be'
call separate-vocals "\"method\":\"tools/call\",\"params\":{\"name\":\"transcribe_audio\",\"arguments\":{\"path\":\"$SILENCE\",\"separate\":\"vocals\"}}"
call rubato-yes '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","rubato":"yes"}}'
expect 'rubato must be true or false'
call rubato-true '"method":"tools/call","params":{"name":"transcribe_audio","arguments":{"path":"/no/such.mp3","rubato":true}}'
refute 'rubato must'
