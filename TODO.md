# TODO

## Transcribe songs from MP3/audio files

Goal: turn a recording into a first-draft ABC that the agent refines and imports, the same way photos are handled now.

Plan (all logic in `john_core`, tested offline like the pitch tests):

1. **Decode**: `ma_decoder` from miniaudio (already a dependency) reads mp3/wav/flac. Mix down to mono and resample to 48 kHz.
2. **Pitch track**: run the existing `detectPitch` (YIN) and `rms` on frames of 2048 samples with a hop of about 10 ms, giving (time, Hz, level) per frame.
3. **Notes**: median-filter the track and round to semitones. Split notes on onsets (a rise in RMS, or a stable pitch change), drop blips under about 50 ms, merge repeats that have no onset between them.
4. **Tempo and grid**: estimate the beat from autocorrelation of the onset envelope, or take the tempo from the user. Quantize to a sixteenth or triplet grid and pick the meter (4/4, 3/4, 6/8) that best fits the accents.
5. **Key**: build a pitch-class histogram and compare it with major and minor key profiles (Krumhansl). Spell notes with the existing `spell()`.
6. **Whistle fit**: fold the result into the D whistle range with `analyze()`'s transposition ranking.
7. **Output**: an ABC string goes through `importAbc()`. Add an MCP tool `transcribe_audio(path, tempo?, meter?, start?, end?)` that returns the ABC draft and per-note confidence, so the agent can fix rhythm and spelling before importing.
8. **Test**: render a known song with `Synth`, transcribe it, and check that the notes match (round trip, no audio device).
9. **GUI (later)**: an "Import audio" button, and showing the pitch track under the staff.

Limits and options:
- This only works on monophonic audio: solo whistle, flute or a cappella voice. A full band mix needs source separation first. One option is Demucs (Python, external): isolate the vocals or lead, then transcribe the stem. Another is a polyphonic model such as Spotify's Basic Pitch (ONNX). Either would be an optional external step, not a build dependency.
- Only transcribe short sections of copyrighted recordings for personal practice.
