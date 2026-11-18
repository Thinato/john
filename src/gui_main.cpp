// john: GUI viewer. Shows songs from the data folder as staff notation with a whistle fingering chart and technique
// marks under/over each note. Reloads automatically when the agent (john-mcp) changes a song.
#include "audio.hpp"
#include "john.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <GLFW/emscripten_glfw3.h>
#include <emscripten.h>
#else
#include <future>
#endif
#ifndef GL_BGRA  // Windows' gl.h stops at OpenGL 1.1
#define GL_BGRA 0x80E1
#endif

using namespace john;

static ImFont* gMusic = nullptr;  // Bravura (SMuFL)
static ImFont* gText = nullptr;
static float gMusicScale = 1.0f;  // ImGui sizes fonts by line height; Bravura's is tall, so rescale to 1 em = size

// SMuFL code points
enum : unsigned {
  Brace = 0xE000, GClef = 0xE050, FClef = 0xE062, TimeSig0 = 0xE080, TimeCommon = 0xE08A, TimeCut = 0xE08B,
  HeadWhole = 0xE0A2, HeadHalf = 0xE0A3, HeadBlack = 0xE0A4, Dot = 0xE1E7,
  Flag8Up = 0xE240, Flag8Down = 0xE241,  // +2 per extra flag
  Flat = 0xE260, Natural = 0xE261, Sharp = 0xE262,
  RestWhole = 0xE4E3, RestHalf = 0xE4E4, RestQuarter = 0xE4E5,  // +1 per shorter rest
  BreathComma = 0xE4CE,
};

static std::string utf8(unsigned cp) {
  std::string s;
  if (cp < 0x80) s += char(cp);
  else if (cp < 0x800) { s += char(0xC0 | (cp >> 6)); s += char(0x80 | (cp & 0x3F)); }
  else { s += char(0xE0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 0x3F)); s += char(0x80 | (cp & 0x3F)); }
  return s;
}

// Translated widget label: "###" + the English text keeps the ImGui ID the same in every language ("##" would not).
static std::string L(const char* english) { return std::string(tr(english)) + "###" + english; }

// A whistle or harmonica key ("Bb") spelled like key names ("Sib" with Do Re Mi keys).
static std::string keyLabel(const std::string& k) {
  auto key = parseKey(k);
  return key ? tonicName(*key) : k;
}

// Instrument name in the viewer's language; a whistle's or harp's key is spelled like key names ("Flauta irlandesa em Ré").
static std::string instName(const Instrument& in) {
  std::string key = keyLabel(in.name.substr(0, in.name.find(' ')));  // "Bb whistle" -> Bb
  if (in.id == "whistle") return trf("%s whistle", key.c_str());
  if (in.id == "harmonica" && in.name != "Harmonica") return trf("%s harmonica", key.c_str());  // "A harmonica"; C = "Harmonica"
  return tr(in.name);
}

// Displayed note value: base length (192 whole .. 6 32nd) and dots. Tuplet notes show their written value.
struct Value { int base = 48, dots = 0; };
static Value valueOf(const Event& e) {
  int len = e.tuplet && e.tupletQ ? e.len * e.tuplet / e.tupletQ : e.len;
  for (int b = WHOLE; b >= WHOLE / 32; b /= 2) {
    if (len == b) return {b, 0};
    if (len * 2 == b * 3) return {b, 1};
    if (len * 4 == b * 7) return {b, 2};
  }
  for (int b = WHOLE; b >= WHOLE / 32; b /= 2) if (len >= b) return {b, 0};  // ponytail: odd lengths drawn as the next shorter value, split into ties if it matters
  return {WHOLE / 32, 0};
}
static int beamCount(int base) { return base == 24 ? 1 : base == 12 ? 2 : base == 6 ? 3 : 0; }

// ---------------- app state ----------------
struct App {
  std::vector<std::string> ids;
  struct Meta { fs::file_time_type mtime{}; std::string folder, title; Instrument in; };
  std::map<std::string, Meta> meta;  // song list info, re-read only when a file changes
  std::vector<Technique> techs;
  std::optional<Song> song;
  std::optional<PitchTrack> track;  // the open song's pitch track (Import audio), drawn under each line of music
  std::string current;
  fs::file_time_type songMtime{}, newestSeen{};
  int sel = -1;
  Settings set = loadSettings();  // edited in place (Settings window, sidebar, transport); saved when it changes
  Settings saved = set;
  bool showSettings = false;
  bool english = true;          // the viewer's language is English (set by main() when the language changes)
  bool follow = true;
  bool busy = false;            // Import audio is transcribing on a worker thread (see importWindow)
#ifndef __EMSCRIPTEN__
  bool showImport = false;
  char importPath[1024] = "", importStart[16] = "", importEnd[16] = "", importTempo[16] = "", importMeter[16] = "";
  std::string importInst, importRec, importError;
  std::future<Song> importing;  // valid while transcribing
#endif
  char comment[2048] = "";
  double lastPoll = -10;

  // ---- playback & practice ----
  enum Grade : char { None, Good, Miss, Octave };
  enum Mode : int { Wait, PlayAlong };
  Audio audio;
  std::string practiceFor;      // song id the state below belongs to
  float bpm = 100;
  bool listen = false;
  int mode = Wait;
  std::vector<Grade> grades;    // per event
  std::vector<int> order;       // playOrder of the song
  int cursor = -1, cursorSys = -1;
  int playFrom = 0;             // order index playback started at
  int lastSel = -1;
  int waitK = 0;                // position in `order` while waiting for the player
  double stable = 0;
  bool needRelease = false;
  int dropFrames = 0;
  float held = 0;               // mic level while a note is held, to spot tongued re-articulation
  size_t gradeVi = 0;
  int frames = 0, hits = 0, octs = 0;
  float heard = 0;              // written MIDI heard on the mic (fractional), 0 = nothing
  double silentFor = 0;
};

static fs::path songPath(const std::string& id) { return dataDir() / "songs" / (id + ".json"); }

static void open(App& a, const std::string& id) {
  if (id != a.current) a.sel = -1;
  a.cursor = -1;
  a.song = loadSong(id);
  a.track = a.song ? loadTrack(id) : std::nullopt;
  a.current = a.song ? id : "";
  std::error_code ec;
  a.songMtime = fs::last_write_time(songPath(id), ec);
  if (!a.song || a.sel >= (int)a.song->events.size()) a.sel = -1;
  snprintf(a.comment, sizeof a.comment, "%s", a.sel >= 0 ? songText(*a.song, a.song->events[a.sel].comment).c_str() : "");
}

// Apply a user edit to the selected note on top of the latest file, so concurrent agent edits are not lost.
template <class F> static void editSong(App& a, F f) {
  std::error_code ec;
  if (fs::last_write_time(songPath(a.current), ec) != a.songMtime) open(a, a.current);
  if (!a.song) return;
  f(*a.song);
  try {
    saveSong(*a.song);
  } catch (const std::exception&) {
    return;  // next poll reloads whatever is on disk
  }
  a.songMtime = fs::last_write_time(songPath(a.current), ec);
}

template <class F> static void editSelected(App& a, F f) {
  editSong(a, [&](Song& s) {
    if (a.sel >= 0 && a.sel < (int)s.events.size() && s.events[a.sel].kind == Kind::Note) f(s.events[a.sel]);
  });
}

static void poll(App& a) {
  if (ImGui::GetTime() - a.lastPoll < 0.5) return;
  a.lastPoll = ImGui::GetTime();
  a.ids = listSongs();
  a.techs = loadTechniques();
  std::error_code ec;
  std::string newest;
  fs::file_time_type newestT{};
  for (auto& id : a.ids) {
    auto t = fs::last_write_time(songPath(id), ec);
    if (!ec && t > newestT) { newestT = t; newest = id; }
    auto& m = a.meta[id];
    if (!ec && m.mtime != t) {
      auto s = loadSong(id);
      Instrument in = s ? instrumentOf(*s) : *findInstrument("whistle");
      m = {t, s ? s->folder : "", s && !s->title.empty() ? songText(*s, s->title) : id, in};
    }
  }
  std::erase_if(a.meta, [&](auto& kv) { return std::find(a.ids.begin(), a.ids.end(), kv.first) == a.ids.end(); });
  if (a.follow && !newest.empty() && newestT > a.newestSeen && newest != a.current) open(a, newest);  // follow the agent
  a.newestSeen = std::max(a.newestSeen, newestT);
  if (!a.current.empty()) {
    auto t = fs::last_write_time(songPath(a.current), ec);
    if (ec) { a.song.reset(); a.current.clear(); }
    else if (t != a.songMtime) open(a, a.current);
  }
}

// ---------------- score drawing ----------------
struct Score {
  ImDrawList* dl;
  float sp;
  ImU32 ink = IM_COL32(20, 20, 20, 255);

