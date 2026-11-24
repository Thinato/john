call tools '"method":"tools/list"'
expect 'an instrument id (list_instruments)'
call list '"method":"tools/call","params":{"name":"list_instruments","arguments":{}}'
# the built-ins
expect 'whistle - D whistle (built-in): written D4..C#6'
expect 'D#4 overblow x3'
# the piano's equivalents of the whistle techniques
expect 'techniques: staccato, accent, trill, grace, turn; instead: cran -> turn, cut -> grace, double_cut -> grace, roll -> turn'
# the voices
expect 'voice-tenor - Tenor voice (built-in): written C4..A5, sounds 12 semitones lower'
expect 'voice-bass - Bass voice'
expect 'techniques: breath, vibrato, slide; sound: violin; singing voice'
