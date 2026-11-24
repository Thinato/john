// The score: staff notation of the open song with fingering charts, technique marks, lyrics and the pitch track.
#include "gui.hpp"
#include <algorithm>
#include <cmath>

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

// drawScore's shared state: the song, the sizes it is drawn at, and the layout that the steps below fill in, in order.
struct Page {
  App& a;
  Song& s;
  const float sp = a.set.zoom;
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x;
  Score sc{ImGui::GetWindowDrawList(), sp};
  std::vector<Pos> pos = positions(s);
  std::vector<Accidental> acc = accidentals(s);
  const float headW = sp * 1.18f, accW = sp * 1.3f;
  const float margin = sp * 2;
  const Instrument inst = myInstrument(a.set);  // hints and range are for the instrument I play, not the song's (#90)
  const bool whistle = inst.hints && a.set.hints;  // fingering charts (whistle-only so far), and they can be hidden
  const float sysH = sp * (whistle ? 26 : 19);  // a system with only near notes on one staff
  const bool grand = grandStaff(s);
  const float bassDy = grand ? sp * 10 : 0;  // treble top line to bass top line
  const PitchTrack* trk = a.track && a.track->sync.size() >= 2 ? &*a.track : nullptr;
  const float stripH = trk ? sp * 5 : 0;  // pitch track strip at the bottom of each system
  const float lyS = sp * 1.6f;  // lyrics text size
  const bool showLyrics = a.set.lyrics;  // off: no lyrics drawn and no room kept for them
  const int absF = std::abs(s.key.fifths);
  const ImU32 ink = sc.ink, red = IM_COL32(200, 40, 40, 255), techCol = IM_COL32(205, 95, 0, 255);
  const float thin = std::max(1.0f, sp * 0.12f);

  std::vector<Slot> slots;                 // breakLines
  std::vector<std::pair<int, int>> lines;  // slot ranges per system
  std::vector<Room> room;                  // measureSystems
  std::vector<float> sysY{origin.y};  // system tops, then the end of the last one
  std::vector<std::vector<int>> beams;     // groupBeams
  float totalH = 0;                        // drawPaper

  float lyricW(const Event& e) const { return gText->CalcTextSizeA(lyS, FLT_MAX, 0, e.lyric.c_str()).x; }
  float slotW(const Event& e, int i) const {
    if (e.kind == Kind::Bar) return sp * (e.bar == "|" ? 1.4f : 2.4f);
    float w = sp * (1.7f + 1.7f * std::sqrt(e.len / 24.0f)) + (acc[i].shown ? accW : 0);
    if (showLyrics && !e.lyric.empty()) w = std::max(w, lyricW(e) + sp * (e.lyricHyphen ? 1.6f : 0.8f));  // room for the syllable (and its hyphen)
    return w;
  }
  float headerW(bool first) const { return sp * (4.0f + absF * 1.0f + (first ? 2.6f : 0)); }
  float sysHOf(int sys) const { return sysY[sys + 1] - sysY[sys]; }
  float staffTop(int sys) const { return sysY[sys] + sp * 8 + room[sys].above * sp / 2; }  // treble staff top line
  // below the lowest note: the fingering charts, then the note names, then the lyrics
  float chartTopOf(int sys) const { return staffTop(sys) + bassDy + sp * 7.5f + room[sys].below * sp / 2; }
  float lyricTop(int sys) const { return chartTopOf(sys) + sp * (whistle ? 10.0f : 2.6f); }
  float yOf(int sys, int step, bool bass) const { return staffTop(sys) + (bass ? bassDy - sp * 6 : 0) + sp * 4 - (step - 30) * sp / 2; }

  void breakLines();
  void measureSystems();
  void placeNotes();
  void groupBeams();
  int drawPaper();  // returns the hovered note's event, or -1
  void drawStaves();
  void drawEvents();
  void drawBar(int k);
  void drawRest(int k);
  void drawNote(int k);
  void drawMarks(const Slot& sl, const Event& e, float top);
  void drawChart(const Slot& sl, const Event& e, const Fingering& f);
  void drawLyrics();
  bool drawPitchTrack();  // returns whether the mouse is over a strip
  void drawBeams();
  void noteTooltip(int hovered);
};

void Page::breakLines() {
  // ---- line breaking: measures end at bar events; justify full lines ----
  std::vector<std::vector<int>> measures(1);
  for (int i = 0; i < (int)s.events.size(); i++) {
    measures.back().push_back(i);
    if (s.events[i].kind == Kind::Bar) measures.emplace_back();
  }
  if (measures.back().empty()) measures.pop_back();
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
}

