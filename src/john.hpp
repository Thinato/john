// John: tin whistle tab toolkit. Shared core used by the MCP server and the GUI.
#pragma once
#include <filesystem>
#include <map>
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
  // Note only, first verse of the lyrics (ABC w:, MusicXML <lyric>): the syllable sung on this note, whether the word
  // goes on (a hyphen before the next syllable), or, with no syllable, whether the previous syllable is held over it.
  std::string lyric;
  bool lyricHyphen = false;
  bool lyricHeld = false;
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
  std::string instrument = "whistle";  // an instrument id (instrumentIds())
  std::string whistle = "D";           // whistle key (used when instrument is "whistle")
  int transposed = 0;                  // semitones transpose() moved it since import; 0 = the original sheet
  std::string harp = "C";              // diatonic harmonica key (used when instrument is "harmonica"), one of HARP_KEYS
  std::string instrumentName;          // a custom instrument's name ("instrument_name"), so a deleted one is still named
  std::vector<Event> events;
  std::string words;       // text printed after the tune (ABC W: lines), one line per W:
  // translations of the title, notes and comments: language -> English text -> translation ("pt-BR" -> ...)
  std::map<std::string, std::map<std::string, std::string>> i18n;
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

// ---- instruments (#85): one JSON profile per instrument ----
// Built-ins ship in instruments/<id>.json (instrumentsDir()); custom ones live in dataDir()/instruments/<id>.json and
// override a built-in with the same id. The file name is the id. See kb/notes/instrument-profile-files.md.
inline const std::vector<std::string> SOUNDS = {"whistle", "violin", "piano", "guitar", "electric-guitar", "harmonica"};  // synth voices
inline const std::vector<std::string> NOTE_HOWS = {"half-hole", "cross-fingering", "bend", "overblow"};
struct NoteLevel {  // how hard one note is on an instrument
  enum Level { Easy, Hard, Impossible } level = Easy;
  std::string how;    // hard notes: one of NOTE_HOWS, or ""
  double weight = 1;  // hard notes: 1 = a whistle half-hole (the whistle's cross-fingered C is 0.2)
  bool operator==(const NoteLevel&) const = default;
};
struct Instrument {
  std::string id, name;    // "violin", "Violin"; a keyed instrument's name includes its key ("D whistle", "A harmonica")
  int low = 0, high = 0;   // playable range, written pitch (MIDI)
  int octave = 0;          // sounding pitch = written + 12 * octave (whistle +1, guitar -1, others 0)
  std::string key;         // the key low..high are for ("D", "Bb"); "" = no key
  std::map<int, NoteLevel> notes;       // semitones above low -> level; missing in range = easy
  std::vector<std::string> techniques;  // technique ids it can play: the sheet shows only these (marks(), #86)
  std::map<std::string, std::string> instead;  // a technique it can't play -> the listed one it plays instead (cut -> grace)
  std::string sound = "whistle";        // one of SOUNDS
  bool hints = false;                   // John has fingering charts for it (tin whistle charts, on `key`)
  bool voice = false;                   // a singing voice (#87): the mic counts a note sung an octave off as right
  std::vector<std::string> keys;        // the keys it comes in, lowest instrument first; empty = only `key`
  std::string keyedName;                // name in another key, "%s whistle"
  int order = 1000;                     // built-ins: place in the lists
  bool builtin = false, custom = false; // the id ships with John / the profile comes from the data folder
  NoteLevel level(int midi) const { auto it = notes.find(midi - low); return it == notes.end() ? NoteLevel{} : it->second; }
  bool plays(int midi) const { return midi >= low && midi <= high && level(midi).level != NoteLevel::Impossible; }
  bool operator==(const Instrument&) const = default;
};
// Diatonic harmonica keys, lowest harp first: G..B harps sit below C (an A harp starts on A3), Db..F# above it.
inline const std::vector<std::string> HARP_KEYS = {"G", "Ab", "A", "Bb", "B", "C", "Db", "D", "Eb", "E", "F", "F#"};
// whistleKey is only used for "whistle", harpKey for "harmonica" (the range moves to that key, see Instrument::keys).
// Returns nullopt for an unknown id or key.
std::optional<Instrument> findInstrument(const std::string& id, const std::string& whistleKey = "D",
                                         const std::string& harpKey = "C");
