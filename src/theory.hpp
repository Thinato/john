// Music theory, transposition, bars and positions, score layout (theory.cpp).
#pragma once
#include "song.hpp"

namespace john {

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

}  // namespace john