  void glyph(unsigned cp, float x, float y, ImU32 col) const {  // (x, y) = SMuFL glyph origin (baseline)
    float size = sp * 4 * gMusicScale;  // SMuFL: 1 em = 4 staff spaces
    std::string s = utf8(cp);
    dl->AddText(gMusic, size, ImVec2(x, y - gMusic->GetFontBaked(size)->Ascent), col, s.c_str());
  }
  float glyphW(unsigned cp) const { return gMusic->CalcTextSizeA(sp * 4 * gMusicScale, FLT_MAX, 0, utf8(cp).c_str()).x; }
  void text(const std::string& t, float cx, float top, float size, ImU32 col) const {
    ImVec2 sz = gText->CalcTextSizeA(size, FLT_MAX, 0, t.c_str());
    dl->AddText(gText, size, ImVec2(cx - sz.x / 2, top), col, t.c_str());
  }
  void line(float x0, float y0, float x1, float y1, float w, ImU32 col) const { dl->AddLine({x0, y0}, {x1, y1}, col, w); }
};

struct Slot {      // one event placed on the page
  int ev;
  int sys;
  float x, w;      // slot left and width
  float headX = 0, cx = 0, y = 0;  // note head left, head center x, head y
  int step = 0;
  Value v;
  bool up = true, beamed = false, bass = false;  // bass: on the bass staff of a grand staff
  float stemX = 0, stemEnd = 0;
};

static int beamGroupTicks(const std::string& m) {
  if (compoundMeter(m)) return WHOLE * 3 / 8;
  if (m == "4/4" || m == "C" || m == "2/2" || m == "C|") return WHOLE / 2;  // reels: groups of four eighths
  return WHOLE / 4;
}

// Right end of a volta bracket starting at slot k: the next repeat/double bar or ending, at most 2 bars, same system.
static float x1Of(const std::vector<Slot>& slots, int k, const Song& s) {
  int plain = 0;
  float end = slots[k].x + slots[k].w;
  for (int j = k + 1; j < (int)slots.size() && slots[j].sys == slots[k].sys; j++) {
    end = slots[j].x + slots[j].w;
    const Event& e = s.events[slots[j].ev];
    if (e.kind != Kind::Bar) continue;
    if (e.bar != "|" || !e.ending.empty() || ++plain == 2) return slots[j].x + slots[j].w / 2;
  }
  return end;
}

