// Theory, fingerings, technique catalog, storage and the agent-facing listing.
#include "john.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace john {

static const int NATURAL_PC[7] = {0, 2, 4, 5, 7, 9, 11};  // C D E F G A B
static const char* LETTERS = "CDEFGAB";
static const int SHARP_ORDER[7] = {3, 0, 4, 1, 5, 2, 6};  // F C G D A E B
static const int FLAT_ORDER[7] = {6, 2, 5, 1, 4, 0, 3};   // B E A D G C F

static int mod12(int x) { return ((x % 12) + 12) % 12; }

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

std::string noteName(const Spelled& s) {
  std::string r(1, LETTERS[s.letter]);
  if (s.alter > 0) r += "#";
  if (s.alter < 0) r += "b";
  return r + std::to_string(s.octave);
}

static const char* MODES[] = {"maj", "min", "dor", "mix", "lyd", "phr", "loc"};
static const int MODE_FIFTHS[] = {0, -3, -2, -1, 1, -4, -5};
static const int MODE_SEMIS[] = {0, 9, 2, 7, 5, 4, 11};  // tonic above the relative major
static const char* MODE_NAMES[] = {"major", "minor", "dorian", "mixolydian", "lydian", "phrygian", "locrian"};

static int modeIndex(const std::string& m) {
  for (int i = 0; i < 7; i++) if (m == MODES[i]) return i;
  return 0;
}

std::string keyName(const Key& k) {
  int mi = modeIndex(k.mode);
  int tonicPc = mod12(k.fifths * 7 + MODE_SEMIS[mi]);
  Spelled t = spell(k, 60 + tonicPc);
  std::string n = noteName(t);
  n.pop_back();  // drop octave digit
  return n + " " + MODE_NAMES[mi];
}

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

std::optional<Instrument> findInstrument(const std::string& id, const std::string& whistleKey) {
  if (id == "whistle") {
    int b = whistleBase(whistleKey);
    if (b < 0) return std::nullopt;
    return Instrument{id, whistleKey + " whistle", b, b + 23, 1};
  }
  // ponytail: ranges only. Harmonica = 10-hole diatonic in C without bends (it can't play every note in range);
  // add holes/bends or other harmonica keys when harmonica hints are needed.
  static const Instrument FIXED[] = {
      {"violin", "Violin", 55, 100, 0},                      // G3..E7
      {"piano", "Piano", 21, 108, 0},                        // A0..C8
      {"guitar", "Guitar", 52, 96, -1},                      // written E3..C7, sounds an octave lower (20 frets)
      {"electric-guitar", "Electric guitar", 52, 100, -1},   // 24 frets
      {"harmonica", "Harmonica", 60, 96, 0},                 // C4..C7
  };
  for (auto& i : FIXED) if (i.id == id) return i;
  return std::nullopt;
}

Instrument instrumentOf(const Song& s) {
  if (auto i = findInstrument(s.instrument, s.whistle)) return *i;
  return *findInstrument("whistle", "D");
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
    if (len == v) { r = n; break; }
    if (len * 2 == v * 3) { r = std::string("dotted ") + n; break; }
    if (len * 4 == v * 7) { r = std::string("double-dotted ") + n; break; }
  }
  if (r.empty()) r = std::to_string(e.len) + " ticks";
  if (e.tuplet) r += " (" + std::to_string(e.tuplet) + "-tuplet)";
  return r;
}

