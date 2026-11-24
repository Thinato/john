# A song's instrument (violin, harmonica) through import_abc, update_song and analyze_song
call fiddle '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Fiddle\nM:4/4\nL:1/4\nK:G\nG,A,B,C|","instrument":"violin"}}'
expect 'instrument: Violin'
call kazoo '"method":"tools/call","params":{"name":"update_song","arguments":{"id":"fiddle","instrument":"kazoo"}}'
expect "unknown instrument 'kazoo'"
call analyze-fiddle '"method":"tools/call","params":{"name":"analyze_song","arguments":{"id":"fiddle"}}'
expect 'Range on the Violin'
call harp '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Harp\nM:4/4\nL:1/4\nK:A\nA,B,CD|","instrument":"harmonica","harp":"A"}}'
expect 'instrument: A harmonica'
call analyze-harp '"method":"tools/call","params":{"name":"analyze_song","arguments":{"id":"harp"}}'
expect 'Range on the A harmonica (playable A3 .. A6)'
call bad-harp '"method":"tools/call","params":{"name":"update_song","arguments":{"id":"harp","harp":"H"}}'
expect "unknown harmonica key 'H'"