static void drawScore(App& a, Song& s) {
  const float sp = a.set.zoom;
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x;
  Score sc{ImGui::GetWindowDrawList(), sp};
  if (!gMusic) { ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", tr("Music font missing (Bravura.otf). Re-run cmake to download it.")); return; }

  auto pos = positions(s);
  auto acc = accidentals(s);
  const float headW = sp * 1.18f, accW = sp * 1.3f;
  const float margin = sp * 2;
  const Instrument inst = instrumentOf(s);
  const bool whistle = inst.id == "whistle" && a.set.hints;  // fingering charts are whistle-only, and can be hidden
  const float sysH = sp * (whistle ? 26 : 19);  // a system with only near notes on one staff
  const bool grand = grandStaff(s);
  const float bassDy = grand ? sp * 10 : 0;  // treble top line to bass top line
  const PitchTrack* trk = a.track && a.track->sync.size() >= 2 ? &*a.track : nullptr;
  const float stripH = trk ? sp * 5 : 0;  // pitch track strip at the bottom of each system
  auto slotW = [&](const Event& e, int i) -> float {
    if (e.kind == Kind::Bar) return sp * (e.bar == "|" ? 1.4f : 2.4f);
    return sp * (1.7f + 1.7f * std::sqrt(e.len / 24.0f)) + (acc[i].shown ? accW : 0);
  };

  // ---- line breaking: measures end at bar events; justify full lines ----
  std::vector<std::vector<int>> measures(1);
  for (int i = 0; i < (int)s.events.size(); i++) {
    measures.back().push_back(i);
    if (s.events[i].kind == Kind::Bar) measures.emplace_back();
  }
  if (measures.back().empty()) measures.pop_back();
  int absF = std::abs(s.key.fifths);
  auto headerW = [&](bool first) { return sp * (4.0f + absF * 1.0f + (first ? 2.6f : 0)); };

  std::vector<Slot> slots;
  std::vector<std::pair<int, int>> lines;  // slot ranges per system
  {
    size_t m = 0;
    int sys = 0;
    while (m < measures.size()) {
      float avail = width - 2 * margin - headerW(sys == 0);
      float used = 0;
      size_t end = m;
      while (end < measures.size()) {
        float mw = 0;
        for (int i : measures[end]) mw += slotW(s.events[i], i);
        if (used + mw > avail && end > m) break;
        used += mw;
        end++;
      }
      float noteW = 0, fixedW = 0;
      for (size_t k = m; k < end; k++)
        for (int i : measures[k]) (s.events[i].kind == Kind::Bar ? fixedW : noteW) += slotW(s.events[i], i);
      float stretch = (end < measures.size() && noteW > 0) ? std::max(1.0f, (avail - fixedW) / noteW) : 1.0f;
      float x = origin.x + margin + headerW(sys == 0);
      int first = (int)slots.size();
      for (size_t k = m; k < end; k++)
        for (int i : measures[k]) {
          float w = slotW(s.events[i], i) * (s.events[i].kind == Kind::Bar ? 1.0f : stretch);
          slots.push_back({i, sys, x, w});
          x += w;
        }
      lines.push_back({first, (int)slots.size()});
      m = end;
      sys++;
    }
  }

  // each system grows above and below for its far notes (and by a bass staff on a grand staff)
  std::vector<Room> room;
  std::vector<float> sysY{origin.y};  // system tops, then the end of the last one
  for (auto [f, t] : lines) {
    room.push_back(f < t ? systemRoom(s, slots[f].ev, slots[t - 1].ev + 1, whistle ? 4 : 6) : Room{});
    sysY.push_back(sysY.back() + sysH + bassDy + (room.back().above + room.back().below) * sp / 2 + stripH);
  }
  auto sysHOf = [&](int sys) { return sysY[sys + 1] - sysY[sys]; };
  auto staffTop = [&](int sys) { return sysY[sys] + sp * 8 + room[sys].above * sp / 2; };  // treble staff top line
  auto yOf = [&](int sys, int step, bool bass) { return staffTop(sys) + (bass ? bassDy - sp * 6 : 0) + sp * 4 - (step - 30) * sp / 2; };
  std::vector<int> slotOf(s.events.size(), -1);
  for (int k = 0; k < (int)slots.size(); k++) slotOf[slots[k].ev] = k;

  // ---- per-note geometry ----
  for (auto& sl : slots) {
    const Event& e = s.events[sl.ev];
    if (e.kind == Kind::Bar) continue;
    sl.v = valueOf(e);
    sl.headX = sl.x + (acc[sl.ev].shown ? accW : 0) + sp * 0.3f;
    sl.cx = sl.headX + headW / 2;
    if (e.kind == Kind::Note) {
      sl.step = spell(s.key, e.midi).step();
      sl.bass = onBassStaff(s, e.midi);
      sl.y = yOf(sl.sys, sl.step, sl.bass);
      sl.up = sl.step < (sl.bass ? 22 : 34);  // stems up below the middle line
      sl.stemX = sl.up ? sl.headX + headW - sp * 0.06f : sl.headX + sp * 0.06f;
      sl.stemEnd = sl.y + (sl.up ? -1 : 1) * sp * 3.5f;
    }
  }
  // ---- beam groups ----
  std::vector<std::vector<int>> beams;
  {
    int group = beamGroupTicks(s.meter);
    std::vector<int> cur;
    int curKey = -1;
    auto flush = [&] { if (cur.size() >= 2) beams.push_back(cur); cur.clear(); };
    for (int k = 0; k < (int)slots.size(); k++) {
      const Event& e = s.events[slots[k].ev];
      bool beamable = e.kind == Kind::Note && beamCount(slots[k].v.base) > 0;
      int key = pos[slots[k].ev].bar * 1000 + pos[slots[k].ev].tick / group;
      bool split = !cur.empty() && (slots[cur.back()].sys != slots[k].sys || slots[cur.back()].bass != slots[k].bass);  // no cross-staff beams
      if (!beamable || key != curKey || split) flush();
      if (beamable) { cur.push_back(k); curKey = key; }
    }
    flush();
    for (auto& g : beams) {
      int sum = 0;
      for (int k : g) sum += slots[k].step - (slots[k].bass ? 22 : 34);
      bool up = sum < 0;
      float end = up ? FLT_MAX : -FLT_MAX;
      for (int k : g) end = up ? std::min(end, slots[k].y - sp * 3.5f) : std::max(end, slots[k].y + sp * 3.5f);
      for (int k : g) {
        Slot& sl = slots[k];
        sl.up = up;
        sl.beamed = true;
        sl.stemX = up ? sl.headX + headW - sp * 0.06f : sl.headX + sp * 0.06f;
        sl.stemEnd = end;
      }
    }
  }

  // ---- paper, hover ----
  float totalH = sysY.back() - origin.y + sp * 2;
  sc.dl->AddRectFilled(origin, {origin.x + width, origin.y + totalH}, IM_COL32(252, 250, 244, 255), 4);
  ImVec2 mouse = ImGui::GetIO().MousePos;
  int hovered = -1;
  for (auto& sl : slots) {
    if (s.events[sl.ev].kind != Kind::Note) continue;
    float top = sysY[sl.sys], h = sysHOf(sl.sys) - stripH;
    if (mouse.x >= sl.x && mouse.x < sl.x + sl.w && mouse.y >= top && mouse.y < top + h && ImGui::IsWindowHovered())
      hovered = sl.ev;
    if (sl.ev == a.sel || sl.ev == hovered)
      sc.dl->AddRectFilled({sl.x, top + sp}, {sl.x + sl.w, top + h - sp},
                           sl.ev == a.sel ? IM_COL32(120, 170, 255, 70) : IM_COL32(0, 0, 0, 18), 3);
    if (sl.ev == a.cursor) {
      sc.dl->AddRectFilled({sl.x, top + sp}, {sl.x + sl.w, top + h - sp}, IM_COL32(60, 200, 90, 70), 3);
      if (sl.sys != a.cursorSys) {  // keep the cursor's line in view, without fighting manual scrolling
        a.cursorSys = sl.sys;
        float contentY = top - (ImGui::GetWindowPos().y - ImGui::GetScrollY());
        if (contentY < ImGui::GetScrollY() || contentY + h > ImGui::GetScrollY() + ImGui::GetWindowHeight())
          ImGui::SetScrollY(std::max(0.0f, contentY - sp * 2));
      }
    }
  }
  if (hovered >= 0 && ImGui::IsMouseClicked(0)) {
    a.sel = hovered;
    snprintf(a.comment, sizeof a.comment, "%s", songText(s, s.events[hovered].comment).c_str());
  }

  const ImU32 ink = sc.ink, red = IM_COL32(200, 40, 40, 255), techCol = IM_COL32(205, 95, 0, 255);
  const float thin = std::max(1.0f, sp * 0.12f);

  // ---- systems: staff, clef, key, time ----
  for (int sys = 0; sys < (int)lines.size(); sys++) {
    if (lines[sys].first == lines[sys].second) continue;
    const Slot& last = slots[lines[sys].second - 1];
    float x0 = origin.x + margin, x1 = last.x + last.w;
    if (grand) {  // brace (1 em = its height) and the line joining the staves
      float h = bassDy + sp * 4, size = h * gMusicScale, top = staffTop(sys);
      sc.dl->AddText(gMusic, size, ImVec2(x0 - sp * 1.5f, top + h - gMusic->GetFontBaked(size)->Ascent), ink, utf8(Brace).c_str());
      sc.line(x0, top, x0, top + h, thin, ink);
    }
    for (int st = 0; st < (grand ? 2 : 1); st++) {  // treble staff, then the bass staff of a grand staff
      const bool bass = st == 1;
      const float top = staffTop(sys) + (bass ? bassDy : 0);
      const int down = bass ? 14 : 0;  // bass clef: key signature two octaves lower
      for (int l = 0; l < 5; l++) sc.line(x0, top + l * sp, x1, top + l * sp, thin, ink);
      sc.glyph(bass ? FClef : GClef, x0 + sp * 0.5f, yOf(sys, bass ? 24 : 32, bass), ink);
      static const int SHARP_STEPS[7] = {38, 35, 39, 36, 33, 37, 34}, FLAT_STEPS[7] = {34, 37, 33, 36, 32, 35, 31};
      float kx = x0 + sp * 3.4f;
      for (int i = 0; i < absF; i++, kx += sp)
        sc.glyph(s.key.fifths > 0 ? Sharp : Flat, kx, yOf(sys, (s.key.fifths > 0 ? SHARP_STEPS[i] : FLAT_STEPS[i]) - down, bass), ink);
      if (sys == 0) {
        float tx = kx + sp * 0.4f;
        int n = 0, d = 0;
        if (s.meter == "C") sc.glyph(TimeCommon, tx, top + sp * 2, ink);
        else if (s.meter == "C|") sc.glyph(TimeCut, tx, top + sp * 2, ink);
        else if (sscanf(s.meter.c_str(), "%d/%d", &n, &d) == 2) {
          auto digits = [&](int v, float y) {
            std::string t = std::to_string(v);
            float w = 0;
            for (char c : t) w += sc.glyphW(TimeSig0 + (c - '0'));
            float x = tx + sp * 0.9f - w / 2;
            for (char c : t) { sc.glyph(TimeSig0 + (c - '0'), x, y, ink); x += sc.glyphW(TimeSig0 + (c - '0')); }
          };
          digits(n, top + sp);
          digits(d, top + sp * 3);
        }
      }
    }
  }

  // ---- events ----
  for (int k = 0; k < (int)slots.size(); k++) {
    Slot& sl = slots[k];
    const Event& e = s.events[sl.ev];
    float top = staffTop(sl.sys), bottom = top + bassDy + sp * 4;  // bottom: lowest staff's bottom line
    if (e.kind == Kind::Bar) {
      float x = sl.x + sl.w / 2;
      auto dots = [&](float dx) {
        for (int st = 0; st < (grand ? 2 : 1); st++) {  // on each staff
          float t = top + st * bassDy;
          sc.dl->AddCircleFilled({x + dx, t + sp * 1.5f}, sp * 0.22f, ink);
          sc.dl->AddCircleFilled({x + dx, t + sp * 2.5f}, sp * 0.22f, ink);
        }
      };
      if (e.bar == "|") sc.line(x, top, x, bottom, thin, ink);
      else if (e.bar == "||") { sc.line(x - sp * 0.25f, top, x - sp * 0.25f, bottom, thin, ink); sc.line(x + sp * 0.25f, top, x + sp * 0.25f, bottom, thin, ink); }
      else if (e.bar == "|]") { sc.line(x - sp * 0.3f, top, x - sp * 0.3f, bottom, thin, ink); sc.line(x + sp * 0.2f, top, x + sp * 0.2f, bottom, sp * 0.5f, ink); }
      else if (e.bar == "|:") { sc.line(x - sp * 0.4f, top, x - sp * 0.4f, bottom, sp * 0.5f, ink); sc.line(x + sp * 0.1f, top, x + sp * 0.1f, bottom, thin, ink); dots(sp * 0.6f); }
      else if (e.bar == ":|") { dots(-sp * 0.6f); sc.line(x - sp * 0.1f, top, x - sp * 0.1f, bottom, thin, ink); sc.line(x + sp * 0.4f, top, x + sp * 0.4f, bottom, sp * 0.5f, ink); }
      else { dots(-sp * 0.8f); sc.line(x - sp * 0.2f, top, x - sp * 0.2f, bottom, thin, ink); sc.line(x + sp * 0.2f, top, x + sp * 0.2f, bottom, thin, ink); dots(sp * 0.8f); }
      if (!e.ending.empty()) {  // volta bracket up to the next structural bar
        int vk = k + 1 < (int)slots.size() && slots[k + 1].sys != sl.sys ? k + 1 : k;
        if (vk != k) x = slots[vk].x;
        float ex = x1Of(slots, vk, s);
        float by = sysY[slots[vk].sys] + sp * 1.2f;
        sc.line(x, by + sp * 1.5f, x, by, thin, ink);
        sc.line(x, by, ex, by, thin, ink);
        sc.text(e.ending + ".", x + sp * 1.0f, by + sp * 0.1f, sp * 1.5f, ink);
      }
      continue;
    }
    if (e.kind == Kind::Rest) {
      int b = sl.v.base;
      bool fullBar = e.len == meterTicks(s.meter);
      unsigned g = fullBar || b == WHOLE ? RestWhole : b == WHOLE / 2 ? RestHalf : RestQuarter + (b == 24 ? 1 : b == 12 ? 2 : b == 6 ? 3 : 0);
      float y = g == RestWhole ? top + sp : top + sp * 2;
      sc.glyph(g, sl.x + sp * 0.4f, y, ink);
      for (int d = 0; d < sl.v.dots && !fullBar; d++) sc.glyph(Dot, sl.x + sp * (2.0f + d * 0.6f), top + sp * 1.5f, ink);
      continue;
    }
    // note
    Fingering f = whistle ? finger(s.whistle, e.midi) : Fingering{"", 0, inst.plays(e.midi)};
    App::Grade g = sl.ev < (int)a.grades.size() ? a.grades[sl.ev] : App::None;
    ImU32 col = !f.ok ? red
                : g == App::Good ? IM_COL32(30, 150, 60, 255)
                : g == App::Miss ? IM_COL32(210, 40, 40, 255)
                : g == App::Octave ? IM_COL32(225, 140, 0, 255)
                : sl.ev == a.sel ? IM_COL32(30, 90, 220, 255) : ink;
    float lx0 = sl.headX - sp * 0.45f, lx1 = sl.headX + headW + sp * 0.45f;
    const int b0 = sl.bass ? 18 : 30;  // bottom line of the note's staff
    for (int L = b0 - 2; L >= sl.step; L -= 2) sc.line(lx0, yOf(sl.sys, L, sl.bass), lx1, yOf(sl.sys, L, sl.bass), thin, ink);
    for (int L = b0 + 10; L <= sl.step; L += 2) sc.line(lx0, yOf(sl.sys, L, sl.bass), lx1, yOf(sl.sys, L, sl.bass), thin, ink);
    if (acc[sl.ev].shown)
      sc.glyph(acc[sl.ev].alter > 0 ? Sharp : acc[sl.ev].alter < 0 ? Flat : Natural, sl.headX - sp * 1.2f, sl.y, col);
    sc.glyph(sl.v.base >= WHOLE ? HeadWhole : sl.v.base == WHOLE / 2 ? HeadHalf : HeadBlack, sl.headX, sl.y, col);
    if (sl.v.base < WHOLE) {
      sc.line(sl.stemX, sl.y, sl.stemX, sl.stemEnd, sp * 0.12f, col);
      int nb = beamCount(sl.v.base);
      if (nb && !sl.beamed) sc.glyph((sl.up ? Flag8Up : Flag8Down) + 2 * (nb - 1), sl.stemX - sp * 0.06f, sl.stemEnd, col);
    }
    float dotY = sl.step % 2 == 0 ? sl.y - sp / 2 : sl.y;
    for (int d = 0; d < sl.v.dots; d++) sc.glyph(Dot, sl.headX + headW + sp * (0.35f + d * 0.6f), dotY, col);
    if (e.tie) {  // arc to the next note (or to the end of the system)
      int nk = k + 1;
      while (nk < (int)slots.size() && s.events[slots[nk].ev].kind != Kind::Note) nk++;
      float xa = sl.headX + headW, xb = nk < (int)slots.size() && slots[nk].sys == sl.sys ? slots[nk].headX : slots[lines[sl.sys].second - 1].x + slots[lines[sl.sys].second - 1].w;
      float dir = sl.up ? 1.0f : -1.0f, ya = sl.y + dir * sp * 0.7f;
      sc.dl->AddBezierCubic({xa, ya}, {xa + (xb - xa) * 0.3f, ya + dir * sp}, {xb - (xb - xa) * 0.3f, ya + dir * sp}, {xb, ya}, ink, sp * 0.15f);
    }
    if (e.tupletStart) {
      float x0 = sl.cx, x1 = sl.cx, yTop = FLT_MAX;
      for (int j = k, n = 0; j < (int)slots.size() && n < e.tuplet; j++) {
        if (s.events[slots[j].ev].kind == Kind::Bar) continue;
        x1 = slots[j].cx;
        float t = s.events[slots[j].ev].kind == Kind::Note ? std::min(slots[j].y - sp, slots[j].up ? slots[j].stemEnd : slots[j].y) : top;
        yTop = std::min(yTop, t);
        n++;
      }
      sc.text(std::to_string(e.tuplet), (x0 + x1) / 2, std::min(yTop, top) - sp * 2.0f, sp * 1.5f, ink);
    }
    // techniques above the note, breath mark after it
    float tTop = std::min({top - sp * 0.8f, sl.y - sp, sl.up ? sl.stemEnd : sl.y}) - sp * 0.4f;
    for (auto& t : e.techniques) {
      if (t == "breath") { sc.glyph(BreathComma, sl.x + sl.w - sp * 0.6f, top - sp * 0.5f, techCol); continue; }
      auto it = std::find_if(a.techs.begin(), a.techs.end(), [&](auto& x) { return x.id == t; });
      std::string sym = it != a.techs.end() ? it->symbol : t;
      tTop -= sp * 2.1f;
      sc.text(sym, sl.cx, tTop, sp * 2.2f, techCol);
    }
    // fingering chart: six holes, top hand first
    float chartTop = bottom + sp * 3.5f + room[sl.sys].below * sp / 2, r = sp * 0.42f;  // below the lowest note
    if (whistle && f.ok) {
      for (int h = 0; h < 6; h++) {
        ImVec2 c{sl.cx, chartTop + sp * (0.5f + h * 1.05f + (h >= 3 ? 0.5f : 0))};
        if (f.holes[h] == 'X') sc.dl->AddCircleFilled(c, r, ink);
        else if (f.holes[h] == 'H') {
          sc.dl->PathArcTo(c, r, 1.5707963f, 4.712389f);
          sc.dl->PathFillConvex(ink);
        }
        sc.dl->AddCircle(c, r, ink, 0, std::max(1.0f, sp * 0.1f));
      }
      if (f.octave) sc.text(std::string(f.octave, '+'), sl.cx, chartTop + sp * 6.9f, sp * 1.6f, ink);
    } else if (whistle) sc.text("?", sl.cx, chartTop + sp * 2, sp * 2, red);
    std::string nm = noteName(spell(s.key, e.midi));
    nm.pop_back();
    sc.text(nm, sl.cx, chartTop + sp * (whistle ? 8.4f : 1.0f), sp * 1.4f, IM_COL32(110, 110, 110, 255));
  }

  // ---- pitch track (Import audio): the recording's melody under each line, placed through its beat times ----
  bool stripHovered = false;
  if (trk) {
    std::vector<int> at(s.events.size() + 1, 0);  // tick each event starts at, counted like PitchTrack::sync
    for (size_t i = 0; i < s.events.size(); i++) at[i + 1] = at[i] + (s.events[i].kind == Kind::Bar ? 0 : s.events[i].len);
    int lo = 127, hi = 0;
    for (auto& p : trk->points) if (p.midi) lo = std::min(lo, p.midi), hi = std::max(hi, p.midi);
    if (hi - lo < 12) { lo = (lo + hi) / 2 - 6; hi = lo + 12; }
    auto stripTop = [&](int sys) { return sysY[sys + 1] - stripH; };
    for (int sys = 0; sys < (int)lines.size(); sys++) {
      if (lines[sys].first == lines[sys].second) continue;
      const Slot& last = slots[lines[sys].second - 1];
      ImVec2 p0{origin.x + margin, stripTop(sys)}, p1{last.x + last.w, sysY[sys + 1] - sp};
      sc.dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 12), 2);
      stripHovered |= ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(p0, p1);
    }
    size_t k = 0;  // slot of the event under the current point
    float px = 0, py = 0, pconf = 0;
    int psys = -1;  // line of the previous point, -1 = it had no pitch
    for (auto& p : trk->points) {
      double tick = trackTick(*trk, p.time);
      while (k < slots.size() && (s.events[slots[k].ev].kind == Kind::Bar || at[slots[k].ev + 1] <= tick)) k++;
      if (k == slots.size()) break;
      const Slot& sl = slots[k];
      if (tick < at[sl.ev]) continue;  // before the first note
      size_t j = k + 1;  // from this note's head to the next note's or rest's, or to the line's end
      while (j < slots.size() && slots[j].sys == sl.sys && s.events[slots[j].ev].kind == Kind::Bar) j++;
      const Slot& end = slots[lines[sl.sys].second - 1];
      float xb = j < slots.size() && slots[j].sys == sl.sys ? slots[j].cx : end.x + end.w;
      float x = sl.cx + (float)((tick - at[sl.ev]) / std::max(1, at[sl.ev + 1] - at[sl.ev])) * (xb - sl.cx);
      if (psys == sl.sys) sc.line(px, py, x, py, sp * 0.35f, IM_COL32(40, 110, 200, pconf >= 0.35f ? 255 : 60));  // faint: unclear
      psys = p.midi ? sl.sys : -1;
      px = x;
      py = stripTop(sl.sys) + (stripH - sp) * (1 - (float)(p.midi - lo) / (hi - lo));
      pconf = p.conf;
    }
  }

  // ---- beams ----
  for (auto& g : beams) {
    const Slot& f0 = slots[g.front()];
    float th = sp * 0.5f, gap = sp * 0.75f, dir = f0.up ? 1.0f : -1.0f;
    for (int level = 0; level < 3; level++) {
      float y = f0.stemEnd + dir * level * gap;
      for (size_t i = 0; i < g.size(); i++) {
        const Slot& sl = slots[g[i]];
        if (beamCount(sl.v.base) <= level) continue;
        bool nextToo = i + 1 < g.size() && beamCount(slots[g[i + 1]].v.base) > level;
        bool prevToo = i > 0 && beamCount(slots[g[i - 1]].v.base) > level;
        float xa = sl.stemX - sp * 0.06f, xb;
        if (nextToo) xb = slots[g[i + 1]].stemX + sp * 0.06f;
        else if (prevToo) continue;
        else xb = i + 1 < g.size() ? xa + sp * 1.1f : xa - sp * 1.1f;  // stub
        ImVec2 p0{std::min(xa, xb), std::min(y, y + dir * th)}, p1{std::max(xa, xb), std::max(y, y + dir * th)};
        sc.dl->AddRectFilled(p0, p1, sl.ev == a.sel ? IM_COL32(30, 90, 220, 255) : ink);
      }
    }
  }

  ImGui::Dummy({width, totalH});
  if (stripHovered) ImGui::SetTooltip("%s", tr("Pitch heard in the recording (faint where it is unclear)"));

  if (hovered >= 0) {
    const Event& e = s.events[hovered];
    ImGui::BeginTooltip();
    ImGui::Text("%s  %s", noteName(spell(s.key, e.midi)).c_str(), durationName(e).c_str());
    if (whistle) ImGui::Text(tr("fingering: %s"), fingeringText(finger(s.whistle, e.midi)).c_str());
    else if (!inst.plays(e.midi)) ImGui::TextColored({1, 0.4f, 0.4f, 1}, tr("out of range for the %s"), instName(inst).c_str());
    for (auto& t : e.techniques) {
      auto it = std::find_if(a.techs.begin(), a.techs.end(), [&](auto& x) { return x.id == t; });
      ImGui::BulletText("%s", it != a.techs.end() ? it->name.c_str() : t.c_str());
    }
    if (!e.comment.empty()) {
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25);
      ImGui::TextWrapped("%s", songText(s, e.comment).c_str());
      ImGui::PopTextWrapPos();
    }
    ImGui::EndTooltip();
  }
}

