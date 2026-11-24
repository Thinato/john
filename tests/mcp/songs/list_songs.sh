call reel '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Smoke Reel\nM:4/4\nL:1/8\nK:D\nA2FA A2dB|A4 d4|]"}}'
call fiddle '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Fiddle\nM:4/4\nL:1/4\nK:G\nG,A,B,C|","instrument":"violin"}}'
# playability on the Settings instrument (the D whistle)
call all '"method":"tools/call","params":{"name":"list_songs","arguments":{}}'
expect 'smoke-reel - Smoke Reel (D major, 4/4, D whistle)'
expect 'fiddle - Fiddle (G major, 4/4, Violin)'
expect 'fiddle - Fiddle (G major, 4/4, Violin): fits an octave up (transpose +12)'
# playability on another instrument; songs are not filtered by the instrument they are written for
call violin '"method":"tools/call","params":{"name":"list_songs","arguments":{"instrument":"violin"}}'
expect 'fiddle - Fiddle (G major, 4/4, Violin): fits as written'
expect 'Playability on the Violin:'
expect 'smoke-reel - Smoke Reel (D major, 4/4, D whistle): fits as written'
call kazoo '"method":"tools/call","params":{"name":"list_songs","arguments":{"instrument":"kazoo"}}'
expect "unknown instrument 'kazoo'"
