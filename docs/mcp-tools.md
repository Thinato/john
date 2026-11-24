# MCP server and tools

`john-mcp` is an MCP server. It gives an AI agent exact tools: parse ABC or MusicXML, map notes to whistle fingerings, check range and transpositions, list candidate spots for ornaments, and save the result. The agent brings the musical judgment: where the ornaments go.

Songs the agent saves appear live in the viewer. The MCP server is part of the desktop app; there is none on the web.

How to set it up: see [Use it with an AI agent](../README.md#use-it-with-an-ai-agent) in the README.

## What to ask

- "Get Cooley's reel from thesession.org and make me a whistle sheet with ornaments."
- "Here's a photo of a sheet (attach it). Convert it to tin whistle tabs with techniques."
- "These are tabs for <tune>: D E F# G ... Where should the cuts and rolls go?"
- "Document a technique called 'bounce': ..." (adds it to the catalog and the legend)
- "I play a C flute, low D to high A." (adds a custom instrument, see [Instruments](instruments.md#custom-instruments))
- "Transcribe this recording." (see [Transcription](transcription.md))

## Tools

Songs:

- `list_songs`: saved songs by folder, and whether each fits an instrument (see [Which songs fit](instruments.md#which-songs-fit)).
- `import_abc`, `import_musicxml`: make a song from ABC notation or a MusicXML file.
- `get_song`: a song as a table of notes, with bar, beat, fingering, techniques, lyrics and comments.
- `analyze_song`: hints. Does the song fit an instrument, the best transpositions, and candidate notes for cuts, rolls, crans, slides, tonguing and breaths.
- `set_techniques`: edit techniques on notes, with a comment explaining each choice.
- `update_song`: title, source, notes, folder, tempo, instrument, whistle or harmonica key, or transpose.
- `delete_bars`: remove bars, for example to keep only the chorus.

Transcription:

- `transcribe_audio`: a draft ABC from an mp3, wav or flac file (see [Transcription](transcription.md)).

Technique catalog:

- `list_techniques`, `define_technique`: list the techniques, or add one (it shows in the viewer's legend).

Instruments:

- `list_instruments`, `define_instrument`, `delete_instrument`: see [Custom instruments](instruments.md#custom-instruments).

The server and its tools always answer in English, whatever language the viewer uses.