Instrument instrumentOf(const Song& s);           // the song's instrument; a D whistle if the song's values are unknown
std::string instrumentLabel(const Song& s);       // its name, or "<instrument_name> (deleted)" when it no longer exists
fs::path instrumentsDir();                        // instruments/ next to the executable, else the repo's
std::vector<Instrument> instruments();            // the catalog: built-ins by order (custom overrides in place), then custom by name
std::vector<std::string> instrumentIds();         // the catalog's ids, same order
std::vector<std::string> instrumentWarnings();    // files the last load skipped, and why
void reloadInstruments();                         // read the files again (another process may have changed them)
int parseNote(const std::string& s);              // "D4" -> 62, "C#6" -> 85 (C4 = 60); -1 if not a note
// A profile from JSON (the file format, plus "id"). Throws std::runtime_error naming the bad field.
Instrument parseInstrument(const json& j);
json toJson(const Instrument& in);                // the file format, without the id (the file name is the id)
void saveInstrument(const Instrument& in);        // dataDir()/instruments/<id>.json, then reloads the catalog; throws for
                                                  // an invalid profile or a technique not in loadTechniques()
bool deleteInstrument(const std::string& id);     // removes the custom file; false if there was none
void applyPreset(Instrument& in, bool diatonic);  // chromatic: all easy; diatonic: notes outside the major scale of key
                                                  // (else of the bottom note) hard
// What the sheet shows of a note's technique marks on `in` (#86). A mark `in` lists stays; one in `in.instead` becomes
// its equivalent; breath always stays (a phrase mark; "phrase end (lift)" on instruments that don't list it); a
// technique that isn't built-in (define_technique) stays, since built-in profiles can't list it. Everything else is
// hidden. Display and playback only: the song keeps its marks. shown has no duplicates; hidden lists the marks dropped.
struct Marks { std::vector<std::string> shown, hidden; };
Marks marks(const std::vector<std::string>& techniques, const Instrument& in);
int hiddenMarks(const Song& s, const Instrument& in);  // marks hidden in the whole song

// ---- viewer language (gettext style: the English text is the key) ----
// Only the GUI calls setLanguage()/setSolfege(); until then tr() returns its argument and note names are letters,
// so the MCP server, which never calls them, stays English.
fs::path i18nDir();                                 // i18n/ next to the executable, else the repo's: one <code>.json per language
bool setLanguage(const std::string& code);          // "en" or a file in i18nDir(), e.g. "pt-BR"; false = English
std::string systemLanguage();                       // LC_ALL/LANG (navigator.language on the web; on macOS, if both are empty,
                                                    // the preferred system language) matched to a file; "en" if none
std::string matchLanguage(std::string want);        // "pt_BR.UTF-8", "pt-PT" -> "pt-BR" (a file in i18nDir()); "en" if none
std::vector<std::pair<std::string, std::string>> languages();  // (code, own name) of every file in i18nDir()
const char* tr(const char* english);                // translation, or `english` when there is none
std::string tr(const std::string& english);
#ifdef __GNUC__
#define JOHN_PRINTF_ARGS __attribute__((format(printf, 1, 2)))  // GCC/Clang check trf()'s arguments
#else
#define JOHN_PRINTF_ARGS  // MSVC
#endif
std::string trf(const char* english, ...) JOHN_PRINTF_ARGS;  // tr() then printf
std::string songText(const Song& s, const std::string& english);  // s.i18n entry for the current language, else english
// notes: noteName() says Do Re Mi instead of C D E. keys: keyName() does ("Ré maior"), but only in a language other
// than English; English keys are always letters ("D major").
void setSolfege(bool notes, bool keys);

