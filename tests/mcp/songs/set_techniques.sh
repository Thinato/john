call reel '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Smoke Reel\nM:4/4\nL:1/8\nK:D\nA2FA A2dB|A4 d4|]"}}'
call roll '"method":"tools/call","params":{"name":"set_techniques","arguments":{"id":"smoke-reel","edits":[{"index":4,"add":["roll"],"comment":"long A"}]}}'
expect 'Updated 1 note'
call unknown '"method":"tools/call","params":{"name":"set_techniques","arguments":{"id":"smoke-reel","edits":[{"index":0,"add":["nope"]}]}}'
expect "unknown technique 'nope'"