// ---------------- playback & practice ----------------
static bool tiedFromPrev(const App& a, int k) {  // note k only continues a tie, it is not played again
  if (k <= 0 || k >= (int)a.order.size()) return false;
  const Event& p = a.song->events[a.order[k - 1]];
  const Event& e = a.song->events[a.order[k]];
  return p.kind == Kind::Note && p.tie && e.kind == Kind::Note && p.midi == e.midi;
}

static int firstNoteFrom(const App& a, int k) {  // next note the player must actually play
  while (k < (int)a.order.size()) {
    const Event& e = a.song->events[a.order[k]];
    if (e.kind == Kind::Note && !tiedFromPrev(a, k) && instrumentOf(*a.song).plays(e.midi)) break;
    k++;
  }
  return k;
}

static void gradeWithTies(App& a, int k, App::Grade g) {  // grade a note and the notes tied onto it
  do a.grades[a.order[k]] = g;
  while (++k < (int)a.order.size() && tiedFromPrev(a, k));
}

static int startOrder(const App& a) {  // the selected note's first position in performance order
  for (int k = 0; k < (int)a.order.size(); k++) if (a.order[k] == a.sel) return k;
  return 0;
}

static void resetPractice(App& a) {
  a.grades.assign(a.song->events.size(), App::None);
  a.waitK = firstNoteFrom(a, startOrder(a));
  a.stable = 0;
  a.needRelease = false;
}

