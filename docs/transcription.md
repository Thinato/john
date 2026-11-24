# Transcription: Import audio

John can listen to a recording and write a first draft of the sheet. Desktop only: there is no audio transcription on the web.

## Import audio in the viewer

**Import audio** is a button in the sidebar.

1. Drop an mp3, wav or flac file on the window, or paste its path.
2. Set the options you need (all optional):
   - start and end, in seconds;
   - tempo and meter (blank = detect);
   - the instrument the sheet is for (default from Settings);
   - the instrument playing the melody in the recording;
   - *Separate the melody (Demucs)*, for a dense mix (see [Demucs](#separate-the-melody-with-demucs));
   - *Rubato*, for music whose tempo drifts.
3. Press *Transcribe*.

John keeps working while it listens. The draft opens as a new song in *Unsorted*:

- Its notes give the detected tempo, meter and key, and whether Demucs ran. The window also shows whether Demucs ran.
- Doubtful notes get the comment "unclear in the recording, check by ear".
- In a whistle draft, the cuts, taps and rolls John hears are marked on their notes as candidates.
- Under each line of music, a strip shows the pitch John heard in the recording. It is faint where John was unsure. This pitch track is kept in `tracks/<id>.json`, next to `songs/`.

It's a machine draft. Compare it with the recording by ear, or ask the agent to tidy it.

## Ask the agent

The agent can also transcribe an audio file, with the `transcribe_audio` tool (see [MCP tools](mcp-tools.md)). It takes the same options, and a few more.

## Separate the melody with Demucs

For a singer or a lead instrument over a full band, John can first separate the melody with [Demucs](https://github.com/adefossez/demucs).

- Demucs is an optional tool you install yourself: `pip install demucs`, or set `JOHN_DEMUCS` to its path.
- It is slow, so give it short sections (use start and end).
- Use only short sections of copyrighted recordings, for personal practice.
- Without Demucs, the full mix is transcribed.

## Rubato

For slow airs, film music and other playing that speeds up and slows down, *Rubato* (`rubato` for the agent) makes the beats follow the recording's tempo.

## For developers

How the transcriber works and how to tune it: see [CLAUDE.md](../CLAUDE.md) and the `transcription-*` notes in [`kb/notes/`](../kb/notes/), for example [transcription-review-with-debug-dump.md](../kb/notes/transcription-review-with-debug-dump.md).
