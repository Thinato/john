// Music theory: meters, keys, spelling, fingerings, durations, transposition, bars and positions, score layout.
#include "john.hpp"
#include "internal.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>

namespace john {

static const int NATURAL_PC[7] = {0, 2, 4, 5, 7, 9, 11};  // C D E F G A B
static const char* LETTERS = "CDEFGAB";
static const int SHARP_ORDER[7] = {3, 0, 4, 1, 5, 2, 6};  // F C G D A E B
static const int FLAT_ORDER[7] = {6, 2, 5, 1, 4, 0, 3};   // B E A D G C F

static bool gSolfege = false, gSolfegeKeys = false;  // set by the GUI only

std::string parseMeter(const std::string& m) {
  if (m == "C") return "4/4";
  if (m == "C|") return "2/2";
  int n = 0, d = 0, used = 0;
  if (sscanf(m.c_str(), "%d/%d%n", &n, &d, &used) != 2 || m[used] || n < 1 || n > 32 || (d != 2 && d != 4 && d != 8 && d != 16)) return "";
  return std::to_string(n) + "/" + std::to_string(d);
}

int meterTicks(const std::string& m) {
  if (m == "C") return WHOLE;
  if (m == "C|") return WHOLE;
  int n = 4, d = 4;
  if (sscanf(m.c_str(), "%d/%d", &n, &d) != 2 || n <= 0 || d <= 0 || n > 32 || d > 64) return WHOLE;
  return std::max(WHOLE / 64, n * WHOLE / d);
}

bool compoundMeter(const std::string& m) {
  int n = 0, d = 0;
  return sscanf(m.c_str(), "%d/%d", &n, &d) == 2 && d == 8 && n > 3 && n % 3 == 0;
}

int keyAlter(const Key& k, int letter) {
  for (int i = 0; i < std::min(7, std::abs(k.fifths)); i++) {
    if (k.fifths > 0 && SHARP_ORDER[i] == letter) return 1;
    if (k.fifths < 0 && FLAT_ORDER[i] == letter) return -1;
  }
  return 0;
}

Spelled spell(const Key& k, int midi) {
  int pc = mod12(midi);
  Spelled s;
  bool found = false;
  for (int l = 0; l < 7 && !found; l++)
    if (mod12(NATURAL_PC[l] + keyAlter(k, l)) == pc) { s.letter = l; s.alter = keyAlter(k, l); found = true; }
  for (int l = 0; l < 7 && !found; l++)
    if (NATURAL_PC[l] == pc) { s.letter = l; s.alter = 0; found = true; }
  int a = k.fifths >= 0 ? 1 : -1;
  for (int l = 0; l < 7 && !found; l++)
    if (mod12(NATURAL_PC[l] + a) == pc) { s.letter = l; s.alter = a; found = true; }
  s.octave = (midi - s.alter) / 12 - 1;
  return s;
}

bool grandStaff(const Song& s) { return s.instrument == "piano"; }
bool onBassStaff(const Song& s, int midi) { return grandStaff(s) && midi < 60; }

Room systemRoom(const Song& s, int from, int to, int free) {
  Room r;
  const bool grand = grandStaff(s);
  for (int i = std::max(from, 0); i < to && i < (int)s.events.size(); i++) {
    const Event& e = s.events[i];
    if (e.kind != Kind::Note) continue;
    int st = spell(s.key, e.midi).step();
    bool bass = onBassStaff(s, e.midi);
    if (!bass) r.above = std::max(r.above, st - 44);                             // top staff: treble
    if (bass == grand) r.below = std::max(r.below, (grand ? 18 : 30) - free - st);  // bottom staff
  }
  return r;
}

void setSolfege(bool notes, bool keys) { gSolfege = notes; gSolfegeKeys = keys; }

static std::string pitchName(const Spelled& s, bool solfege) {
  static const char* SOLFEGE[7] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si"};
  std::string r = solfege ? tr(SOLFEGE[s.letter]) : std::string(1, LETTERS[s.letter]);
  if (s.alter > 0) r += "#";
  if (s.alter < 0) r += "b";
  return r + std::to_string(s.octave);
}

std::string noteName(const Spelled& s) { return pitchName(s, gSolfege); }

static const char* MODES[] = {"maj", "min", "dor", "mix", "lyd", "phr", "loc"};
static const int MODE_FIFTHS[] = {0, -3, -2, -1, 1, -4, -5};
static const int MODE_SEMIS[] = {0, 9, 2, 7, 5, 4, 11};  // tonic above the relative major
static const char* MODE_FMT[] = {"%s major", "%s minor", "%s dorian", "%s mixolydian", "%s lydian", "%s phrygian", "%s locrian"};

static int modeIndex(const std::string& m) {
  for (int i = 0; i < 7; i++) if (m == MODES[i]) return i;
  return 0;
}

int tonicPc(const Key& k) { return mod12(k.fifths * 7 + MODE_SEMIS[modeIndex(k.mode)]); }

std::string tonicName(const Key& k) {
  std::string n = pitchName(spell(k, 60 + tonicPc(k)), gLocalized && gSolfegeKeys);  // English keys always use letters
  n.pop_back();  // drop octave digit
  return n;
}

std::string keyName(const Key& k) { return trf(MODE_FMT[modeIndex(k.mode)], tonicName(k).c_str()); }

std::optional<Key> parseKey(const std::string& raw) {
  std::string s;
  for (char c : raw) if (c != ' ') s += c;
  if (s.empty()) return Key{0, "maj"};
  static const int LETTER_FIFTHS[7] = {0, 2, 4, -1, 1, 3, 5};
  const char* p = strchr(LETTERS, toupper(s[0]));
  if (!p || !*p) return std::nullopt;
  int f = LETTER_FIFTHS[p - LETTERS];
  size_t i = 1;
  if (i < s.size() && s[i] == '#') { f += 7; i++; }
  else if (i < s.size() && s[i] == 'b') { f -= 7; i++; }
  std::string m;
  for (; i < s.size() && isalpha((unsigned char)s[i]); i++) m += (char)tolower(s[i]);
  int mi = 0;
  if (m == "m") mi = 1;
  else if (m.size() >= 3) {
    mi = -1;
    for (int k = 0; k < 7; k++) if (m.compare(0, 3, MODES[k]) == 0) mi = k;
    if (m.compare(0, 3, "ion") == 0) mi = 0;
    if (m.compare(0, 3, "aeo") == 0) mi = 1;
    if (mi < 0) return std::nullopt;
  } else if (!m.empty()) return std::nullopt;
  f += MODE_FIFTHS[mi];
  if (f < -7 || f > 7) return std::nullopt;
  return Key{f, MODES[mi]};
}

int whistleBase(const std::string& w) {
  if (w.empty() || w.size() > 2 || (w.size() == 2 && w[1] != '#' && w[1] != 'b')) return -1;
  auto k = parseKey(w);
  if (!k) return -1;
  return 60 + mod12(k->fifths * 7);  // C whistle = C4 (60) ... B whistle = B4 (71); D = 62
}

int parseNote(const std::string& s) {
  static const int SEMI[7] = {9, 11, 0, 2, 4, 5, 7};  // A..G
  if (s.size() < 2 || s[0] < 'A' || s[0] > 'G') return -1;
  int m = SEMI[s[0] - 'A'];
  size_t i = 1;
  if (s[i] == '#' || s[i] == 'b') m += s[i++] == '#' ? 1 : -1;
  if (i == s.size()) return -1;
  char* end = nullptr;
  long oct = std::strtol(s.c_str() + i, &end, 10);
  if (*end || oct < -1 || oct > 9) return -1;
  m += (int)(oct + 1) * 12;
  return m >= 0 && m <= 127 ? m : -1;
}

Fingering finger(const std::string& whistle, int midi) {
  // Offsets from the whistle's bottom note. Same pattern both octaves; C natural uses the OXX OOO cross fingering.
  static const char* F[12] = {"XXXXXX", "XXXXXH", "XXXXXO", "XXXXHO", "XXXXOO", "XXXOOO",
                              "XXHOOO", "XXOOOO", "XHOOOO", "XOOOOO", "OXXOOO", "OOOOOO"};
  int base = whistleBase(whistle);
  int off = midi - base;
  if (base < 0 || off < 0 || off > 23) return {"", 0, false};
  return {F[off % 12], off / 12, true};
}

std::string fingeringText(const Fingering& f) {
  if (!f.ok) return "out-of-range";
  std::string r = f.holes.substr(0, 3) + " " + f.holes.substr(3);
  if (f.octave) r += " " + std::string(f.octave, '+');
  return r;
}

std::string durationName(const Event& e) {
  int len = e.len;
  if (e.tuplet && e.tupletQ) len = len * e.tuplet / e.tupletQ;
  static const std::pair<int, const char*> V[] = {
      {WHOLE, "whole"}, {WHOLE / 2, "half"}, {WHOLE / 4, "quarter"},
      {WHOLE / 8, "eighth"}, {WHOLE / 16, "sixteenth"}, {WHOLE / 32, "32nd"}};
  std::string r;
  for (auto [v, n] : V) {
    if (len == v) { r = tr(n); break; }
    if (len * 2 == v * 3) { r = trf("dotted %s", tr(n)); break; }
    if (len * 4 == v * 7) { r = trf("double-dotted %s", tr(n)); break; }
  }
  if (r.empty()) r = trf("%d ticks", e.len);
  if (e.tuplet) r += trf(" (%d-tuplet)", e.tuplet);
  return r;
}

void transpose(Song& s, int semis) {
  for (auto& e : s.events) if (e.kind == Kind::Note) e.midi += semis;
  static const int PC_FIFTHS[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};
  int mi = modeIndex(s.key.mode);
  int tonic = mod12(s.key.fifths * 7 + MODE_SEMIS[mi] + semis);
  int f = PC_FIFTHS[mod12(tonic - MODE_SEMIS[mi])];
  s.key.fifths = f;
  s.transposed += semis;
}

int deleteBars(Song& s, int first, int last) {
  if (first > last) throw std::invalid_argument("first bar is after last bar");
  auto pos = positions(s);
  std::vector<Event> out;
  int removed = 0, kept = 0;
  bool prevDeleted = false;  // was the last note/rest seen deleted? then the next bar line closed a deleted bar
  for (size_t i = 0; i < s.events.size(); i++) {
    const Event& e = s.events[i];
    if (e.kind != Kind::Bar) {
      bool del = pos[i].bar >= first && pos[i].bar <= last;
      if (del && !prevDeleted)  // a tie can't lead into a deleted note
        for (auto it = out.rbegin(); it != out.rend(); ++it)
          if (it->kind != Kind::Bar) { it->tie = false; break; }
      prevDeleted = del;
      if (del) { removed++; continue; }
      kept++;
      out.push_back(e);
      continue;
    }
    if (!prevDeleted) { out.push_back(e); continue; }
    if (!out.empty() && out.back().kind == Kind::Bar) {  // keep repeat/end signs and volta marks
      Event& b = out.back();
      if (b.bar == "|") b.bar = e.bar;
      if (b.ending.empty()) b.ending = e.ending;
    }
  }
  if (!removed) throw std::invalid_argument("no bars numbered " + std::to_string(first) + ".." + std::to_string(last));
  if (!kept) throw std::invalid_argument("that would delete every bar");
  while (!out.empty() && out.front().kind == Kind::Bar && out.front().bar == "|" && out.front().ending.empty()) out.erase(out.begin());
  s.events = std::move(out);
  return removed;
}

std::vector<Accidental> accidentals(const Song& s) {
  std::vector<Accidental> out(s.events.size());
  std::map<int, int> cur;  // diatonic step -> alteration in effect in this bar
  const Event* prev = nullptr;
  for (size_t i = 0; i < s.events.size(); i++) {
    const Event& e = s.events[i];
    if (e.kind == Kind::Bar) { cur.clear(); continue; }
    if (e.kind != Kind::Note) continue;
    Spelled sp = spell(s.key, e.midi);
    auto it = cur.find(sp.step());
    int inEffect = it != cur.end() ? it->second : keyAlter(s.key, sp.letter);
    bool tiedIn = prev && prev->tie && prev->midi == e.midi;
    out[i] = {sp.alter != inEffect && !tiedIn, sp.alter};
    cur[sp.step()] = sp.alter;
    prev = &e;
  }
  return out;
}

bool tiedFrom(const Song& s, int i) {
  for (int j = i - 1; j >= 0; j--)
    if (s.events[j].kind != Kind::Bar) return s.events[i].kind == Kind::Note && s.events[j].kind == Kind::Note && s.events[j].tie;
  return false;
}

std::vector<Pos> positions(const Song& s) {
  const int m = meterTicks(s.meter);
  int pickup = 0;
  for (auto& e : s.events) {
    if (e.kind == Kind::Bar) { if (pickup) break; continue; }
    pickup += e.len;
  }
  if (pickup >= m) pickup = 0;
  std::vector<Pos> out;
  long t = 0, voltaStart = -1;
  for (auto& e : s.events) {
    if (e.kind == Kind::Bar && !e.ending.empty()) {  // later endings replay the time of the first one
      if (e.ending[0] == '1' || voltaStart < 0) voltaStart = t;
      else t = voltaStart;
    }
    out.push_back(t < pickup ? Pos{0, int(t + m - pickup)} : Pos{int(1 + (t - pickup) / m), int((t - pickup) % m)});
    if (e.kind != Kind::Bar) t += e.len;
  }
  return out;
}

}  // namespace john