static void startPlayback(App& a, int fromOrder, bool countIn) {
  a.playFrom = fromOrder;
  a.gradeVi = 0;
  a.frames = a.hits = a.octs = 0;
  a.audio.play(perform(*a.song, a.bpm, fromOrder, countIn, a.set.sound));
}

static void restartHere(App& a) {  // restart playback at the current note, e.g. with a new tempo or sound
  double t = a.audio.playTime();
  int from = a.playFrom;
  for (auto& v : a.audio.performance().voices) if (v.start <= t) from = v.ord;
  startPlayback(a, from, false);
}

static void practiceUpdate(App& a, float dt) {
  if (!a.song) { a.audio.stop(); a.practiceFor.clear(); return; }
  auto order = playOrder(*a.song);
  if (a.practiceFor != a.current || order != a.order || a.grades.size() != a.song->events.size()) {  // new or edited song
    a.audio.stop();
    if (a.practiceFor != a.current) a.bpm = a.song->tempo ? (float)a.song->tempo : (float)a.set.tempo;
    a.practiceFor = a.current;
    a.order = std::move(order);
    a.cursor = -1;
    resetPractice(a);
  }
  if (a.sel != a.lastSel) {  // clicking a note moves the "wait for me" cursor there
    a.lastSel = a.sel;
    if (a.mode == App::Wait && a.sel >= 0) { a.waitK = firstNoteFrom(a, startOrder(a)); a.needRelease = false; }
  }

  // microphone -> heard pitch
  a.heard = 0;
  float recent = 0;  // level of the last ~10 ms, to catch tongued gaps shorter than the pitch window
  if (a.listen) {
    static std::vector<float> buf(2048);
    int n = a.audio.latest(buf.data(), (int)buf.size());
    float level = n ? rms(buf.data(), n) : 0;
    recent = n ? rms(buf.data() + n - 512, 512) : 0;
    a.silentFor = level > 1e-5f ? 0 : a.silentFor + dt;
    Instrument in = instrumentOf(*a.song);
    float fmin = (float)soundingFreq(in.low, in.octave) * 0.8f, fmax = (float)soundingFreq(in.high, in.octave) * 1.15f;
    if (level > a.set.gate)
      if (float hz = detectPitch(buf.data(), n, a.audio.micRate(), fmin, fmax)) a.heard = (float)writtenMidiFromFreq(hz, in.octave);
  }
  auto cents = [&](int midi) { return (a.heard - midi) * 100; };

  int prevCursor = a.cursor;
  a.cursor = -1;
  if (a.audio.playing()) {
    const Performance& p = a.audio.performance();
    double t = a.audio.playTime();
    for (auto& v : p.voices) if (t >= v.start && t < v.start + v.dur) a.cursor = v.ev;
    if (a.listen && a.mode == App::PlayAlong) {  // grade each note once its time has passed
      double tl = t - a.set.latencyMs / 1000.0;
      while (a.gradeVi < p.voices.size() && tl >= p.voices[a.gradeVi].start + p.voices[a.gradeVi].dur) {
        const Voice& v = p.voices[a.gradeVi];
        if (a.frames > 0 && v.ord >= 0 && v.ord < (int)a.order.size())
          gradeWithTies(a, v.ord, a.hits * 2 >= a.frames ? App::Good : a.octs * 2 >= a.frames ? App::Octave : App::Miss);
        a.frames = a.hits = a.octs = 0;
        a.gradeVi++;
      }
      if (a.gradeVi < p.voices.size()) {
        const Voice& v = p.voices[a.gradeVi];
        if (tl >= v.start + v.dur * 0.25 && tl < v.start + v.dur - v.gap) {  // skip the attack and graces
          a.frames++;
          if (a.heard > 0 && std::abs(cents(v.midi)) <= a.set.tolerance) a.hits++;
          else if (a.heard > 0 && std::abs(std::abs(cents(v.midi)) - 1200) <= a.set.tolerance) a.octs++;
        }
      }
    }
  } else {
    if (a.audio.performance().length > 0) a.audio.stop();  // finished
    if (a.listen && a.mode == App::Wait && a.waitK < (int)a.order.size()) {
      const Event& e = a.song->events[a.order[a.waitK]];
      a.cursor = a.order[a.waitK];
      bool right = a.heard > 0 && std::abs(cents(e.midi)) <= a.set.tolerance;
      // re-articulation for repeated notes: a cut (pitch jump), a tongued dip in level, or a real gap (2+ frames)
      a.dropFrames = a.heard == 0 ? a.dropFrames + 1 : 0;
      if ((a.heard > 0 && std::abs(cents(e.midi)) > 100) || (a.held > 0 && recent < a.held * 0.4f) || a.dropFrames >= 2)
        a.needRelease = false;
      if (a.heard > 0) a.held += (recent - a.held) * 0.2f;
      if (right && !a.needRelease) {
        a.stable += dt;
        if (a.stable >= 0.08) {
          gradeWithTies(a, a.waitK, App::Good);
          a.waitK = firstNoteFrom(a, a.waitK + 1);
          a.needRelease = a.waitK < (int)a.order.size() && a.song->events[a.order[a.waitK]].midi == e.midi;
          a.stable = 0;
        }
      } else a.stable = 0;
    }
  }
  if (a.cursor < 0 && prevCursor >= 0) a.cursorSys = -1;
}

