call reel '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Smoke Reel\nM:4/4\nL:1/8\nK:D\nA2FA A2dB|A4 d4|]"}}'
call whistle '"method":"tools/call","params":{"name":"analyze_song","arguments":{"id":"smoke-reel"}}'
expect 'best transpositions'
expect 'Playability on the D whistle: fits as written'
# on the whistle: whistle technique candidates, no equivalents
refute 'way to play'
expect 'tongue (phrase start'
# for another instrument: its playability, and its equivalents of the whistle techniques
call piano '"method":"tools/call","params":{"name":"analyze_song","arguments":{"id":"smoke-reel","instrument":"piano"}}'
expect 'Playability on the Piano'
expect "turn (the Piano's way to play: short roll"
expect "grace (the Piano's way to play: cut"
refute 'tongue (phrase start'
