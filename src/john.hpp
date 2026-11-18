// John: tin whistle tab toolkit. Shared core used by the MCP server and the GUI.
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace john {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr int WHOLE = 192;  // ticks per whole note (divisible by triplet 32nds)

enum class Kind { Note, Rest, Bar };

struct Event {
  Kind kind = Kind::Note;
  int midi = 0;          // Note only; written pitch (whistle music is written an octave low, D whistle low D = D4 = 62)
  int len = 0;           // Note/Rest: sounding length in ticks
  bool tie = false;      // Note: tied to the next note
  int tuplet = 0;        // Note/Rest inside a tuplet: p of (p:q
  int tupletQ = 0;       // q of (p:q
  bool tupletStart = false;
  std::vector<std::string> techniques;  // technique ids from the catalog
  std::string comment;   // why the techniques were chosen (shown in the GUI)
  std::string bar;       // Bar only: "|", "||", "|]", "|:", ":|", "::"
  std::string ending;    // Bar only: volta number that starts after this bar ("1", "2")
};

struct Key {
  int fifths = 2;          // -7..7, number of sharps (positive) or flats (negative)
  std::string mode = "maj";  // maj min dor mix lyd phr loc
};

struct Song {
  std::string id;
  std::string title;
  std::string source;      // where it came from (thesession URL, photo, ...)
  std::string notes;       // free text from the agent: overall advice for the tune
  std::string folder;      // song list group: one of FOLDERS or any name; empty = unsorted
  std::string meter = "4/4";
  Key key;
  int tempo = 0;           // quarter notes per minute, 0 = unknown
  std::string instrument = "whistle";  // one of INSTRUMENTS
  std::string whistle = "D";           // whistle key (used when instrument is "whistle")
  std::vector<Event> events;
};

struct Technique {
  std::string id;          // "roll"
  std::string name;        // "Long roll"
  std::string symbol;      // text drawn above the note, e.g. "~"
  std::string description;
};

struct Fingering {
  std::string holes;  // 6 chars top to bottom: X covered, O open, H half-covered
  int octave = 0;     // 0 low, 1 second (blow harder), 2 third
  bool ok = false;    // false when the note is outside the whistle's range
};

struct Spelled {
  int letter = 0;      // 0=C .. 6=B
  int alter = 0;       // -1 flat, 0 natural, +1 sharp
  int octave = 4;      // scientific octave
  int step() const { return octave * 7 + letter; }  // diatonic position, E4 = 30 is the bottom staff line
};

// ---- instruments ----
inline const std::vector<std::string> INSTRUMENTS = {"whistle", "violin", "piano", "guitar", "electric-guitar", "harmonica"};
struct Instrument {
  std::string id, name;    // "violin", "Violin"; the whistle's name includes its key ("D whistle")
  int low = 0, high = 0;   // playable range, written pitch (MIDI)
  int octave = 0;          // sounding pitch = written + 12 * octave (whistle +1, guitar -1, others 0)
  bool plays(int midi) const { return midi >= low && midi <= high; }
};
// whistleKey is only used for "whistle". Returns nullopt for an unknown id or whistle key.
std::optional<Instrument> findInstrument(const std::string& id, const std::string& whistleKey = "D");
Instrument instrumentOf(const Song& s);           // the song's instrument; a D whistle if the song's values are unknown

// ---- theory ----
std::string parseMeter(const std::string& meter);  // "N/D" (D = 2, 4, 8, 16; 1 <= N <= 32), "C" -> "4/4", "C|" -> "2/2"; "" if invalid
int meterTicks(const std::string& meter);          // ticks per bar
bool compoundMeter(const std::string& meter);      // 6/8, 9/8, 12/8
int keyAlter(const Key& k, int letter);            // key signature alteration for a letter
Spelled spell(const Key& k, int midi);
std::string noteName(const Spelled& s);            // "F#4"
std::string keyName(const Key& k);                 // "D major", "E dorian"
std::optional<Key> parseKey(const std::string& s); // "D", "Ador", "Bm", "G mixolydian"
int whistleBase(const std::string& whistle);       // MIDI of the lowest written note, D -> 62; -1 if not a whistle key
Fingering finger(const std::string& whistle, int midi);
std::string fingeringText(const Fingering& f);     // "XXX XXO +"
std::string durationName(const Event& e);          // "dotted quarter", "eighth (triplet)"
void transpose(Song& s, int semitones);
// Remove bars first..last (numbers from positions(), as get_song shows them) and the bar lines that close them.
// A closing repeat/end sign or volta mark moves onto the previous plain bar line. Returns notes/rests removed;
// throws std::invalid_argument if the range matches nothing or would empty the song.
int deleteBars(Song& s, int first, int last);

// Score layout (drawn by the GUI). Steps are Spelled::step(), C4 = 28; the treble staff's lines are steps 30..38.
// Piano songs get a grand staff: notes below middle C go on the bass staff (lines 18..26). Drawing only, the song
// stays one melody line.
bool grandStaff(const Song& s);
bool onBassStaff(const Song& s, int midi);
// Extra room, in steps (half staff spaces), the notes of events [from, to) need above the top staff and below the
// bottom staff. 6 steps above the top line and `free` steps below the bottom line fit the normal spacing.
struct Room { int above = 0, below = 0; };
Room systemRoom(const Song& s, int from, int to, int free);

// Displayed accidental for each note (0 = none, else -1/0/+1 via `shown`), recomputed per bar.
struct Accidental { bool shown = false; int alter = 0; };
std::vector<Accidental> accidentals(const Song& s);

// Bar number and tick-in-bar for every event, by elapsed time. A short first bar is a pickup (bar 0), two short
// bars around a repeat sign count as one, and 2nd/3rd endings restart at the time the 1st ending started.
struct Pos { int bar = 0, tick = 0; };
std::vector<Pos> positions(const Song& s);

// ---- sound: performance, synth, pitch detection ----
std::vector<int> playOrder(const Song& s);   // note/rest event indices in performance order, repeats and endings unrolled

struct Voice {                 // one note as played by the backing voice
  int ev = -1;                 // event index
  int ord = -1;                // index into playOrder()
  int midi = 0;                // written pitch (sounds an octave higher)
  double start = 0, dur = 0;   // seconds
  double gap = 0;              // silence at the end of the note (breath, staccato, next note tongued/repeated)
  bool tongued = false, vibrato = false;
  std::vector<std::pair<double, int>> graces;  // (seconds after start, semitones from the note) for cuts/taps/rolls
};
struct Performance {
  std::vector<Voice> voices;                    // notes only (rests are gaps)
  std::vector<std::pair<double, bool>> clicks;  // metronome: time, bar accent
  double length = 0;                            // seconds
  double countIn = 0;                           // seconds of clicks before the first note
  int octave = 1;                               // sounding = written + 12 * octave (the song's instrument)
  std::string sound = "whistle";                // instrument id whose timbre plays the notes
};
// fromOrder: index into playOrder() to start at. bpm = quarter notes per minute.
// sound: an INSTRUMENTS id to play with, "" = the song's instrument. It changes the timbre and which
// ornaments/breaths are played, never the pitch (the octave always comes from the song's instrument).
Performance perform(const Song& s, double bpm, int fromOrder = 0, bool countIn = true, const std::string& sound = "");
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

// ---- catalog & storage ----
std::vector<Technique> defaultTechniques();
fs::path dataDir();                         // $JOHN_HOME or ~/Documents/John
std::vector<Technique> loadTechniques();    // defaults merged with dataDir()/techniques.json
void saveCustomTechnique(const Technique& t);
// User preferences, saved in dataDir()/settings.json and read by both the GUI and the MCP server.
// A new setting: one field here, its name in the JSON list in song.cpp, one line in the GUI's Settings window.
struct Settings {
  bool hints = true;                // instrument hints (whistle fingering charts and fingerings)
  float zoom = 9;                   // staff space in px
  bool light = false;               // light theme instead of dark
  int tempo = 100;                  // playback tempo for songs without one
  bool countIn = true;              // one bar of clicks before Play
  bool metronome = true;
  float volume = 0.6f;
  std::string sound;                // playback sound, an INSTRUMENTS id; "" = the song's instrument
  float gate = 0.01f;               // mic level below which nothing is heard
  float latencyMs = 80;             // mic + speaker delay, for grading play-along
  float tolerance = 40;             // cents from the note that still count as right
  std::string instrument = "whistle";  // for new songs (import_abc without an instrument)
  std::string songFilter;           // song list shows only this instrument id; "" = all
  bool operator==(const Settings&) const = default;
};
Settings loadSettings();                    // missing keys -> defaults; unreadable file -> all defaults
void saveSettings(const Settings& s);
std::vector<std::string> listSongs();       // ids
std::optional<Song> loadSong(const std::string& id);
void saveSong(const Song& s);
std::string slug(const std::string& title);
inline const std::vector<std::string> FOLDERS = {"Lessons", "Beginner", "Intermediate", "Advanced"};
bool folderBefore(const std::string& a, const std::string& b);  // FOLDERS order, then custom A-Z, unsorted last
fs::path lessonsDir();                      // lessons/ next to the executable (release package), else the repo's
fs::path bundled(const fs::path& rel, const fs::path& built);  // rel next to the executable if it exists, else built
int installSongs(const fs::path& from);     // copy songs from a bundled folder (lessons/, demo/) not installed before; returns count
json toJson(const Song& s);
Song fromJson(const json& j);

// ---- import / analysis ----
struct ImportResult { Song song; std::vector<std::string> warnings; };
ImportResult importAbc(const std::string& abc);
std::string listing(const Song& s);          // compact text table of every event, for the agent
std::string analyze(const Song& s);          // deterministic technique candidates + range report

// ---- audio transcription (transcribe.cpp) ----
std::vector<float> decodeAudio(const std::string& path, int sampleRate);  // mono; mp3/wav/flac; throws on failure
struct TranscribeOptions {
  double bpm = 0;             // quarter notes per minute; 0 = detect
  std::string meter;          // "" = detect (4/4, 3/4, 6/8 or 9/8)
  std::string title = "Transcription";
  std::string instrument = "whistle";  // the song is written for this instrument (D whistle) and fitted to its range
  std::string recordingInstrument;     // plays the melody in the recording (D whistle); limits the pitch search to its
                                       // sounding range. "" = search C3..E7
};
struct Transcription { Song song; std::string report; };
// Melody of a recording as a song for o.instrument (octave fitted to its range). Throws if no melody is found.
Transcription transcribe(const std::vector<float>& audio, int sampleRate, const TranscribeOptions& o = {});
std::string toAbc(const Song& s);            // ABC with L:1/16 (no tuplets), readable by importAbc

}  // namespace john
