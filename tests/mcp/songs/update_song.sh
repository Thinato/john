call reel '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Smoke Reel\nM:4/4\nL:1/8\nK:D\nA2FA A2dB|A4 d4|]"}}'
call huge-transpose '"method":"tools/call","params":{"name":"update_song","arguments":{"id":"smoke-reel","transpose":-2000000000}}'
expect 'transpose must be within'