static void transportBar(App& a) {
  bool playing = a.audio.playing();
  bool space = ImGui::IsKeyPressed(ImGuiKey_Space, false) && !ImGui::GetIO().WantTextInput;
  if (ImGui::Button(("  " + std::string(playing ? tr("Stop") : tr("Play")) + "  ###play").c_str()) || space) {
    if (playing) a.audio.stop();
    else startPlayback(a, startOrder(a), a.set.countIn);
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", a.set.countIn ? tr("Space. Starts from the selected note, after a one-bar count-in.") : tr("Space. Starts from the selected note."));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(170);
  ImGui::SliderFloat("##bpm", &a.bpm, 40, 220, tr("tempo %.0f"));
  if (ImGui::IsItemDeactivatedAfterEdit() && playing) restartHere(a);  // with the new tempo
  if (ImGui::IsItemHovered()) ImGui::SetTooltip(tr("Quarter notes per minute (song tempo: %s)"), a.song->tempo ? std::to_string(a.song->tempo).c_str() : tr("not set"));
  ImGui::SameLine();
  auto soundName = [&](const std::string& id) { return instName(findInstrument(id, a.song->whistle).value_or(*findInstrument(id))); };
  std::string songSound = trf("%s (song)", instName(instrumentOf(*a.song)).c_str());
  std::string soundLabel = trf("sound: %s", (a.set.sound.empty() ? songSound : soundName(a.set.sound)).c_str());
  ImGui::SetNextItemWidth(std::max(170.0f, ImGui::CalcTextSize(soundLabel.c_str()).x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2));  // longer in Portuguese
  if (ImGui::BeginCombo("##sound", soundLabel.c_str())) {
    if (ImGui::Selectable(songSound.c_str(), a.set.sound.empty())) { a.set.sound.clear(); if (playing) restartHere(a); }
    for (auto& id : INSTRUMENTS)
      if (ImGui::Selectable(soundName(id).c_str(), a.set.sound == id)) { a.set.sound = id; if (playing) restartHere(a); }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Which instrument plays the tune (saved, also in Settings). The notes keep the song's pitch."));
  ImGui::SameLine();
  bool melody = a.audio.melody;
  if (ImGui::Checkbox(L("Melody").c_str(), &melody)) a.audio.melody = melody;
  ImGui::SameLine();
  ImGui::Checkbox(L("Metronome").c_str(), &a.set.metronome);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90);
  ImGui::SliderFloat("##vol", &a.set.volume, 0, 1, tr("volume %.1f"));

  // microphone row
  if (ImGui::Button((std::string(a.listen ? tr("Mic: on") : tr("Mic: off")) + "###mic").c_str())) {
    a.listen = !a.listen;
    if (a.listen && !a.audio.startMic()) a.listen = false;
    if (!a.listen) a.audio.stopMic();
    a.silentFor = 0;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(L("Wait for me").c_str(), a.mode == App::Wait)) { a.mode = App::Wait; resetPractice(a); }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The cursor waits on each note until you play it in tune."));
  ImGui::SameLine();
  if (ImGui::RadioButton(L("Play along").c_str(), a.mode == App::PlayAlong)) a.mode = App::PlayAlong;
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Press Play and play with the backing. Notes turn green (right), red (wrong) or orange (wrong octave).\nUse headphones, or untick Melody and follow the metronome, so the mic doesn't hear the backing."));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(110);
  ImGui::SliderFloat("##tol", &a.set.tolerance, 10, 100, tr("tolerance %.0fc"));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("How far from the note (in cents, 100 = a semitone) still counts as right."));
  ImGui::SameLine();
  if (ImGui::Button(L("Reset marks").c_str())) resetPractice(a);
  if (ImGui::BeginPopupContextItem("calib")) {
    ImGui::SliderFloat(L("mic gate").c_str(), &a.set.gate, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat(L("latency ms").c_str(), &a.set.latencyMs, 0, 300, "%.0f");
    ImGui::EndPopup();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Right-click for mic calibration (noise gate, latency; also in Settings)."));

  if (a.listen) {
    ImGui::SameLine();
    if (a.heard > 0) {
      int m = (int)std::lround(a.heard);
      float c = (a.heard - m) * 100;
      ImVec4 col = std::abs(c) <= a.set.tolerance ? ImVec4(0.3f, 0.9f, 0.4f, 1) : ImVec4(1, 0.7f, 0.2f, 1);
      bool whistle = instrumentOf(*a.song).id == "whistle" && a.set.hints;
      ImGui::TextColored(col, tr("hearing %s %+.0fc  %s"), noteName(spell(a.song->key, m)).c_str(), c,
                         whistle ? fingeringText(finger(a.song->whistle, m)).c_str() : "");
      if (a.cursor >= 0 && a.cursor < (int)a.song->events.size()) {
        int want = a.song->events[a.cursor].midi;
        if (m == want + 12) { ImGui::SameLine(); ImGui::TextColored({1, 0.7f, 0.2f, 1}, "%s", whistle ? tr("(octave too high: blow softer)") : tr("(octave too high)")); }
        if (m == want - 12) { ImGui::SameLine(); ImGui::TextColored({1, 0.7f, 0.2f, 1}, "%s", whistle ? tr("(octave too low: blow harder)") : tr("(octave too low)")); }
      }
    } else if (a.silentFor > 2) ImGui::TextColored({1, 0.5f, 0.4f, 1}, "%s", tr("no sound from the mic: allow microphone access for this app/terminal in System Settings > Privacy & Security"));
    else ImGui::TextDisabled("%s", tr("listening..."));
  }
  int good = 0, total = 0;
  for (size_t i = 0; i < a.grades.size(); i++) { good += a.grades[i] == App::Good; total += a.grades[i] != App::None; }
  if (a.listen && a.mode == App::Wait && a.waitK >= (int)a.order.size()) { ImGui::SameLine(); ImGui::TextColored({0.3f, 0.9f, 0.4f, 1}, "%s", tr("Done! Well played.")); }
  else if (total) { ImGui::SameLine(); ImGui::TextDisabled(tr("%d/%d right"), good, total); }
  if (!a.audio.error.empty()) ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", a.audio.error.c_str());
}

// ---------------- panels ----------------
static void detailsPanel(App& a) {
  Song& s = *a.song;
  ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.4f);
  ImGui::TextWrapped("%s", songText(s, s.title).c_str());
  ImGui::PopFont();
  const Instrument inst = instrumentOf(s);
  const bool whistle = inst.id == "whistle" && a.set.hints;
  ImGui::TextDisabled("%s  |  %s  |  %s%s", keyName(s.key).c_str(), s.meter.c_str(), instName(inst).c_str(),
                      s.tempo ? ("  |  " + std::to_string(s.tempo) + " bpm").c_str() : "");
  if (!s.source.empty()) ImGui::TextWrapped(tr("Source: %s"), s.source.c_str());
  std::vector<std::string> choices = FOLDERS;
  if (!s.folder.empty() && std::find(choices.begin(), choices.end(), s.folder) == choices.end()) choices.push_back(s.folder);
  choices.push_back("");
  bool edited = false;  // editSong may reload the song or empty a.song: don't use `s` after an edit this frame
  ImGui::SetNextItemWidth(200);
  if (ImGui::BeginCombo(L("Folder").c_str(), s.folder.empty() ? tr("Unsorted") : tr(s.folder.c_str()))) {
    for (auto& f : choices)
      if (ImGui::Selectable(f.empty() ? tr("Unsorted") : tr(f.c_str()), f == s.folder)) {
        std::string pick = f;
        editSong(a, [&](Song& x) { x.folder = pick; });
        edited = true;
        break;
      }
    ImGui::EndCombo();
  }
  if (edited) return;  // redraw from the fresh song next frame
  ImGui::SetNextItemWidth(200);
  if (ImGui::BeginCombo(L("Instrument").c_str(), instName(inst).c_str())) {
    for (auto& id : INSTRUMENTS) {
      std::string pick = id;
      std::string harp = id == "harmonica" && inst.id != "harmonica" ? a.set.harp : s.harp;  // a new harmonica song: Settings' harp
      if (ImGui::Selectable(instName(findInstrument(id, s.whistle, harp).value_or(*findInstrument(id))).c_str(), id == inst.id)) {
        editSong(a, [&](Song& x) {
          if (pick == "harmonica" && x.instrument != "harmonica") x.harp = harp;
          x.instrument = pick;
        });
        edited = true;
        break;
      }
    }
    ImGui::EndCombo();
  }
  if (edited) return;
  if (inst.id == "harmonica") {
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo(L("Harmonica key").c_str(), keyLabel(s.harp).c_str())) {
      for (auto& k : HARP_KEYS)
        if (ImGui::Selectable((keyLabel(k) + "###" + k).c_str(), k == s.harp)) {
          std::string pick = k;
          editSong(a, [&](Song& x) { x.harp = pick; });
          edited = true;
          break;
        }
      ImGui::EndCombo();
    }
    if (edited) return;
  }
  // Play in key: the 12 tonics in the song's mode; the octave follows the instrument (transposeToKey)
  const int cur = tonicPc(s.key), orig = ((cur - s.transposed) % 12 + 12) % 12;
  ImGui::SetNextItemWidth(200);
  if (ImGui::BeginCombo(L("Play in key").c_str(), keyName(s.key).c_str(), ImGuiComboFlags_HeightLargest)) {  // all 12, no scrolling
    for (int pc = 0; pc < 12; pc++) {
      Song k;
      k.key = s.key;
      transpose(k, pc - cur);  // no notes: only the key
      std::string name = pc == orig ? trf("%s (original)", keyName(k.key).c_str()) : keyName(k.key);
      if (ImGui::Selectable((name + "###key" + std::to_string(pc)).c_str(), pc == cur)) {
        const int tonic = pc;
        if (tonic != cur) editSong(a, [&](Song& x) { transposeToKey(x, tonic, instrumentOf(x)); });
        edited = true;
        break;
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Transposes the song and saves it. Pick the original key to get it back."));
  if (edited) return;
  if (inst.id == "harmonica") {  // the usual keys for this harp
    Key harp = parseKey(s.harp).value_or(Key{0, "maj"}), second{harp.fifths + 1, "maj"};
    ImGui::TextDisabled(tr("1st position: %s  |  2nd position: %s"), tonicName(harp).c_str(), tonicName(second).c_str());
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", tr("1st position plays in the harmonica's key. 2nd position (cross harp), a fifth above, is the usual choice for blues and folk."));
  }
  if (!s.notes.empty()) { ImGui::Separator(); ImGui::TextWrapped("%s", songText(s, s.notes).c_str()); }
  ImGui::Separator();

  if (a.sel >= 0 && a.sel < (int)s.events.size() && s.events[a.sel].kind == Kind::Note) {
    Event& e = s.events[a.sel];
    ImGui::Text(tr("Note #%d: %s, %s"), a.sel, noteName(spell(s.key, e.midi)).c_str(), durationName(e).c_str());
    if (whistle) ImGui::Text(tr("Fingering: %s"), fingeringText(finger(s.whistle, e.midi)).c_str());
    else if (!inst.plays(e.midi)) ImGui::TextColored({1, 0.4f, 0.4f, 1}, tr("Out of range for the %s"), instName(inst).c_str());
    for (auto& t : a.techs) {
      bool on = std::find(e.techniques.begin(), e.techniques.end(), t.id) != e.techniques.end();
      if (ImGui::Checkbox((t.symbol + "  " + t.name + "###" + t.id).c_str(), &on)) {
        editSelected(a, [&](Event& n) {
          n.techniques.erase(std::remove(n.techniques.begin(), n.techniques.end(), t.id), n.techniques.end());
          if (on) n.techniques.push_back(t.id);
        });
        break;  // the song may have been reloaded; redraw next frame
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.description.c_str());
    }
    ImGui::InputTextMultiline("##comment", a.comment, sizeof a.comment, {-1, ImGui::GetTextLineHeight() * 4});
    // unchanged box = the translation it was filled with: don't overwrite the English with it
    if (ImGui::Button(L("Save comment").c_str()) && a.comment != songText(s, e.comment))
      editSelected(a, [&](Event& n) { n.comment = a.comment; });
  } else ImGui::TextDisabled("%s", tr("Click a note to see and edit its techniques."));

  ImGui::Separator();
  if (ImGui::CollapsingHeader(L("Legend").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    if (whistle) ImGui::TextWrapped("%s", tr("Fingering chart: top three holes = left hand, bottom three = right hand. Filled = covered, empty = open, half = half-covered. + = blow harder (second octave)."));
    for (auto& t : a.techs) {
      ImGui::TextColored({0.95f, 0.55f, 0.15f, 1}, "%-4s", t.symbol.c_str());
      ImGui::SameLine();
      ImGui::Text("%s", t.name.c_str());
      ImGui::Indent();
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      ImGui::TextWrapped("%s", t.description.c_str());
      ImGui::PopStyleColor();
      ImGui::Unindent();
    }
  }
}

static void instrumentCombo(const char* label, std::string& v, const char* none) {  // none: label for "", or null
  auto name = [&](const std::string& id) { return id.empty() ? std::string(none) : instName(*findInstrument(id)); };
  if (!ImGui::BeginCombo(label, name(v).c_str())) return;
  if (none && ImGui::Selectable(none, v.empty())) v.clear();
  for (auto& id : INSTRUMENTS) if (ImGui::Selectable(name(id).c_str(), v == id)) v = id;
  ImGui::EndCombo();
}

// One line per Settings field; App::set is saved by the main loop whenever it changes.
static void settingsWindow(App& a) {
  if (!a.showSettings) return;
  Settings& s = a.set;
  ImGui::SetNextWindowPos({240, 60}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(1);  // the score would show through
  if (!ImGui::Begin(L("Settings").c_str(), &a.showSettings, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
  ImGui::PushItemWidth(220);
  ImGui::SeparatorText(tr("Language"));
  static const auto langs = languages();  // the i18n/ files
  auto langName = [&](const std::string& code) -> std::string {
    if (code.empty()) return tr("Automatic (system language)");
    for (auto& [c, name] : langs) if (c == code) return name;
    return code == "en" ? "English" : code;  // each language in its own name
  };
  if (ImGui::BeginCombo(L("Language").c_str(), langName(s.language).c_str())) {
    std::vector<std::string> codes = {"", "en"};
    for (auto& l : langs) codes.push_back(l.first);
    for (auto& c : codes) if (ImGui::Selectable(langName(c).c_str(), c == s.language)) s.language = c;
    ImGui::EndCombo();
  }
  ImGui::Checkbox(L("Note names as Do Re Mi").c_str(), &s.solfege);
  if (!a.english) ImGui::Checkbox(L("Keys as Do Re Mi").c_str(), &s.solfegeKeys);  // English keys are always letters
  ImGui::SeparatorText(tr("Sheet"));
  ImGui::Checkbox(L("Instrument hints (whistle fingerings)").c_str(), &s.hints);
  if (ImGui::BeginCombo(L("Harmonica key").c_str(), keyLabel(s.harp).c_str())) {
    for (auto& k : HARP_KEYS) if (ImGui::Selectable((keyLabel(k) + "###" + k).c_str(), k == s.harp)) s.harp = k;
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Your harmonica's key, used when a song is switched to harmonica or imported for it"));
  ImGui::SliderFloat(L("Zoom").c_str(), &s.zoom, 6, 16, "%.0f");
  ImGui::Checkbox(L("Light theme").c_str(), &s.light);
  ImGui::SeparatorText(tr("Playback"));
  ImGui::SliderInt(L("Tempo for songs without one").c_str(), &s.tempo, 40, 220);
  ImGui::Checkbox(L("Count-in bar before playing").c_str(), &s.countIn);
  ImGui::Checkbox(L("Metronome").c_str(), &s.metronome);
  ImGui::SliderFloat(L("Volume").c_str(), &s.volume, 0, 1, "%.1f");
  instrumentCombo(L("Sound").c_str(), s.sound, tr("The song's instrument"));
  ImGui::SeparatorText(tr("Microphone"));
  ImGui::SliderFloat(L("Tolerance (cents)").c_str(), &s.tolerance, 10, 100, "%.0f");
  ImGui::SliderFloat(L("Noise gate").c_str(), &s.gate, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat(L("Latency (ms)").c_str(), &s.latencyMs, 0, 300, "%.0f");
#ifndef __EMSCRIPTEN__  // only the agent (john-mcp, desktop) imports songs
  ImGui::SeparatorText(tr("New songs"));
  instrumentCombo(L("Instrument").c_str(), s.instrument, nullptr);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Used when the agent imports a song without saying which instrument it is for"));
#endif
  ImGui::PopItemWidth();
  ImGui::End();
}

#ifndef __EMSCRIPTEN__
static std::string gDropped;  // a file dropped on the window (GLFW drop callback), for Import audio

static void openImport(App& a) {
  a.showImport = true;
  a.importError.clear();
  if (!a.busy) a.importInst = a.set.instrument;  // default from Settings
}

// Import audio: transcribe a recording into a new song. importAudio() runs on a worker thread so the GUI never
// blocks; the main loop polls the future and opens the song when it is ready.
static void importWindow(App& a) {
  if (!gDropped.empty()) {
    snprintf(a.importPath, sizeof a.importPath, "%s", gDropped.c_str());
    gDropped.clear();
    openImport(a);
  }
  if (a.busy && a.importing.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    a.busy = false;
    try {
      Song s = a.importing.get();
      a.showImport = false;
      open(a, s.id);
    } catch (const std::exception& e) {
      a.importError = trf("Couldn't transcribe this file: %s", e.what());
    }
  }
  if (!a.showImport) return;
  ImGui::SetNextWindowPos({260, 80}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(1);
  if (!ImGui::Begin(L("Import audio").c_str(), &a.showImport, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
  ImGui::TextUnformatted(tr("Drop an audio file (mp3, wav or flac) on the John window, or paste its path."));
  ImGui::BeginDisabled(a.busy);
  ImGui::PushItemWidth(320);
  ImGui::InputText(L("File").c_str(), a.importPath, sizeof a.importPath);
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(150);
  ImGui::InputTextWithHint(L("Start (seconds)").c_str(), "0", a.importStart, sizeof a.importStart);
  ImGui::InputTextWithHint(L("End (seconds)").c_str(), tr("end of the file"), a.importEnd, sizeof a.importEnd);
  ImGui::InputTextWithHint(L("Tempo (bpm)").c_str(), tr("detect"), a.importTempo, sizeof a.importTempo);
  ImGui::InputTextWithHint(L("Meter").c_str(), tr("detect"), a.importMeter, sizeof a.importMeter);
  instrumentCombo(L("Instrument").c_str(), a.importInst, nullptr);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The instrument the sheet is for"));
  instrumentCombo(L("Recording instrument").c_str(), a.importRec, tr("not set"));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Who plays the melody in the recording, if you know: John then listens only in its range"));
  ImGui::PopItemWidth();
  if (ImGui::Button(L("Transcribe").c_str())) {
    auto num = [](const char* t, double& v) {  // blank = 0
      char* e;
      v = strtod(t, &e);
      while (*e == ' ') e++;
      return *e == 0 && v >= 0;
    };
    double start, end, bpm;
    TranscribeOptions o;
    o.meter = parseMeter(a.importMeter);
    o.instrument = a.importInst;
    o.recordingInstrument = a.importRec;
    std::error_code ec;
    if (!fs::is_regular_file(a.importPath, ec)) a.importError = tr("File not found.");
    else if (!num(a.importStart, start) || !num(a.importEnd, end)) a.importError = tr("Start and end are seconds into the recording.");
    else if (!num(a.importTempo, bpm)) a.importError = tr("Tempo is quarter notes per minute, or blank to detect it.");
    else if (o.meter.empty() && a.importMeter[strspn(a.importMeter, " ")]) a.importError = tr("Meter is like 4/4, 3/4 or 6/8, or blank to detect it.");
    else {
      a.importError.clear();
      o.bpm = bpm;
      a.busy = true;
      a.importing = std::async(std::launch::async, [path = std::string(a.importPath), start, end, o] { return importAudio(path, start, end, o); });
    }
  }
  ImGui::EndDisabled();
  if (a.busy) { ImGui::SameLine(); ImGui::TextUnformatted(tr("Transcribing...")); }
  if (!a.importError.empty()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
    ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", a.importError.c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::End();
}
#endif

int main() {
  try { installSongs(lessonsDir()); } catch (...) {}
#ifdef __EMSCRIPTEN__
  try { installSongs("/demo"); } catch (...) {}  // starter songs, web only: desktop users have the agent
#endif
  if (!glfwInit()) return 1;
#ifndef __EMSCRIPTEN__  // the web gets WebGL and imgui's default "#version 100" shaders
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
  GLFWwindow* win = glfwCreateWindow(1400, 900, "John", nullptr, nullptr);
  if (!win) return 1;
#ifdef __EMSCRIPTEN__
  emscripten_glfw_make_canvas_resizable(win, "window", nullptr);  // the canvas fills the browser window
#endif
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui::GetStyle().FontSizeBase = 16.0f;
  std::string textFont = bundled("fonts/NotoSans-Regular.ttf", JOHN_TEXT_FONT).string();  // release package, else build dir
  std::string musicFont = bundled("fonts/Bravura.otf", JOHN_FONT).string();
  if (fs::exists(textFont)) gText = io.Fonts->AddFontFromFileTTF(textFont.c_str(), 16.0f);
  if (!gText) gText = io.Fonts->AddFontDefault();
  if (fs::exists(musicFont)) gMusic = io.Fonts->AddFontFromFileTTF(musicFont.c_str(), 36.0f);
  ImGui_ImplGlfw_InitForOpenGL(win, true);
#ifdef __EMSCRIPTEN__
  ImGui_ImplOpenGL3_Init(nullptr);
#else
  ImGui_ImplOpenGL3_Init("#version 150");
  glfwSetDropCallback(win, [](GLFWwindow*, int n, const char** paths) { if (n > 0) gDropped = paths[0]; });
#endif

  App a;
  auto applyLanguage = [&a](const std::string& code) {
    a.english = !setLanguage(code.empty() ? systemLanguage() : code);
    for (auto& [id, m] : a.meta) m.mtime = {};  // reload the song list's titles in the new language
    a.lastPoll = -1;
  };
  std::string lang = a.set.language;  // language currently applied
  applyLanguage(lang);
  const char* snapshot = std::getenv("JOHN_SNAPSHOT");  // dev aid: write the window to a .tga after a few frames and exit
  bool light = false;  // theme currently applied
  int frame = 0;
  auto step = [&]() -> bool {  // one frame; false = stop
    if (gMusic && frame == 1) {  // measure a notehead (1.18 staff spaces = 0.295 em) once the font is usable
      float w = gMusic->CalcTextSizeA(100, FLT_MAX, 0, utf8(HeadBlack).c_str()).x;
      if (w > 0) gMusicScale = 29.5f / w;
    }
    bool live = snapshot || a.audio.playing() || a.listen;
#ifdef __EMSCRIPTEN__
    live = true;  // the browser paces the frames; the GLFW port has no glfwWaitEventsTimeout
#endif
    if (live) glfwPollEvents(); else glfwWaitEventsTimeout(0.5);  // idle cheaply, still notice agent edits
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    try { poll(a); } catch (const std::exception&) {}  // e.g. data folder briefly unavailable; retry next poll
    practiceUpdate(a, io.DeltaTime);
    if (a.set.light != light) { light = a.set.light; light ? ImGui::StyleColorsLight() : ImGui::StyleColorsDark(); }
    if (!a.busy) {  // the transcription thread reads them (tr(), keyName())
      if (a.set.language != lang) applyLanguage(lang = a.set.language);
      setSolfege(a.set.solfege, a.set.solfegeKeys);
    }
    a.audio.metronome = a.set.metronome;
    a.audio.volume = a.set.volume;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::BeginChild("songs", {220, 0}, ImGuiChildFlags_Borders);
    ImGui::TextDisabled("%s", tr("SONGS"));
    auto instLabel = [](const std::string& id) -> std::string { return id.empty() ? tr("All instruments") : id == "whistle" ? tr("Tin whistle") : instName(*findInstrument(id)); };
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##instFilter", instLabel(a.set.songFilter).c_str())) {
      if (ImGui::Selectable(instLabel("").c_str(), a.set.songFilter.empty())) a.set.songFilter = "";
      for (auto& id : INSTRUMENTS)
        if (ImGui::Selectable(instLabel(id).c_str(), id == a.set.songFilter)) a.set.songFilter = id;
      ImGui::EndCombo();
    }
    std::map<std::string, std::vector<std::string>, decltype(&folderBefore)> byFolder(&folderBefore);
    for (auto& id : a.ids)
      if (a.set.songFilter.empty() || a.meta[id].in.id == a.set.songFilter) byFolder[a.meta[id].folder].push_back(id);
    for (auto& [folder, ids] : byFolder) {
      std::string label = tr(folder.empty() ? "Unsorted" : folder.c_str()) + std::string(" (") + std::to_string(ids.size()) + ")###" + folder;
      if (!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) continue;
      for (auto& id : ids) {
        auto& m = a.meta[id];
        std::string tag = m.in.name == "D whistle" ? "" : instName(m.in);  // the instrument after the title
        float x = ImGui::GetCursorPosX();
        if (ImGui::Selectable((m.title + "##" + id).c_str(), id == a.current)) { a.sel = -1; open(a, id); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s%s%s", m.title.c_str(), tag.empty() ? "" : " - ", tag.c_str());  // long titles get clipped
        if (!tag.empty()) {
          ImGui::SameLine(x + ImGui::CalcTextSize(m.title.c_str()).x + 6);
          ImGui::TextDisabled("%s", tag.c_str());
        }
      }
      ImGui::TreePop();
    }
#ifdef __EMSCRIPTEN__  // no agent on the web: point to the desktop app instead
    if (a.ids.empty()) ImGui::TextWrapped("%s", tr("No songs yet. Songs are imported in the desktop app."));
#else
    if (a.ids.empty()) ImGui::TextWrapped("%s", tr("No songs yet. Ask the agent to import one via the john MCP server."));
#endif
    else if (byFolder.empty()) ImGui::TextDisabled("%s", tr("No songs for this instrument."));
    ImGui::Separator();
#ifdef __EMSCRIPTEN__
    if (ImGui::TextLink(L("Get the desktop app").c_str())) emscripten_run_script("window.open('https://github.com/Thinato/john/releases', '_blank')");
    ImGui::TextWrapped("%s", tr("Adds the AI agent (MCP), audio transcription and songs on disk."));
#else
    ImGui::Checkbox(L("Follow agent").c_str(), &a.follow);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Switch to a song as soon as the agent changes it"));
#endif
    const bool noHints = a.song && instrumentOf(*a.song).id != "whistle";  // only the whistle has hints so far
    ImGui::BeginDisabled(noHints);
    bool shown = a.set.hints && !noHints;  // unticked while disabled; a.set.hints keeps the choice for whistle songs
    if (ImGui::Checkbox(L("Instrument hints").c_str(), &shown) && !noHints) a.set.hints = shown;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      if (noHints) ImGui::SetTooltip(tr("No hints for the %s yet"), instName(instrumentOf(*a.song)).c_str());
      else ImGui::SetTooltip("%s", tr("Show fingering charts and fingerings for the notes"));
    }
    ImGui::SliderFloat(L("Zoom").c_str(), &a.set.zoom, 6, 16, "%.0f");
    if (ImGui::Button(L("Settings").c_str())) a.showSettings = !a.showSettings;
#ifndef __EMSCRIPTEN__  // the web keeps songs in the browser (IndexedDB) and has no transcription
    ImGui::SameLine();
    if (ImGui::Button(L("Import audio").c_str())) {
      if (a.showImport) a.showImport = false;
      else openImport(a);
    }
    ImGui::TextDisabled("%s", tr("Data folder:"));
    ImGui::TextWrapped("%s", dataDir().string().c_str());
#endif
    ImGui::EndChild();
    ImGui::SameLine();

    if (a.song) {
      float detailsW = 340;
      float scoreW = ImGui::GetContentRegionAvail().x - detailsW - 8;
      ImGui::BeginGroup();
      ImGui::BeginChild("transport", {scoreW, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
      transportBar(a);
      ImGui::EndChild();
      ImGui::BeginChild("score", {scoreW, 0}, ImGuiChildFlags_Borders);
      drawScore(a, *a.song);
      ImGui::EndChild();
      ImGui::EndGroup();
      ImGui::SameLine();
      ImGui::BeginChild("details", {0, 0}, ImGuiChildFlags_Borders);
      detailsPanel(a);
      ImGui::EndChild();
    }
    ImGui::End();
    settingsWindow(a);
#ifndef __EMSCRIPTEN__
    importWindow(a);
#endif
    if (a.set != a.saved && !ImGui::IsAnyItemActive()) {  // save on change, once a slider drag ends
      a.saved = a.set;
      try { saveSettings(a.set); } catch (const std::exception&) {}
    }

    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (snapshot && frame == 5) {
      std::vector<unsigned char> px((size_t)w * h * 4);
      glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
      FILE* f = fopen(snapshot, "wb");
      unsigned char hdr[18] = {0, 0, 2};
      hdr[12] = w & 255; hdr[13] = w >> 8; hdr[14] = h & 255; hdr[15] = h >> 8; hdr[16] = 32;
      if (f) { fwrite(hdr, 1, 18, f); fwrite(px.data(), 1, px.size(), f); fclose(f); }
      return false;
    }
    glfwSwapBuffers(win);
    frame++;
    return true;
  };
#ifdef __EMSCRIPTEN__
  // the browser drives the frames; main() never returns, so App and the window stay alive
  emscripten_set_main_loop_arg([](void* f) { (*static_cast<decltype(step)*>(f))(); }, &step, 0, true);
#else
  while (!glfwWindowShouldClose(win) && step()) {}
#endif
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwTerminate();
}
