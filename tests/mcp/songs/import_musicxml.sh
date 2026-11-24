call xml '"method":"tools/call","params":{"name":"import_musicxml","arguments":{"xml":"<score-partwise><work><work-title>Smoke XML</work-title></work><part id=\"P1\"><measure><attributes><divisions>2</divisions><key><fifths>1</fifths></key></attributes><note><pitch><step>G</step><octave>4</octave></pitch><duration>2</duration></note><note><pitch><step>B</step><octave>4</octave></pitch><duration>2</duration><chord/></note></measure></part></score-partwise>","instrument":"violin","folder":"Beginner"}}'
expect "Saved as 'smoke-xml'"
expect 'warning: chords reduced to their top note'
# keeps the instrument and the chord's top note
expect 'instrument: Violin'
expect 'B4'
call missing '"method":"tools/call","params":{"name":"import_musicxml","arguments":{"path":"/no/such.mxl"}}'
expect 'cannot read /no/such.mxl'
call abc '"method":"tools/call","params":{"name":"import_musicxml","arguments":{"xml":"X:1\nT:a\nK:D\nABc|"}}'
expect 'not MusicXML'