// ---- theory ----
std::string parseMeter(const std::string& meter);  // "N/D" (D = 2, 4, 8, 16; 1 <= N <= 32), "C" -> "4/4", "C|" -> "2/2"; "" if invalid
int meterTicks(const std::string& meter);          // ticks per bar
bool compoundMeter(const std::string& meter);      // 6/8, 9/8, 12/8
int keyAlter(const Key& k, int letter);            // key signature alteration for a letter
Spelled spell(const Key& k, int midi);
std::string noteName(const Spelled& s);            // "F#4" ("Fá#4" with setSolfege notes)
std::string keyName(const Key& k);                 // "D major", "E dorian"; translated by tr()
std::string tonicName(const Key& k);               // "D" ("Ré" with setSolfege keys in a non-English language)
int tonicPc(const Key& k);                         // pitch class of the tonic, C = 0: D major and B minor -> 2, 11
std::optional<Key> parseKey(const std::string& s); // "D", "Ador", "Bm", "G mixolydian"
int whistleBase(const std::string& whistle);       // MIDI of the lowest written note, D -> 62; -1 if not a whistle key
Fingering finger(const std::string& whistle, int midi);
std::string fingeringText(const Fingering& f);     // "XXX XXO +"
std::string durationName(const Event& e);          // "dotted quarter", "eighth (3-tuplet)"; translated by tr()
void transpose(Song& s, int semitones);         // also adds to s.transposed
// Notes out of range (or impossible) and hard notes (in's note table) if s were transposed by semis. Written pitches are
// compared, whatever instrument the song is for. half/cross count hard notes played by half-holing / cross-fingering,
// other the rest (bends, overblows, ...). hard() weighs them all, 10 per weight-1 note: a whistle half-hole is 10, its
// cross-fingered C 2. weight defaults to that formula so a literal RangeCost{out, half, cross} means what it did.
struct RangeCost {
  int out = 0, half = 0, cross = 0;
  int weight = half * 10 + cross * 2;
  int other = 0;
  int hard() const { return weight; }
};
RangeCost rangeCost(const Song& s, const Instrument& in, int semis);
// Play the song in the key whose tonic is pitch class `tonic` (same mode): transposes by the smallest interval, then
// moves whole octaves to the octave that fits `in` best (rangeCost: fewest out, then hard()). On a tie, the octave
// closest to the original sheet (s.transposed nearest 0), so the original key restores it exactly. Returns the shift.
int transposeToKey(Song& s, int tonic, const Instrument& in);
// The share of notes, in percent, that makes a song hard to play: hard notes (RangeCost::hard() / 10, so a half-hole
// counts 1 and a cross-fingered C 1/5) at or above it and the song doesn't fit (playability); out-of-range notes at or
// above it and Import song points to Play in key.
constexpr int PLAYABLE_PERCENT = 10;
bool fits(const RangeCost& c, int notes);  // no note out of range and hard notes under PLAYABLE_PERCENT
// Can `in` play the song (written pitches, see rangeCost)? AsWritten: it fits at shift 0. Otherwise every key in the
// same mode is tried at its best octave (as transposeToKey); the best is the one with fewest notes out, then one that
// fits, then the nearest key (the same key an octave away first), then fewest hard notes. OtherKey: transposing by
// `shift` fits, `key` is the key after it (the same key when shift is a whole octave). NoFit: nothing fits, `cost.out`
// notes stay out at best.
struct Playability {
  enum Verdict { AsWritten, OtherKey, NoFit } verdict = AsWritten;
  int shift = 0;
  Key key;
  RangeCost cost;  // at shift
  int notes = 0;
};
Playability playability(const Song& s, const Instrument& in);
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
// Note i continues a tie from the previous note: the same sound, so it takes no lyric syllable.
bool tiedFrom(const Song& s, int i);

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

