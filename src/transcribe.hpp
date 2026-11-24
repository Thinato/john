// Audio transcription (transcribe.cpp) and audio files to songs (import_audio.cpp).
#pragma once
#include "song.hpp"

namespace john {

// ---- audio transcription (transcribe.cpp) ----
// mp3/wav/flac, interleaved when channels > 1 (mono by default); throws on failure
std::vector<float> decodeAudio(const std::string& path, int sampleRate, int channels = 1);
void writeWav(const std::string& path, const std::vector<float>& x, int sampleRate, int channels = 1);  // 16-bit PCM
struct TranscribeOptions {
  double bpm = 0;             // quarter notes per minute; 0 = detect
  std::string meter;          // "" = detect (4/4, 3/4, 6/8 or 9/8)
  std::string title = "Transcription";
  std::string instrument = "whistle";  // the song is written for this instrument (D whistle) and fitted to its range
  std::string recordingInstrument;     // plays the melody in the recording (D whistle); limits the pitch search to its
                                       // sounding range. "" = search C3..E7
  // semitones to transpose the melody by before the octave fit (K: follows). Unset = automatic: when the best octave
  // still folds more than 10% of the note time, the shift in -5..+6 with the fewest notes out of range and hard
  // fingerings (see transcribe()); otherwise none.
  std::optional<int> transpose;
  // "vocals" or "other": transcribeFile() first runs Demucs (JOHN_DEMUCS, else demucs on PATH) on the section and
  // transcribes that stem; if it can't, the full mix. The report's "separation:" line says which. "" = no separation.
  std::string separate;
  // the tempo drifts (slow airs, film music, free playing): beats follow a local tempo instead of a steady one, and
  // the report gives the tempo range (song tempo = the median). Off: as before #11.
  bool rubato = false;
};
struct TrackPoint { float time = 0; int midi = 0; float conf = 0; };  // seconds, sounding MIDI (0 = no melody), salience
// The melody line frame by frame (what JOHN_TRANSCRIBE_DEBUG dumps) and, per detected beat, (song tick, seconds): the
// song's ticks count from its first event, in event order, so linear interpolation maps the recording onto the notes.
struct PitchTrack { std::vector<TrackPoint> points; std::vector<std::pair<int, double>> sync; };
struct Transcription { Song song; std::string report; PitchTrack track; };
// Melody of a recording as a song for o.instrument (octave fitted to its range). Throws if no melody is found.
Transcription transcribe(const std::vector<float>& audio, int sampleRate, const TranscribeOptions& o = {});
std::string toAbc(const Song& s);            // ABC with L:1/16 (tuplets as (3), readable by importAbc
double trackTick(const PitchTrack& t, double seconds);  // song tick at a time of the recording (extrapolated at the ends)
// Decode `path` (~/ allowed), keep start..end seconds (end <= 0: to the end) and transcribe it: transcribe_audio and
// the viewer's Import audio. o.meter must be "" or normalized by parseMeter. Throws on unreadable audio or no melody.
Transcription transcribeFile(std::string path, double start, double end, TranscribeOptions o);
// Import audio (desktop viewer): transcribeFile(), then save the draft as a new song (Unsorted, source = the file
// name, notes = the report's separation/tempo/meter/key/transposed lines, notes the recording is unclear about commented) and its pitch
// track as dataDir()/tracks/<id>.json. Returns the saved song. Safe on a worker thread while the GUI keeps drawing,
// as long as the language isn't changed meanwhile (tr() reads it).
Song importAudio(const std::string& path, double start, double end, const TranscribeOptions& o);
std::optional<PitchTrack> loadTrack(const std::string& id);  // dataDir()/tracks/<id>.json, if the song has one

}  // namespace john
