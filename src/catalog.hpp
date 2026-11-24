// The catalog: techniques and instrument profiles (catalog.cpp).
#pragma once
#include "song.hpp"

namespace john {

struct Technique {
  std::string id;          // "roll"
  std::string name;        // "Long roll"
  std::string symbol;      // text drawn above the note, e.g. "~"
  std::string description;
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

// ---- techniques ----
std::vector<Technique> defaultTechniques();       // names and descriptions translated by tr()
std::vector<Technique> loadTechniques();    // defaults merged with dataDir()/techniques.json
void saveCustomTechnique(const Technique& t);
bool builtinTechnique(const std::string& id);  // in defaultTechniques()

}  // namespace john
