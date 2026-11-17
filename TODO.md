# TODO

## Transcribe songs from MP3/audio files

Status: steps 1-8 are done (`transcribe_audio` MCP tool, `src/transcribe.cpp`). Still open:
- Tune it by ear on more real songs in `tmp/`.
- Detect the meter (it's 4/4 unless given).
- Triplets and swing: the grid is straight sixteenths.
- Split notes at beat boundaries so syncopation reads more easily.
- The GUI import button (step 9).
- Optional source separation for dense mixes.
- Learned from fuze (solo piano intro):
  - Piano and guitar notes fade before the next note, so the draft fills with rests. Sustain through short unvoiced gaps.
  - The continuity filter dropped a real note at the start of a wide leap (a low D before a jump up a tenth). Don't drop notes at phrase starts, or ones that recur.
  - The detected tempo was wrong without a hint (125 instead of 70). Report the onset regularity so the agent can spot it.
  - Only octave shifts are tried for the range fit. Suggest a non-octave transposition when no octave fits (D minor needed +2 to E minor).
- Learned from Concerning Hobbits (a whistle solo over an orchestra):
  - The tracker kept jumping down to the orchestra's low notes (A3, F#3) under the whistle. Add an `instrument` option (e.g. "whistle") that limits the pitch search to that instrument's sounding range (D5-B6 for a D whistle), so accompaniment below it can't win.
  - Rubato (the film stretches notes freely) defeats the fixed beat grid. Let the agent pass a section's tempo, or quantize phrase by phrase.
  - Ornaments (cuts, turns, shakes) show up as tiny notes. Report them as candidate techniques instead of melody notes.

Original plan:

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
