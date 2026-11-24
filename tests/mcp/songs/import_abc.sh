call reel '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Smoke Reel\nM:4/4\nL:1/8\nK:D\nA2FA A2dB|A4 d4|]"}}'
expect "Saved as 'smoke-reel'"
call bad-whistle '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"[1 AB|","whistle":"Dm"}}'
expect "unknown whistle key 'Dm'"
call lyrics '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Sung\nM:3/4\nL:1/4\nK:D\nD E F|A3|]\nw:Ev-er_ on\nW:After the tune."}}'
expect 'lyric \\"Ev-\\"'
expect 'lyric _'
expect 'words (W:): After the tune.'
# a lowercase whistle key still works
call c-whistle '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Low C\nM:4/4\nL:1/4\nK:C\nC D E F|","whistle":"c"}}'
expect 'instrument: C whistle'
expect 'C4    quarter                  XXX XXX'