void Page::measureSystems() {
  // each system grows above and below for its far notes (and by a bass staff on a grand staff)
  // and by a line of lyrics under the note names when it has any
  for (auto [f, t] : lines) {
    room.push_back(f < t ? systemRoom(s, slots[f].ev, slots[t - 1].ev + 1, whistle ? 4 : 6) : Room{});
    bool words = showLyrics && std::any_of(slots.begin() + f, slots.begin() + t, [&](const Slot& sl) { return !s.events[sl.ev].lyric.empty(); });
    sysY.push_back(sysY.back() + sysH + bassDy + (room.back().above + room.back().below) * sp / 2 + stripH + (words ? sp * 2.4f : 0));
  }
}

void Page::placeNotes() {
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
}

void Page::groupBeams() {
  // ---- beam groups ----
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
}

int Page::drawPaper() {
  // ---- paper, hover ----
  totalH = sysY.back() - origin.y + sp * 2;
  sc.dl->AddRectFilled(origin, {origin.x + width, origin.y + totalH}, IM_COL32(252, 250, 244, 255), 4);
  ImVec2 mouse = ImGui::GetIO().MousePos;
  int hovered = -1;
  for (auto& sl : slots) {
    if (s.events[sl.ev].kind != Kind::Note) continue;
    float top = sysY[sl.sys], h = sysHOf(sl.sys) - stripH;
    if (sl.ev == a.snapHover) a.snapMouse = {sl.x + sl.w / 2, top + h / 2};  // JOHN_SNAPSHOT_HOVER: main() moves the mouse here
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
  return hovered;
}

void Page::drawStaves() {
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
}

void Page::drawEvents() {
  for (int k = 0; k < (int)slots.size(); k++) {
    const Kind kind = s.events[slots[k].ev].kind;
    if (kind == Kind::Bar) drawBar(k);
    else if (kind == Kind::Rest) drawRest(k);
    else drawNote(k);
  }
}

void Page::drawBar(int k) {
  Slot& sl = slots[k];
  const Event& e = s.events[sl.ev];
  float top = staffTop(sl.sys), bottom = top + bassDy + sp * 4;  // bottom: lowest staff's bottom line
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
}

void Page::drawRest(int k) {
  Slot& sl = slots[k];
  const Event& e = s.events[sl.ev];
  float top = staffTop(sl.sys);
  int b = sl.v.base;
  bool fullBar = e.len == meterTicks(s.meter);
  unsigned g = fullBar || b == WHOLE ? RestWhole : b == WHOLE / 2 ? RestHalf : RestQuarter + (b == 24 ? 1 : b == 12 ? 2 : b == 6 ? 3 : 0);
  float y = g == RestWhole ? top + sp : top + sp * 2;
  sc.glyph(g, sl.x + sp * 0.4f, y, ink);
  for (int d = 0; d < sl.v.dots && !fullBar; d++) sc.glyph(Dot, sl.x + sp * (2.0f + d * 0.6f), top + sp * 1.5f, ink);
}

void Page::drawNote(int k) {
  Slot& sl = slots[k];
  const Event& e = s.events[sl.ev];
  float top = staffTop(sl.sys);
  Fingering f = whistle ? finger(inst.key, e.midi) : Fingering{"", 0, inst.plays(e.midi)};
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
  drawMarks(sl, e, top);
  drawChart(sl, e, f);
}

void Page::drawMarks(const Slot& sl, const Event& e, float top) {
  // techniques above the note, breath mark after it: the marks my instrument shows (#86), or all of them
  float tTop = std::min({top - sp * 0.8f, sl.y - sp, sl.up ? sl.stemEnd : sl.y}) - sp * 0.4f;
  for (auto& t : a.set.allMarks ? e.techniques : marks(e.techniques, inst).shown) {
    if (t == "breath") { sc.glyph(BreathComma, sl.x + sl.w - sp * 0.6f, top - sp * 0.5f, techCol); continue; }
    auto it = std::find_if(a.techs.begin(), a.techs.end(), [&](auto& x) { return x.id == t; });
    std::string sym = it != a.techs.end() ? it->symbol : t;
    tTop -= sp * 2.1f;
    sc.text(sym, sl.cx, tTop, sp * 2.2f, techCol);
  }
}

void Page::drawChart(const Slot& sl, const Event& e, const Fingering& f) {
  // fingering chart: six holes, top hand first
  float chartTop = chartTopOf(sl.sys), r = sp * 0.42f;
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

void Page::drawLyrics() {
  // ---- lyrics: a hyphen before the next syllable of a word, a line under the notes that hold a syllable ----
  // The syllable being sung (#87) is highlighted: the play-along cursor's, or the one its note holds over (held or tied).
  auto holds = [&](int i) {
    const Event& h = s.events[i];
    return h.kind == Kind::Bar || (h.kind == Kind::Note && h.lyric.empty() && (h.lyricHeld || tiedFrom(s, i)));
  };
  int sung = a.cursor < (int)s.events.size() ? a.cursor : -1;
  while (sung > 0 && holds(sung)) sung--;
  for (int k = 0; showLyrics && k < (int)slots.size(); k++) {
    const Slot& sl = slots[k];
    const Event& e = s.events[sl.ev];
    if (e.kind != Kind::Note || e.lyric.empty()) continue;
    const float y = lyricTop(sl.sys), x0 = sl.cx + lyricW(e) / 2;
    if (sl.ev == sung) sc.dl->AddRectFilled({sl.cx - lyricW(e) / 2 - sp * 0.4f, y - sp * 0.1f}, {x0 + sp * 0.4f, y + lyS * 1.1f}, IM_COL32(60, 200, 90, 120), sp * 0.3f);
    sc.text(e.lyric, sl.cx, y, lyS, sl.ev == sung ? IM_COL32(0, 100, 30, 255) : ink);
    if (e.lyricHyphen) {  // centred between this syllable and the next one, or after it at the end of the line
      int nk = k + 1;
      while (nk < (int)slots.size() && slots[nk].sys == sl.sys && s.events[slots[nk].ev].lyric.empty()) nk++;
      bool here = nk < (int)slots.size() && slots[nk].sys == sl.sys;
      float x1 = here ? slots[nk].cx - lyricW(s.events[slots[nk].ev]) / 2 : x0 + sp * 1.2f;
      sc.text("-", (x0 + x1) / 2, y, lyS, ink);
      continue;
    }
    float end = 0;  // right edge of the last note holding the syllable (tied notes pass it on)
    for (int j = k + 1; j < (int)slots.size() && slots[j].sys == sl.sys; j++) {
      const Event& h = s.events[slots[j].ev];
      if (h.kind == Kind::Bar) continue;
      if (h.kind != Kind::Note || !(h.lyricHeld || tiedFrom(s, slots[j].ev))) break;
      if (h.lyricHeld) end = slots[j].headX + headW;
    }
    if (end > x0) sc.line(x0 + sp * 0.2f, y + lyS * 0.85f, end, y + lyS * 0.85f, thin, ink);
  }
}

bool Page::drawPitchTrack() {
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
  return stripHovered;
}

void Page::drawBeams() {
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
}

void Page::noteTooltip(int hovered) {
  const Event& e = s.events[hovered];
  ImGui::BeginTooltip();
  ImGui::Text("%s  %s", noteName(spell(s.key, e.midi)).c_str(), durationName(e).c_str());
  if (whistle) ImGui::Text(tr("fingering: %s"), fingeringText(finger(inst.key, e.midi)).c_str());
  else if (!inst.plays(e.midi)) ImGui::TextColored({1, 0.4f, 0.4f, 1}, tr("out of range for the %s"), instName(inst).c_str());
  auto techName = [&](const std::string& t) {
    auto it = std::find_if(a.techs.begin(), a.techs.end(), [&](auto& x) { return x.id == t; });
    return it != a.techs.end() ? it->name : t;
  };
  if (a.set.allMarks) for (auto& t : e.techniques) ImGui::BulletText("%s", techName(t).c_str());
  else {  // what the sheet shows, then the marks hidden on my instrument, so the comment about them still reads
    const Marks m = marks(e.techniques, inst);
    auto lists = [&](const std::string& x) { return std::count(inst.techniques.begin(), inst.techniques.end(), x) > 0; };
    for (auto& t : m.shown) {
      if (t == "breath" && !lists(t)) { ImGui::BulletText("%s", tr("Phrase end (lift)")); continue; }  // as in the legend
      auto from = std::find_if(e.techniques.begin(), e.techniques.end(), [&](auto& x) { return !lists(x) && inst.instead.count(x) && inst.instead.at(x) == t; });
      bool mapped = !std::count(e.techniques.begin(), e.techniques.end(), t) && from != e.techniques.end();
      ImGui::BulletText("%s", mapped ? trf("%s (instead of %s)", techName(t).c_str(), techName(*from).c_str()).c_str() : techName(t).c_str());
    }
    for (auto& t : m.hidden) ImGui::TextDisabled(tr("hidden: %s"), techName(t).c_str());
  }
  if (!e.comment.empty()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25);
    ImGui::TextWrapped("%s", songText(s, e.comment).c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::EndTooltip();
}

void drawScore(App& a, Song& s) {
  if (!gMusic) { ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", tr("Music font missing (Bravura.otf). Re-run cmake to download it.")); return; }
  Page p{a, s};
  p.breakLines();
  p.measureSystems();
  p.placeNotes();
  p.groupBeams();
  const int hovered = p.drawPaper();
  p.drawStaves();
  p.drawEvents();
  p.drawLyrics();
  const bool stripHovered = p.drawPitchTrack();
  p.drawBeams();

  ImGui::Dummy({p.width, p.totalH});
  if (!s.words.empty()) ImGui::TextWrapped("%s", s.words.c_str());  // ABC W: lines, printed after the tune
  if (stripHovered) ImGui::SetTooltip("%s", tr("Pitch heard in the recording (faint where it is unclear)"));
  if (hovered >= 0) p.noteTooltip(hovered);
}