// ---- catalog & storage ----
std::vector<Technique> defaultTechniques();       // names and descriptions translated by tr()
std::string homeDir();                            // $HOME, or %USERPROFILE% on Windows
fs::path dataDir();                         // $JOHN_HOME or ~/Documents/John
std::vector<Technique> loadTechniques();    // defaults merged with dataDir()/techniques.json
void saveCustomTechnique(const Technique& t);
bool builtinTechnique(const std::string& id);  // in defaultTechniques()
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
  std::string sound;                // playback sound, a SOUNDS id; "" = my instrument's
  float gate = 0.01f;               // mic level below which nothing is heard
  float latencyMs = 80;             // mic + speaker delay, for grading play-along
  float tolerance = 40;             // cents from the note that still count as right
  std::string instrument = "whistle";  // my instrument: the song list shows what fits it; new songs are for it
  std::string whistle = "D";        // my whistle's key, when instrument is "whistle"
  bool allSongs = false;            // song list shows every song, not only those that fit my instrument
  std::string language;             // viewer language: "" = automatic (system/browser), "en", "pt-BR", ...
  bool solfege = false;             // note names Do Re Mi instead of C D E
  bool solfegeKeys = true;          // key names Do Re Mi ("Ré maior"); only in a language other than English
  std::string harp = "C";           // my harmonica's key (also for songs switched to or imported as harmonica), one of HARP_KEYS
  bool allMarks = false;            // the sheet shows every technique mark, not only my instrument's (marks())
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
// MusicXML (musicxml.cpp): .musicxml/.xml text or the bytes of a zipped .mxl. The first part's first voice, chords
// reduced to their top note, one key and meter (the first); what is dropped or rounded becomes a warning. Throws when
// the file can't be read or has no notes.
ImportResult importMusicXml(const std::string& data);
inline constexpr size_t MXL_MAX_ENTRY = 64u << 20;  // bigger .mxl entries are refused before unzipping (zip bombs)
bool isMusicXml(const std::string& data);         // an .mxl (zip) or text with a <score-partwise>/<score-timewise> root
// Image import (#75): copisteria sometimes misses the key signature of the first line only. In the reader's MusicXML
// (lines start at <print new-system/new-page>): the first line's key differs from one key that starts on line 2 and
// holds to the end. Only when the first line has no sharps or flats, and no written accidental that the later key would
// make unneeded (a sign of a real key change), is the first line given the later key (`fixed`): its notes without a
// written accidental take the key's sharps or flats, and `xml` is rewritten. nullopt: nothing to report.
struct FirstLineKey { int first = 0, rest = 0; bool fixed = false; };  // fifths of the first line and of the rest
std::optional<FirstLineKey> checkFirstLineKey(std::string& xml);
std::string graceTechnique(const std::vector<int>& graces, int midi);  // grace pitches before a note: cut, tap, roll, double_cut
// Import MusicXML (isMusicXml) as one song, or every tune (X:) of ABC, as new saved songs: instrument, whistle, harp and
// folder from `like`, source too when set (else the ABC's C:/S:/Z:, the MusicXML composer). Ids come from
// uniqueId(title); the first song's from like.id when set (slug(like.id) as is with overwrite). Warnings name their
// tune when there are several. An ABC tune needs a K: line and notes; throws if none has. Used by import_abc,
// import_musicxml and the viewer's Import song.
std::vector<ImportResult> importSongs(const std::string& data, const Song& like, bool overwrite = false);
std::string listing(const Song& s);          // compact text table of every event, for the agent
// Deterministic technique candidates + range report (on the song's instrument). The candidates are for `player`
// (default the song's instrument): marks it lists as they are, equivalents named as such, hidden ones left out (#86).
std::string analyze(const Song& s, const std::optional<Instrument>& player = std::nullopt);

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
std::string uniqueId(const std::string& title);  // slug(title), with -2, -3... if a song already has it

}  // namespace john