void transpose(Song& s, int semis) {
  for (auto& e : s.events) if (e.kind == Kind::Note) e.midi += semis;
  static const int PC_FIFTHS[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};
  int mi = modeIndex(s.key.mode);
  int tonic = mod12(s.key.fifths * 7 + MODE_SEMIS[mi] + semis);
  int f = PC_FIFTHS[mod12(tonic - MODE_SEMIS[mi])];
  s.key.fifths = f;
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

// ---------------- catalog ----------------

std::vector<Technique> defaultTechniques() {
  return {
      {"cut", "Cut", "^", "Grace note ABOVE the melody note: flick a finger up off a hole just above the sounding note and put it straight back. Separates two notes of the same pitch without tonguing, and accents a note. Usually the G finger (cutting notes below G) or the A/B finger for upper notes."},
      {"tap", "Tap (strike)", "v", "Grace note BELOW: slap a finger down on the hole just below the sounding note and lift it immediately. Accents a note; impossible on bottom D."},
      {"roll", "Long roll", "~", "Note, cut, note, tap, note, all in the time of a dotted quarter (3 eighths). The signature ornament on long notes in jigs and reels. Not possible on bottom D (use a cran)."},
      {"short_roll", "Short roll", "s~", "Starts directly with the cut (cut, note, tap, note), in the time of a quarter note. Fits where a long roll does not."},
      {"cran", "Cran", "cr", "Series of quick cuts with no taps, on notes where a roll is impossible: bottom D and E. Borrowed from the uilleann pipes."},
      {"slide", "Slide", "/", "Glide up into the note by slowly uncovering the hole(s) from the note below. Expressive; common on F#, G, A and B in slow airs and polkas."},
      {"tongue", "Tongued", "T", "Start the note with the tongue ('tu' or 'du'). Irish style slurs most notes; tongue only phrase starts and accented notes."},
      {"breath", "Breath", ",", "Take a breath AFTER this note. Usually done by leaving out a weak note or shortening a long one."},
      {"staccato", "Staccato", ".", "Short, detached note."},
      {"accent", "Accent", ">", "Emphasise the note with breath or tonguing."},
      {"vibrato", "Vibrato", "vib", "Pulse the breath (diaphragm) or shake a finger over an open hole below the note. Mostly for slow airs."},
      {"trill", "Trill", "tr", "Rapid alternation with the note above."},
      {"double_cut", "Double cut", "^^", "Two quick cuts in a row before the note, giving a stutter. A variant of rolls and crans."},
  };
}

fs::path dataDir() {
#ifdef __EMSCRIPTEN__
  fs::path p = "/john";  // IndexedDB-backed folder (IDBFS), mounted and loaded by web/pre.js before main()
#else
  const char* h = std::getenv("JOHN_HOME");
  fs::path p = h && *h ? fs::path(h) : fs::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / "Documents" / "John";
#endif
  fs::create_directories(p / "songs");
  return p;
}

// Web: copy the data folder to IndexedDB after every save, so it survives a reload. Desktop: nothing to do.
static void persist() {
#ifdef __EMSCRIPTEN__
  EM_ASM(FS.syncfs(false, function(e) { if (e) console.error("John: saving to IndexedDB failed", e); }););
#endif
}

static json readJson(const fs::path& p) {
  std::ifstream f(p);
  if (!f) return nullptr;
  return json::parse(f, nullptr, false);  // discarded value on error
}

static void writeJson(const fs::path& p, const json& j) {
  // write-then-rename so the GUI never reads a half-written file
  fs::path tmp = p;
  tmp += ".tmp" + std::to_string(getpid());
  { std::ofstream f(tmp); f << j.dump(1) << "\n"; }
  fs::rename(tmp, p);
  persist();
}

std::vector<Technique> loadTechniques() {
  auto all = defaultTechniques();
  json j = readJson(dataDir() / "techniques.json");
  if (!j.is_array()) return all;
  for (auto& t : j) {
    Technique x;
    try { x = {t.value("id", ""), t.value("name", ""), t.value("symbol", ""), t.value("description", "")}; }
    catch (const json::exception&) { continue; }  // skip malformed entries
    if (x.id.empty()) continue;
    auto it = std::find_if(all.begin(), all.end(), [&](auto& a) { return a.id == x.id; });
    if (it != all.end()) *it = x; else all.push_back(x);
  }
  return all;
}

void saveCustomTechnique(const Technique& t) {
  fs::path p = dataDir() / "techniques.json";
  json j = readJson(p);
  if (!j.is_array()) j = json::array();
  json x = {{"id", t.id}, {"name", t.name}, {"symbol", t.symbol}, {"description", t.description}};
  bool replaced = false;
  for (auto& e : j) if (e.value("id", "") == t.id) { e = x; replaced = true; }
  if (!replaced) j.push_back(x);
  writeJson(p, j);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Settings, hints, zoom, light, tempo, countIn, metronome, volume, sound,
                                                gate, latencyMs, tolerance, instrument, songFilter)

Settings loadSettings() {
  Settings s;
  try { s = readJson(dataDir() / "settings.json").get<Settings>(); }
  catch (const json::exception&) { return {}; }  // missing, garbage, or a key of the wrong type
  auto known = [](const std::string& id) { return std::count(INSTRUMENTS.begin(), INSTRUMENTS.end(), id) > 0; };
  if (!s.sound.empty() && !known(s.sound)) s.sound.clear();
  if (!s.songFilter.empty() && !known(s.songFilter)) s.songFilter.clear();
  if (!known(s.instrument)) s.instrument = "whistle";
  s.zoom = std::clamp(s.zoom, 6.0f, 16.0f);  // values the GUI could choke on
  s.tempo = std::clamp(s.tempo, 40, 220);
  s.volume = std::clamp(s.volume, 0.0f, 1.0f);
  return s;
}

void saveSettings(const Settings& s) { writeJson(dataDir() / "settings.json", json(s)); }

std::string slug(const std::string& title) {
  std::string r;
  for (unsigned char c : title) {
    if (isalnum(c)) r += (char)tolower(c);
    else if (!r.empty() && r.back() != '-') r += '-';
  }
  while (!r.empty() && r.back() == '-') r.pop_back();
  return r.empty() ? "song" : r;
}

json toJson(const Song& s) {
  json ev = json::array();
  for (auto& e : s.events) {
    json x;
    if (e.kind == Kind::Bar) {
      x = {{"type", "bar"}, {"bar", e.bar}};
      if (!e.ending.empty()) x["ending"] = e.ending;
    } else {
      x = {{"type", e.kind == Kind::Note ? "note" : "rest"}, {"len", e.len}};
      if (e.kind == Kind::Note) x["midi"] = e.midi;
      if (e.tie) x["tie"] = true;
      if (e.tuplet) { x["tuplet"] = e.tuplet; x["tuplet_q"] = e.tupletQ; }
      if (e.tupletStart) x["tuplet_start"] = true;
      if (!e.techniques.empty()) x["techniques"] = e.techniques;
      if (!e.comment.empty()) x["comment"] = e.comment;
    }
    ev.push_back(x);
  }
  json j = {{"id", s.id}, {"title", s.title}, {"source", s.source}, {"notes", s.notes},
          {"meter", s.meter}, {"key", {{"fifths", s.key.fifths}, {"mode", s.key.mode}}},
          {"tempo", s.tempo}, {"instrument", s.instrument}, {"whistle", s.whistle}, {"events", ev}};
  if (!s.folder.empty()) j["folder"] = s.folder;
  return j;
}

bool folderBefore(const std::string& a, const std::string& b) {
  auto rank = [](const std::string& f) {
    auto it = std::find(FOLDERS.begin(), FOLDERS.end(), f);
    return f.empty() ? 1000 : it != FOLDERS.end() ? (int)(it - FOLDERS.begin()) : 100;
  };
  return rank(a) != rank(b) ? rank(a) < rank(b) : a < b;
}

Song fromJson(const json& j) {
  Song s;
  s.id = j.value("id", "");
  s.title = j.value("title", "");
  s.source = j.value("source", "");
  s.notes = j.value("notes", "");
  s.folder = j.value("folder", "");
  s.meter = j.value("meter", "4/4");
  if (j.contains("key")) s.key = {std::clamp(j["key"].value("fifths", 2), -7, 7), j["key"].value("mode", "maj")};
  s.tempo = j.value("tempo", 0);
  s.instrument = j.value("instrument", "whistle");
  s.whistle = j.value("whistle", "D");
  for (auto& x : j.value("events", json::array())) {
    Event e;
    std::string t = x.value("type", "note");
    e.kind = t == "bar" ? Kind::Bar : t == "rest" ? Kind::Rest : Kind::Note;
    e.midi = x.value("midi", 0);
    e.len = x.value("len", 0);
    e.tie = x.value("tie", false);
    e.tuplet = x.value("tuplet", 0);
    e.tupletQ = x.value("tuplet_q", 0);
    e.tupletStart = x.value("tuplet_start", false);
    e.techniques = x.value("techniques", std::vector<std::string>{});
    e.comment = x.value("comment", "");
    e.bar = x.value("bar", "");
    e.ending = x.value("ending", "");
    s.events.push_back(e);
  }
  return s;
}

std::vector<std::string> listSongs() {
  std::vector<std::string> r;
  for (auto& f : fs::directory_iterator(dataDir() / "songs"))
    if (f.path().extension() == ".json") r.push_back(f.path().stem().string());
  std::sort(r.begin(), r.end());
  return r;
}

std::optional<Song> loadSong(const std::string& id) {
  if (id.empty() || id.find('/') != std::string::npos || id.find("..") != std::string::npos) return std::nullopt;
  json j = readJson(dataDir() / "songs" / (id + ".json"));
  if (!j.is_object()) return std::nullopt;
  try {
    Song s = fromJson(j);
    s.id = id;
    return s;
  } catch (const json::exception&) {
    return std::nullopt;  // malformed file: hide it rather than break every listing
  }
}

void saveSong(const Song& s) { writeJson(dataDir() / "songs" / (s.id + ".json"), toJson(s)); }

fs::path bundled(const fs::path& rel, const fs::path& built) {
  std::error_code ec;
  fs::path dir;  // the running executable's folder
#if defined(__APPLE__)
  char buf[4096];
  uint32_t n = sizeof buf;
  if (_NSGetExecutablePath(buf, &n) == 0) dir = fs::canonical(buf, ec).parent_path();
#elif defined(__linux__)
  dir = fs::canonical("/proc/self/exe", ec).parent_path();
#endif
  return !dir.empty() && fs::exists(dir / rel, ec) ? dir / rel : built;
}

fs::path lessonsDir() {
#ifdef JOHN_LESSONS
  return bundled("lessons", JOHN_LESSONS);
#else
  return {};
#endif
}

// ponytail: a lesson is copied once; later edits to the repo's copy don't reach existing installs.
int installLessons(const fs::path& from) {
  std::error_code ec;
  if (from.empty() || !fs::is_directory(from, ec)) return 0;
  fs::path marker = dataDir() / "lessons-installed.txt";  // so a lesson the user deletes stays deleted
  std::set<std::string> done;
  { std::ifstream f(marker); for (std::string l; std::getline(f, l);) if (!l.empty()) done.insert(l); }
  std::ofstream out(marker, std::ios::app);
  int n = 0;
  for (auto& e : fs::directory_iterator(from, ec)) {
    std::string id = e.path().stem().string();
    if (e.path().extension() != ".json" || done.count(id)) continue;
    fs::path to = dataDir() / "songs" / (id + ".json");
    if (!fs::exists(to) && fs::copy_file(e.path(), to, ec)) n++;
    out << id << "\n";
  }
  out.close();
  persist();
  return n;
}

// ---------------- listing ----------------

static std::string beatLabel(const Song& s, int tickInBar) {
  int beat = compoundMeter(s.meter) ? WHOLE * 3 / 8 : WHOLE / 4;
  double b = 1.0 + (double)tickInBar / beat;
  char buf[16];
  snprintf(buf, sizeof buf, "%g", std::round(b * 100) / 100);
  return buf;
}

std::string listing(const Song& s) {
  std::ostringstream o;
  const Instrument in = instrumentOf(s);
  const bool whistle = in.id == "whistle";
  o << "id: " << s.id << "\ntitle: " << s.title << "\nkey: " << keyName(s.key) << "  meter: " << s.meter
    << "  instrument: " << in.name << "  tempo: " << (s.tempo ? std::to_string(s.tempo) : "?") << "\n";
  if (!s.source.empty()) o << "source: " << s.source << "\n";
  if (!s.notes.empty()) o << "notes: " << s.notes << "\n";
  o << "\n#idx  bar beat  note  length                   " << (whistle ? "fingering    " : "range        ") << " techniques  // comment\n";
  auto pos = positions(s);
  for (size_t i = 0; i < s.events.size(); i++) {
    const Event& e = s.events[i];
    char buf[160];
    if (e.kind == Kind::Bar) {
      snprintf(buf, sizeof buf, "#%-4zu %s%s\n", i, e.bar.c_str(), e.ending.empty() ? "" : ("  ending " + e.ending).c_str());
      o << buf;
      continue;
    }
    std::string name = e.kind == Kind::Note ? noteName(spell(s.key, e.midi)) : "rest";
    std::string fing = e.kind != Kind::Note ? "" : whistle ? fingeringText(finger(s.whistle, e.midi)) : in.plays(e.midi) ? "" : "out-of-range";
    std::string tech;
    for (auto& t : e.techniques) tech += (tech.empty() ? "" : ",") + t;
    snprintf(buf, sizeof buf, "#%-4zu %-3d %-5s %-5s %-24s %-13s %s", i, pos[i].bar, beatLabel(s, pos[i].tick).c_str(), name.c_str(),
             (durationName(e) + (e.tie ? " tie" : "")).c_str(), fing.c_str(), tech.c_str());
    std::string row = buf;
    while (!row.empty() && row.back() == ' ') row.pop_back();
    o << row;
    if (!e.comment.empty()) o << "  // " << e.comment;
    o << "\n";
  }
  return o.str();
}

}  // namespace john
