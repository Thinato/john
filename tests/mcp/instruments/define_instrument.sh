call c-flute '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"C flute","low":"D4","high":"A6","key":"C","preset":"diatonic","notes":{"0":"impossible"},"techniques":["cut","roll"]}}'
expect "Instrument 'c-flute' created"
expect 'impossible: D4'
expect 'hard: D#4, F#4'
call bad-low '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"Bad","low":"H4","high":"A6"}}'
expect 'low must be a note'
call bad-technique '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"Odd","low":60,"high":72,"techniques":["nope"]}}'
expect "unknown technique 'nope'"
# a custom instrument works like a built-in one
call import '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Flute Air\nM:4/4\nL:1/4\nK:C\nE F G A|","instrument":"c-flute"}}'
expect 'instrument: C flute'
call list-songs '"method":"tools/call","params":{"name":"list_songs","arguments":{"instrument":"c-flute"}}'
expect 'Playability on the C flute'
expect 'flute-air - Flute Air (C major, 4/4, C flute): fits as written'
call hints '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"Charts","low":60,"high":72,"hints":true}}'
expect "unknown field 'hints'"
# instead: equivalents must map to the instrument's own techniques
call instead-unlisted '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"Keys","low":60,"high":72,"techniques":["grace"],"instead":{"cut":"turn"}}}'
expect 'instead \\"cut\\": \\"turn\\" must be one of the instrument'
call instead-unknown '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"Keys","low":60,"high":72,"techniques":["grace"],"instead":{"cut":"grace","nope":"grace"}}}'
expect "unknown technique 'nope'"
call instead '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"Keys","low":60,"high":72,"techniques":["grace"],"instead":{"cut":"grace","tap":"grace"}}}'
expect "Instrument 'keys' created"
expect 'techniques: grace; instead: cut -> grace, tap -> grace'
call voice '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"Choir tenor","low":"D4","high":"G5","octave":-1,"voice":true,"techniques":["breath","slide"],"sound":"violin"}}'
expect "Instrument 'choir-tenor' created"
expect 'singing voice'
