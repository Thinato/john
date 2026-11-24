call c-flute '"method":"tools/call","params":{"name":"define_instrument","arguments":{"name":"C flute","low":"D4","high":"A6","key":"C","preset":"diatonic","notes":{"0":"impossible"},"techniques":["cut","roll"]}}'
call import '"method":"tools/call","params":{"name":"import_abc","arguments":{"abc":"X:1\nT:Flute Air\nM:4/4\nL:1/4\nK:C\nE F G A|","instrument":"c-flute"}}'
# deleting an instrument a song uses says so, and the song still names it
call delete '"method":"tools/call","params":{"name":"delete_instrument","arguments":{"id":"c-flute"}}'
expect '1 song(s) are written for it'
call get '"method":"tools/call","params":{"name":"get_song","arguments":{"id":"flute-air"}}'
expect 'instrument: C flute (deleted)'
call built-in '"method":"tools/call","params":{"name":"delete_instrument","arguments":{"id":"violin"}}'
expect "'violin' is not a custom instrument"
