// The data folder, user settings, and the songs bundled with John (lessons/, demo/).
#include "john.hpp"
#include "internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // GetModuleFileNameW
#endif

namespace john {

std::string homeDir() {
#ifdef _WIN32
  const char* h = std::getenv("USERPROFILE");  // Windows usually has no HOME
#else
  const char* h = std::getenv("HOME");
#endif
  return h ? h : ".";
}

fs::path dataDir() {
#ifdef __EMSCRIPTEN__
  fs::path p = "/john";  // IndexedDB-backed folder (IDBFS), mounted and loaded by web/pre.js before main()
#else
  const char* h = std::getenv("JOHN_HOME");
  fs::path p = h && *h ? fs::path(h) : fs::path(homeDir()) / "Documents" / "John";
#endif
  fs::create_directories(p / "songs");
  return p;
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Settings, hints, zoom, light, tempo, countIn, metronome, volume, sound,
                                                gate, latencyMs, tolerance, instrument, whistle, allSongs, language,
                                                solfege, solfegeKeys, harp, allMarks, lyrics)

Settings loadSettings() {
  Settings s;
  try { s = readJson(dataDir() / "settings.json").get<Settings>(); }
  catch (const json::exception&) { return {}; }  // missing, garbage, or a key of the wrong type
  if (!s.sound.empty() && !std::count(SOUNDS.begin(), SOUNDS.end(), s.sound)) s.sound.clear();
  // An unknown instrument id stays (a custom file may be broken or missing for a while): the readers fall back to the
  // D whistle, and only deleteInstrument() resets it (#85).
  if (std::find(HARP_KEYS.begin(), HARP_KEYS.end(), s.harp) == HARP_KEYS.end()) s.harp = "C";
  if (whistleBase(s.whistle) < 0) s.whistle = "D";
  s.zoom = std::clamp(s.zoom, 6.0f, 16.0f);  // values the GUI could choke on
  s.tempo = std::clamp(s.tempo, 40, 220);
  s.volume = std::clamp(s.volume, 0.0f, 1.0f);
  return s;
}

void saveSettings(const Settings& s) { writeJson(dataDir() / "settings.json", json(s)); }

fs::path bundled(const fs::path& rel, const fs::path& built) {
  std::error_code ec;
  fs::path dir;  // the running executable's folder
#if defined(__APPLE__)
  char buf[4096];
  uint32_t n = sizeof buf;
  if (_NSGetExecutablePath(buf, &n) == 0) dir = fs::canonical(buf, ec).parent_path();
#elif defined(__linux__)
  dir = fs::canonical("/proc/self/exe", ec).parent_path();
#elif defined(_WIN32)
  wchar_t buf[32768];
  DWORD n = GetModuleFileNameW(nullptr, buf, 32768);
  if (n && n < 32768) dir = fs::path(buf).parent_path();
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

// A bundled song that gained lyrics after the user installed it: copy them into the user's copy, but only while that
// copy has no lyrics of its own and the same notes up to a transposition (the same lengths in order and one constant
// midi difference, so a song moved with "Play in key" still gets them). A copy whose notes were edited is left alone.
static bool backfillLyrics(Song b, Song& mine) {
  auto notes = [](Song& s) {
    std::vector<Event*> r;
    for (auto& e : s.events) if (e.kind == Kind::Note) r.push_back(&e);
    return r;
  };
  auto hasLyrics = [&](Song& s) {
    if (!s.words.empty()) return true;
    for (auto* e : notes(s)) if (!e->lyric.empty() || e->lyricHeld) return true;
    return false;
  };
  if (!hasLyrics(b) || hasLyrics(mine)) return false;
  auto from = notes(b), to = notes(mine);
  if (from.size() != to.size() || from.empty()) return false;
  for (size_t i = 0; i < from.size(); i++)
    if (from[i]->len != to[i]->len || to[i]->midi - from[i]->midi != to[0]->midi - from[0]->midi) return false;
  for (size_t i = 0; i < from.size(); i++) {
    to[i]->lyric = from[i]->lyric;
    to[i]->lyricHyphen = from[i]->lyricHyphen;
    to[i]->lyricHeld = from[i]->lyricHeld;
  }
  mine.words = b.words;
  return true;
}

// FNV-1a of the song's canonical JSON, so whitespace and key order in the file don't count
static std::string songHash(Song s, const std::string& id) {
  s.id = id;
  uint64_t h = 14695981039346656037ull;
  for (unsigned char c : toJson(s).dump()) h = (h ^ c) * 1099511628211ull;
  char buf[17];
  snprintf(buf, sizeof buf, "%016llx", (unsigned long long)h);
  return buf;
}

// ponytail: smart update by content hash. The marker lists "id<TAB>hash of the bundled version installed". A copy still
// equal to that hash is replaced when the bundle changes; a copy the user changed only gets lyrics (backfillLyrics).
// Old marker lines have no hash: a copy equal to the current bundle gets it, any other copy counts as changed. A fix to
// a bundled song's notes reaches only untouched copies; to push it to everyone, give it a new id.
int installSongs(const fs::path& from) {
  std::error_code ec;
  if (from.empty() || !fs::is_directory(from, ec)) return 0;
  // one marker per bundled folder (lessons-installed.txt, demo-installed.txt), so a song the user deletes stays deleted
  fs::path marker = dataDir() / (from.filename().string() + "-installed.txt");
  std::map<std::string, std::string> done;  // id -> hash ("" on lines from before the hash)
  std::string before;
  {
    std::ifstream f(marker);
    for (std::string l; std::getline(f, l);) {
      before += l + "\n";
      size_t t = l.find('\t');
      if (!l.empty()) done[l.substr(0, t)] = t == std::string::npos ? "" : l.substr(t + 1);
    }
  }
  int n = 0;
  bool wrote = false;
  for (auto& e : fs::directory_iterator(from, ec)) {
    std::string id = e.path().stem().string();
    if (e.path().extension() != ".json") continue;
    json bj = readJson(e.path());
    Song b;
    try { b = fromJson(bj); } catch (const json::exception&) { continue; }
    std::string bh = songHash(b, id);
    fs::path to = dataDir() / "songs" / (id + ".json");
    if (!done.count(id)) {
      if (!fs::exists(to) && fs::copy_file(e.path(), to, ec)) n++, wrote = true;
      done[id] = bh;
      continue;
    }
    auto mine = loadSong(id);
    if (!mine) continue;  // deleted
    std::string mh = songHash(*mine, id), &was = done[id];
    if (mh == bh) {
      was = bh;
    } else if (mh == was) {  // untouched since it was installed: take the new version
      writeJson(to, bj, 1, false);
      was = bh;
      wrote = true;
    } else if (backfillLyrics(b, *mine)) {
      writeJson(to, toJson(*mine), 1, false);
      wrote = true;
    }
  }
  std::string after;
  for (auto& [id, h] : done) after += h.empty() ? id + "\n" : id + "\t" + h + "\n";
  if (after != before) {
    fs::path tmp = marker;
    tmp += ".tmp" + std::to_string(std::random_device{}());  // unique per writer: the GUI and the MCP server both install lessons
    { std::ofstream f(tmp); f << after; }
    fs::rename(tmp, marker);
    wrote = true;
  }
  if (wrote) persist();  // once for the whole folder, not per song
  return n;
}

}  // namespace john
