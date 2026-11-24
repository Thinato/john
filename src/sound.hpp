// Performance, synth and pitch detection (sound.cpp).
#pragma once
#include "catalog.hpp"

namespace john {

// ---- sound: performance, synth, pitch detection ----
std::vector<int> playOrder(const Song& s);   // note/rest event indices in performance order, repeats and endings unrolled

struct Voice {                 // one note as played by the backing voice
  int ev = -1;                 // event index
  int ord = -1;                // index into playOrder()
  int midi = 0;                // written pitch (sounds at written + 12 * Performance::octave)
  double start = 0, dur = 0;   // seconds
  double gap = 0;              // silence at the end of the note (breath, staccato, next note tongued/repeated)
  bool tongued = false, vibrato = false;
  std::vector<std::pair<double, int>> graces;  // (seconds after start, semitones from the note) for cuts/taps/rolls
  double graceLen = 0.035;     // seconds each grace sounds (shorter on short notes with grace notes, turns or shakes)
};
struct Performance {
  std::vector<Voice> voices;                    // notes only (rests are gaps)
  std::vector<std::pair<double, bool>> clicks;  // metronome: time, bar accent
  double length = 0;                            // seconds
  double countIn = 0;                           // seconds of clicks before the first note
  int octave = 1;                               // sounding = written + 12 * octave (the player's instrument)
  std::string sound = "whistle";                // instrument id whose timbre plays the notes
};
// fromOrder: index into playOrder() to start at. bpm = quarter notes per minute.
// player: the instrument the tune is played on (the GUI passes Settings' instrument), default the song's own. The
// octave always comes from it: a violin-written tune played on a whistle sounds an octave above the written notes.
// sound: a SOUNDS id to play with, "" = the player's instrument's sound. It changes the timbre and which ornaments/breaths are
// played, never the pitch. The marks played are the player's (marks()): hidden ones are not played, equivalents are.
Performance perform(const Song& s, double bpm, int fromOrder = 0, bool countIn = true, const std::string& sound = "",
                    const std::optional<Instrument>& player = std::nullopt);
double soundingFreq(int writtenMidi, int octave);   // octave: Instrument::octave (whistle music sounds an octave up)
double writtenMidiFromFreq(double hz, int octave);  // fractional written MIDI

struct Synth {                // simple voice per instrument (Performance::sound) + metronome; render() runs on the audio thread
  int sampleRate = 48000;
  bool melody = true, metronome = true;
  float volume = 0.6f;
  void render(const Performance& p, long long& pos, float* out, int frames);
 private:
  double phase = 0, amp = 0, noise = 0;
  size_t vi = 0, ci = 0;
  unsigned rng = 12345;
};

float rms(const float* x, int n);
// YIN pitch detector. Returns Hz, or 0 when no clear pitch in [fmin, fmax].
float detectPitch(const float* x, int n, int sampleRate, float fmin = 400, float fmax = 3000);

// Mic play-along: how the heard pitch (written MIDI, fractional, as writtenMidiFromFreq gives it; 0 = nothing) compares
// with the written note due. cents: from the note; for a voice (Instrument::voice) folded to the nearest octave of it,
// since singers pick their own octave (#87). Right: within tolerance cents. Octave: an octave off (never for a voice).
enum class MicNote { Wrong, Right, Octave };
float heardCents(float heard, int want, const Instrument& in);
MicNote judgeNote(float heard, int want, float toleranceCents, const Instrument& in);
// Two written notes the mic can't tell apart on `in`: the same note, or for a voice the same pitch class. Wait for me
// asks for a new attack between them (a repeated note, a singer's octave leap).
bool sameHeard(int a, int b, const Instrument& in);
// The mic's pitch search for play-along on `in` (sounding Hz) and the samples per analysis (YIN needs two periods of
// fmin). The instrument's sounding range plus a margin; a voice an octave further each way, since singers pick their
// own octave (#87), with a longer window for the low end.
struct MicSearch { float fmin = 0, fmax = 0; int window = 2048; };
MicSearch micSearch(const Instrument& in, int sampleRate);
// Range test (#87, Find my range): the pitch search for the takes (B1..F#6) and the note of one take, the pitches
// detected frame by frame (0 = none): the median of those in the search range, as a sounding MIDI note. nullopt when
// under a third of the frames had one (nothing clear was sung).
constexpr float VOICE_MIN_HZ = 60, VOICE_MAX_HZ = 1500;
std::optional<int> sungNote(const std::vector<float>& hz);
// "My voice" (id my-voice, custom): the sung lowest and highest notes (sounding MIDI, either order) written in the
// preset's octave, with the preset's octave, techniques, instead and sound. nullopt when they are less than a fifth apart.
std::optional<Instrument> myVoice(const Instrument& preset, int low, int high);

}  // namespace john
