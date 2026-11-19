// Core self-checks: key parsing, spelling, fingerings, ABC import, transpose, analysis, JSON round trip.
#undef NDEBUG  // the checks are asserts: keep them in Release builds
#include "john.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <regex>
#include <set>
#include <stdexcept>
#include <miniz.h>
#ifdef _WIN32
#define setenv(k, v, overwrite) _putenv_s(k, v)
#define unsetenv(k) _putenv_s(k, "")
#endif

using namespace john;

static const Event& nth(const Song& s, int n) {  // n-th note/rest (skipping bars)
  for (auto& e : s.events) if (e.kind != Kind::Bar && n-- == 0) return e;
  assert(false);
  return s.events[0];
}

// Share of the track's voiced frames, placed on the song through PitchTrack::sync, that land on a note of the same
// pitch class (the written octave differs from the recording's by the instrument).
static double aligned(const Song& s, const PitchTrack& t) {
  std::vector<int> at(s.events.size() + 1, 0);
  for (size_t i = 0; i < s.events.size(); i++) at[i + 1] = at[i] + (s.events[i].kind == Kind::Bar ? 0 : s.events[i].len);
  int same = 0, all = 0;
  size_t i = 0;
  for (auto& p : t.points) {
    double tick = trackTick(t, p.time);
    while (i < s.events.size() && at[i + 1] <= tick) i++;
    if (i == s.events.size()) break;
    if (tick < 0 || !p.midi || s.events[i].kind != Kind::Note) continue;
    all++;
    same += (p.midi - s.events[i].midi) % 12 == 0;
  }
  return all ? (double)same / all : 0;
}

int main() {
  fs::path home = fs::temp_directory_path() / ("john-test-" + std::to_string(std::random_device{}()));
  setenv("JOHN_HOME", home.string().c_str(), 1);

  assert(parseKey("D")->fifths == 2);
  assert(parseKey("Ador")->fifths == 1 && parseKey("Ador")->mode == "dor");
  assert(parseKey("Bm")->fifths == 2 && parseKey("Bminor")->mode == "min");
  assert(parseKey("G Mixolydian")->fifths == 0);
  assert(parseKey("Bb")->fifths == -2);
  assert(parseKey("Dmajor")->fifths == 2);  // thesession.org style
  assert(!parseKey("H"));
  assert(keyName({1, "dor"}) == "A dorian");
  assert(keyName({-2, "maj"}) == "Bb major");
  assert(keyName({3, "min"}) == "F# minor");

  Key d{2, "maj"};
  assert(noteName(spell(d, 66)) == "F#4");
  assert(noteName(spell(d, 72)) == "C5");
  assert(noteName(spell({-1, "maj"}, 70)) == "Bb4");
  assert(noteName(spell({0, "maj"}, 61)) == "C#4");

  assert(finger("D", 62).holes == "XXXXXX" && finger("D", 62).octave == 0);
  assert(finger("D", 79).holes == "XXXOOO" && finger("D", 79).octave == 1);
  assert(finger("D", 72).holes == "OXXOOO");
  assert(finger("D", 85).ok && !finger("D", 86).ok && !finger("D", 61).ok);
  assert(finger("C", 60).holes == "XXXXXX");
  assert(fingeringText(finger("D", 76)) == "XXX XXO +");

  // ABC: roll, grace cut, tuplet, rest, tie, voltas, key signature
  auto r = importAbc("X:1\nT:Test Jig\nM:6/8\nL:1/8\nK:D\n|:~A3 {g}AFA|d2e f2d|(3efg a2 z|1 d3- d2:|2 d3 d2B||\n");
  Song& s = r.song;
  assert(s.title == "Test Jig" && s.meter == "6/8" && s.key.fifths == 2);
  assert(s.events[0].kind == Kind::Bar && s.events[0].bar == "|:");
  assert(nth(s, 0).midi == 69 && nth(s, 0).len == 72 && nth(s, 0).techniques == std::vector<std::string>{"roll"});
  assert(nth(s, 1).techniques == std::vector<std::string>{"cut"});
  assert(nth(s, 2).midi == 66);  // F is sharp in D
  assert(nth(s, 8).len == 16 && nth(s, 8).tuplet == 3 && nth(s, 8).tupletStart && !nth(s, 9).tupletStart);
  assert(nth(s, 12).kind == Kind::Rest);
  assert(nth(s, 13).tie);
  int endings = 0, repeats = 0;
  for (auto& e : s.events) { endings += !e.ending.empty(); repeats += e.bar == ":|"; }
  assert(endings == 2 && repeats == 1);
  assert(r.warnings.empty());

  // broken rhythm and accidentals that last for the bar
  auto b = importAbc("X:1\nT:b\nM:4/4\nL:1/8\nK:G\nA>B c<d|\n").song;
  assert(nth(b, 0).len == 36 && nth(b, 1).len == 12 && nth(b, 2).len == 12 && nth(b, 3).len == 36);
  auto a = importAbc("X:1\nT:a\nM:4/4\nL:1/8\nK:D\n=F F f2 | F4|\n").song;
  assert(nth(a, 0).midi == 65 && nth(a, 1).midi == 65 && nth(a, 2).midi == 78 && nth(a, 3).midi == 66);
  auto acc = accidentals(a);
  assert(acc[0].shown && acc[0].alter == 0 && !acc[1].shown);

  // score layout: grand staff for piano (below middle C on the bass staff), room grows only for far notes
  auto pn = importAbc("X:1\nT:p\nM:4/4\nL:1/4\nK:C\nA,,,, C c''' B,|\n").song;  // A0 C4 C8 B3
  assert(!grandStaff(pn));
  pn.instrument = "piano";
  assert(grandStaff(pn) && onBassStaff(pn, 59) && !onBassStaff(pn, 60));
  Room rm = systemRoom(pn, 0, (int)pn.events.size(), 6);
  assert(rm.above == 56 - 44 && rm.below == 18 - 6 - 5);    // C8 above the treble staff, A0 below the bass staff
  rm = systemRoom(pn, 1, 2, 6);                             // C4 alone: treble staff, no room needed
  assert(rm.above == 0 && rm.below == 0);
  auto gt = importAbc("X:1\nT:g\nM:4/4\nL:1/4\nK:Em\nE, G B e|\n").song;
  gt.instrument = "guitar";
  rm = systemRoom(gt, 0, (int)gt.events.size(), 6);
  assert(!grandStaff(gt) && rm.above == 0 && rm.below == 1);  // low E3 one step past the note-name room
  auto wh = importAbc("X:1\nT:w\nM:4/4\nL:1/4\nK:D\nD F b c'|\n").song;  // whistle D4..C#6: normal spacing
  rm = systemRoom(wh, 0, (int)wh.events.size(), 4);
  assert(rm.above == 0 && rm.below == 0);

  // thesession.org bare body: no headers, "!" line breaks
  auto t = importAbc("A2FA A2dB|A2FA BEE2|! A2FA A2Bd|egfd edBd|");
  assert(t.warnings == std::vector<std::string>{"no K: field, assumed D major"} && nth(t.song, 1).midi == 66 && t.song.title == "Untitled");

  // pickup bar is bar 0; positions follow elapsed time
  auto pk = importAbc("X:1\nT:p\nM:4/4\nL:1/8\nK:Edor\n|:D2|EBBA B2EB|]\n").song;
  auto ps = positions(pk);
  assert(ps[1].bar == 0 && ps[1].tick == WHOLE * 3 / 4);  // D2 pickup
  assert(ps[3].bar == 1 && ps[3].tick == 0);                // first E

  // review regressions: header comments, volta at start, zero/huge lengths, odd meters, voltas restart time
  auto hc = importAbc("%abc-2.1\nX:1\nT:G jig\n% comment\nM:6/8\nL:1/8\nK:G\nGAB cBc|\n");
  assert(hc.song.key.fifths == 1 && hc.song.meter == "6/8" && nth(hc.song, 3).midi == 72);
  assert(!importAbc("[1 AB|").song.events.empty());
  assert(!importAbc("|1 AB|").song.events.empty());
  assert(nth(importAbc("X:1\nK:D\nA/0 B|").song, 0).len > 0);
  assert(importAbc("X:1\nK:D\nZ3000000|").song.events.size() < 200);
  assert(parseMeter("6/8") == "6/8" && parseMeter("C") == "4/4" && parseMeter("C|") == "2/2" && parseMeter("1/2") == "1/2" && parseMeter("7/16") == "7/16");
  for (const char* bad : {"", "4", "x", "4/", "/4", "0/4", "4/0", "4/3", "4/32", "4/4x", "33/4", "-3/4", "none"}) assert(parseMeter(bad).empty());
  assert(importAbc("X:1\nM:C|\nK:D\nA|").song.meter == "C|" && importAbc("X:1\nM:none\nK:D\nA|").song.meter == "4/4");
  assert(meterTicks("1/256") > 0 && meterTicks("0/4") == WHOLE);
  assert(whistleBase("Dm") < 0 && whistleBase("Bb") == 70 && whistleBase("D") == 62);
  auto vt = importAbc("X:1\nT:v\nM:6/8\nL:1/8\nK:D\n|:A|d2d fed|1 d3 d2:|2 d3 d2e||f3 f2e|\n").song;
  auto vp = positions(vt);
  for (size_t i = 0; i < vt.events.size(); i++)
    if (vt.events[i].kind == Kind::Note && vt.events[i].midi == 78 && vt.events[i].len == 36) assert(vp[i].tick == 0);

  // play order: repeats and 1st/2nd endings
  auto po = importAbc("X:1\nT:o\nM:4/4\nL:1/4\nK:D\n|: A B |1 c :|2 d |] |: e :: f :|\n").song;
  std::string seq;
  for (int i : playOrder(po)) seq += noteName(spell(po.key, po.events[i].midi)).substr(0, 1);
  assert(seq == "ABCABDEEFF");
  auto orderOf = [](const std::string& body) {
    auto so = importAbc("X:1\nT:o\nM:4/4\nL:1/4\nK:C\n" + body + "\n").song;
    std::string r;
    for (int i : playOrder(so)) r += noteName(spell(so.key, so.events[i].midi)).substr(0, 1);
    return r;
  };
  assert(orderOf("|: A B |1 c :|2 d || e f |1 g :|2 a |]") == "ABCABDEFGEFA");  // B part without |:
  assert(orderOf("|: A B |1 c :| d |]") == "ABCABD");                          // implied 2nd ending
  assert(orderOf("|: A |1 B :|2 C :|3 D |]") == "ABACAD");                      // three endings
  assert(orderOf("A B :| c |]") == "ABABC");                                     // repeat to the start

  // ties merge only the tied notes; the next same-pitch note is re-articulated
  auto tie = perform(importAbc("X:1\nT:t\nM:4/4\nL:1/4\nK:D\nA- A A B|\n").song, 120, 0, false);
  assert(tie.voices.size() == 3 && std::abs(tie.voices[0].dur - 1.0) < 1e-9);

  // count-in stays on the beat grid for a jig pickup: clicks every dotted quarter
  auto jig = perform(importAbc("X:1\nT:j\nM:6/8\nL:1/8\nK:D\nA|d2d fed|\n").song, 120, 0, true);
  for (size_t i = 1; i < jig.clicks.size(); i++) assert(std::abs(jig.clicks[i].first - jig.clicks[i - 1].first - 0.75) < 1e-9);
  assert(std::abs(jig.voices[0].start - (jig.countIn)) < 1e-9 && std::abs(jig.voices[1].start - jig.clicks[2].first - 0.75) < 1e-9);

  // performance timing: quarter = 0.5 s at 120 bpm, one bar count-in, ties merge, cut adds a grace
  auto pf = importAbc("X:1\nT:p\nM:4/4\nL:1/4\nK:D\n{g}A B- B c|\n").song;
  Performance perf = perform(pf, 120);
  assert(std::abs(perf.countIn - 2.0) < 1e-9 && perf.voices.size() == 3);
  assert(std::abs(perf.voices[0].start - 2.0) < 1e-9 && std::abs(perf.voices[1].dur - 1.0) < 1e-9);
  assert(perf.voices[0].graces.size() == 1 && std::abs(perf.length - 4.0) < 1e-9);
  assert(perf.clicks.size() == 8 && perf.clicks[0].second && !perf.clicks[1].second);

  // pitch detection: pure sine, synthesized whistle note, silence
  {
    const int sr = 48000, n = 2048;
    std::vector<float> buf(n);
    for (int i = 0; i < n; i++) buf[i] = 0.5f * std::sin(6.283185307f * 880.0f * i / sr);
    assert(std::abs(detectPitch(buf.data(), n, sr) - 880) < 2);
    Song one = importAbc("X:1\nT:x\nM:4/4\nL:1/4\nK:D\nA4|\n").song;  // A4 written, sounds A5 = 880 Hz
    Performance p1 = perform(one, 60, 0, false);
    Synth syn;
    syn.metronome = false;
    long long sp = sr / 2;  // skip the attack
    std::vector<float> w(n);
    syn.render(p1, sp, w.data(), n);
    float hz = detectPitch(w.data(), n, sr);
    assert(std::abs(writtenMidiFromFreq(hz, 1) - 69) < 0.2);
    std::vector<float> quiet(n, 0.0f);
    assert(detectPitch(quiet.data(), n, sr) == 0 && rms(quiet.data(), n) == 0);
  }

  // instruments: ranges, written vs sounding octave, JSON, playback pitch, range hints
  {
    for (auto& id : INSTRUMENTS) assert(findInstrument(id));
    assert(!findInstrument("kazoo") && !findInstrument("whistle", "Dm"));
    Instrument cw = *findInstrument("whistle", "C");
    assert(cw.low == 60 && cw.high == 83 && cw.octave == 1 && cw.name == "C whistle");
    assert(findInstrument("guitar")->octave == -1 && findInstrument("violin")->plays(55) && !findInstrument("violin")->plays(54));
    assert(std::abs(soundingFreq(69, 0) - 440) < 1e-9 && std::abs(soundingFreq(69, 1) - 880) < 1e-9);
    assert(std::abs(soundingFreq(69, -1) - 220) < 1e-9 && std::abs(writtenMidiFromFreq(220, -1) - 69) < 1e-9);

    Song v = importAbc("X:1\nT:v\nM:4/4\nL:1/4\nK:D\nA4|\n").song;
    assert(v.instrument == "whistle" && fromJson(toJson(v)).instrument == "whistle");
    json old = toJson(v);
    old.erase("instrument");  // songs saved before instruments existed
    assert(fromJson(old).instrument == "whistle");
    v.instrument = "violin";
    assert(fromJson(toJson(v)).instrument == "violin" && instrumentOf(v).name == "Violin");
    Song odd = v;
    odd.instrument = "kazoo";
    assert(instrumentOf(odd).name == "D whistle");

    const int sr = 48000, n = 2048;  // written A4 on a violin sounds A4 = 440 Hz (a whistle would sound 880)
    Performance pv = perform(v, 60, 0, false);
    assert(pv.octave == 0);
    Synth syn;
    syn.metronome = false;
    long long sp = sr / 2;
    std::vector<float> w(n);
    syn.render(pv, sp, w.data(), n);
    assert(std::abs(detectPitch(w.data(), n, sr, 200, 3000) - 440) < 3);

    Song low = importAbc("X:1\nT:l\nM:4/4\nL:1/4\nK:C\nG,4|F,4|\n").song;  // G3 is a violin's lowest note, F3 is below it
    low.instrument = "violin";
    std::string an = analyze(low), ls = listing(low);
    assert(an.find("Range on the Violin (playable G3 .. E7)") != std::string::npos && an.find("current: 1 out of range\n") != std::string::npos);
    assert(an.find("half") == std::string::npos && an.find("phrasing hints") != std::string::npos);
    assert(ls.find("instrument: Violin") != std::string::npos && ls.find("out-of-range") != std::string::npos);
    low.instrument = "piano";
    assert(analyze(low).find("current: 0 out of range") != std::string::npos);

    // harmonica keys (#67): the C harp is unchanged; G..B harps sit below C, Db..F# above
    Instrument ch = *findInstrument("harmonica");
    assert(ch.name == "Harmonica" && ch.low == 60 && ch.high == 96 && ch.octave == 0);
    assert(findInstrument("harmonica", "D", "C")->name == "Harmonica");
    Instrument ah = *findInstrument("harmonica", "D", "A");
    assert(ah.name == "A harmonica" && ah.low == 57 && ah.high == 93 && ah.octave == 0);  // starts on A3
    assert(findInstrument("harmonica", "D", "G")->low == 55 && findInstrument("harmonica", "D", "B")->low == 59);
    assert(findInstrument("harmonica", "D", "Db")->low == 61 && findInstrument("harmonica", "D", "F#")->low == 66);
    for (auto& k : HARP_KEYS) assert(findInstrument("harmonica", "D", k));
    assert(!findInstrument("harmonica", "D", "H") && !findInstrument("harmonica", "D", "C#"));
    assert(findInstrument("violin", "D", "A")->name == "Violin");  // the harp key only matters for the harmonica
    Song hs = v;
    hs.instrument = "harmonica";
    assert(!toJson(hs).contains("harp") && fromJson(toJson(hs)).harp == "C");  // C harp songs are saved as before
    hs.harp = "A";
    assert(toJson(hs)["harp"] == "A" && instrumentOf(fromJson(toJson(hs))).name == "A harmonica");
    hs.harp = "H";
    assert(instrumentOf(hs).name == "D whistle");  // unknown key: falls back like a bad whistle key
  }

  // play in another key (#67): smallest interval, then the octave that fits the instrument; back = the original
  {
    Song s = importAbc("X:1\nT:k\nM:4/4\nL:1/8\nK:D\nDEFG ABcd|\n").song;  // D4..D5 on a D whistle
    const Song orig = s;
    std::vector<int> before;
    for (auto& e : s.events) if (e.kind == Kind::Note) before.push_back(e.midi);
    assert(tonicPc(s.key) == 2 && tonicPc(*parseKey("Bm")) == 11 && tonicPc(*parseKey("Ador")) == 9);
    assert(transposeToKey(s, 7, instrumentOf(s)) == 5 && keyName(s.key) == "G major");    // G4..G5: up a fourth
    assert(transposeToKey(s, 9, instrumentOf(s)) == 2 && keyName(s.key) == "A major");
    assert(transposeToKey(s, 2, instrumentOf(s)) == -7 && keyName(s.key) == "D major");   // +5 would reach D6
    assert(toJson(s) == toJson(orig) && !toJson(s).contains("transposed"));
    assert(transposeToKey(s, 2, instrumentOf(s)) == 0);
    transposeToKey(s, 11, instrumentOf(s));  // B: -3 (B3 is out) or +9
    assert(s.transposed == 9 && toJson(s)["transposed"] == 9 && fromJson(toJson(s)).transposed == 9);
    transpose(s, -12);  // a manual transposition (update_song) adds to it too
    assert(s.transposed == -3);
    Song m = importAbc("X:1\nT:m\nM:4/4\nL:1/8\nK:Edor\nEFGA Bcde|\n").song;
    transposeToKey(m, 2, instrumentOf(m));
    assert(keyName(m.key) == "D dorian" && m.events[0].midi == 62);  // the mode stays
    Song h = importAbc("X:1\nT:h\nM:4/4\nL:1/8\nK:A\nA,B,CD EFGA|\n").song;  // A3..A4: the bottom of an A harp
    h.instrument = "harmonica";
    h.harp = "A";
    const Song hOrig = h;
    assert(transposeToKey(h, 7, instrumentOf(h)) == 10 && keyName(h.key) == "G major");  // down 2 would leave G3, G#3 out
    // back to A: +2 and -10 both fit; the tie goes to the original octave, so the sheet is exactly the original
    assert(transposeToKey(h, 9, instrumentOf(h)) == -10 && toJson(h) == toJson(hOrig));
    for (int k = 0; k < 12; k++) transposeToKey(h, k, instrumentOf(h));  // a walk through every key and back
    transposeToKey(h, 9, instrumentOf(h));
    assert(toJson(h) == toJson(hOrig));
    h.harp = "C";
    assert(transposeToKey(h, 9, instrumentOf(h)) == 12);  // a C harp starts on C4: the octave up fits
  }

  // playback sounds: every voice keeps the song's pitch, struck/plucked voices decay, no NaN or clipping
  {
    const int sr = 48000, n = 2048;
    Song v = importAbc("X:1\nT:v\nM:4/4\nL:1/4\nK:D\nA4|\n").song;  // 4 s whole note
    v.instrument = "violin";                                         // written A4 sounds 440 Hz
    auto window = [&](const std::vector<float>& x, double at) { return x.data() + (int)(at * sr); };
    for (auto& id : INSTRUMENTS) {
      Performance p = perform(v, 60, 0, false, id);
      assert(p.sound == id && p.octave == 0);
      Synth syn;
      syn.metronome = false;
      syn.volume = 1;
      long long sp = 0;
      std::vector<float> x(sr * 9 / 2);
      syn.render(p, sp, x.data(), (int)x.size());
      for (float f : x) assert(std::isfinite(f) && std::abs(f) < 1);
      float hz = detectPitch(window(x, 0.15), n, sr, 100, 3000);
      if (std::abs(hz - 440) >= 3) fprintf(stderr, "%s: %.1f Hz\n", id.c_str(), hz);
      assert(std::abs(hz - 440) < 3);
      float early = rms(window(x, 0.1), n), late = rms(window(x, 2.5), n);
      bool decays = id == "piano" || id == "guitar" || id == "electric-guitar";
      assert(early > 0.05f && (decays ? late < 0.6f * early : late > 0.8f * early));
      assert(rms(window(x, 4.2), n) < 0.01f);  // silent after the note
    }
    Song w = v;  // the octave comes from the song's instrument, not the sound: a whistle song on guitar still sounds A5
    w.instrument = "whistle";
    Performance pg = perform(w, 60, 0, false, "guitar");
    assert(pg.octave == 1 && perform(w, 60, 0, false, "kazoo").sound == "whistle" && perform(w, 60).sound == "whistle");
    Synth syn;
    syn.metronome = false;
    long long sp = sr / 10;
    std::vector<float> x(n);
    syn.render(pg, sp, x.data(), n);
    assert(std::abs(detectPitch(x.data(), n, sr) - 880) < 5);

    // which ornaments each voice plays (kb/notes/playback-voice-ornaments.md)
    Song o = importAbc("X:1\nT:o\nM:4/4\nL:1/4\nK:D\n{g}A !breath! B !slide!c !vibrato!d|\n").song;
    auto on = [&](const char* id) { return perform(o, 60, 0, false, id).voices; };
    auto wh = on("whistle"), vn = on("violin"), pn = on("piano"), gt = on("guitar"), hm = on("harmonica");
    assert(wh[0].graces.size() == 1 && wh[0].gap > 0.1 && wh[2].graces.size() == 1 && wh[3].vibrato);
    assert(vn[0].graces.size() == 1 && vn[0].gap == 0 && vn[2].graces.size() == 1 && vn[3].vibrato);  // no breaths on a bow
    assert(pn[0].graces.empty() && pn[0].gap < 0.02 && pn[2].graces.empty() && !pn[3].vibrato);      // no ornaments on keys
    assert(gt[0].graces.size() == 1 && gt[2].graces.size() == 1 && !gt[3].vibrato);                  // hammer-ons, slides
    assert(hm[0].graces.empty() && hm[0].gap > 0.1 && hm[2].graces.size() == 1 && hm[3].vibrato);    // breaths, bends
  }

  // transpose D major down a tone -> C major
  Song c = s;
  transpose(c, -2);
  assert(c.key.fifths == 0 && nth(c, 0).midi == 67);

  // delete bars: pickup is bar 0, closing repeat moves back, ties into deleted notes are dropped
  {
    auto t = importAbc("X:1\nT:del\nM:4/4\nL:1/4\nK:D\nA | d e f g | a b c' d'- | d' c' b a :|\n").song;
    Song u = t;
    assert(deleteBars(u, 3, 3) == 4);
    assert(u.events.back().kind == Kind::Bar && u.events.back().bar == ":|");
    assert(!nth(u, 8).tie && nth(u, 8).midi == 86);
    u = t;
    assert(deleteBars(u, 0, 0) == 1 && u.events.front().kind == Kind::Note && u.events.front().midi == 74);
    bool threw = false;
    try { deleteBars(u, 9, 9); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    threw = false;
    try { u = t; deleteBars(u, 0, 3); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    // the bar line opening a 1st ending keeps its volta mark when the bar before it goes
    auto v = importAbc("X:1\nT:volta\nM:2/4\nL:1/4\nK:D\n|: d e | f g |1 a b :|2 c' d' |]\n").song;
    assert(deleteBars(v, 2, 2) == 2);
    assert(playOrder(v).size() == 8);  // d e a b d e c' d'
  }

  // transcription: John's synth playing a tune over a louder piano-like accompaniment, then back to notes
  {
    auto tune = importAbc("X:1\nT:t\nM:4/4\nL:1/8\nK:G\nd2 B2 G2 B2|d2 g2 f2 e2|d2 B2 c2 A2|G4 z4|"
                          "g2 f2 e2 d2|e2 c2 A2 c2|B2 d2 A2 F2|G8|]\n").song;
    const int sr = 22050;
    Performance perf = perform(tune, 100, 0, false);
    std::vector<float> mix((size_t)((perf.length + 1) * sr), 0.f);
    Synth syn;
    syn.sampleRate = sr;
    syn.metronome = false;
    long long pos = 0;
    syn.render(perf, pos, mix.data(), (int)mix.size());
    float melodyRms = rms(mix.data(), (int)mix.size());
    // accompaniment: a decaying chord + bass per bar (G G C G | C C D G), 6 harmonics each
    const int chords[8][4] = {{43, 55, 59, 62}, {43, 55, 59, 62}, {48, 55, 60, 64}, {43, 55, 59, 62},
                              {48, 55, 60, 64}, {48, 55, 60, 64}, {50, 57, 62, 66}, {43, 55, 59, 62}};
    std::vector<float> acc(mix.size(), 0.f);
    const double barSec = 2.4;  // 4 beats at 100 bpm
    for (int b = 0; b < 8; b++)
      for (int beat = 0; beat < 4; beat++)
        for (int m : chords[b]) {
          double f0 = 440 * std::pow(2, (m - 69) / 12.0), t0 = b * barSec + beat * 0.6;
          for (size_t i = (size_t)(t0 * sr); i < (size_t)((t0 + 0.6) * sr) && i < acc.size(); i++) {
            double t = (double)i / sr - t0, v = 0;
            for (int h = 1; h <= 6; h++) v += std::sin(2 * M_PI * f0 * h * t) / h;
            acc[i] += (float)(v * std::exp(-3 * t));
          }
        }
    float accRms = rms(acc.data(), (int)acc.size());
    for (size_t i = 0; i < mix.size(); i++) mix[i] += acc[i] * 0.5f * melodyRms / accRms;  // 6 dB under the melody, like a lead over a band
    auto melodyOf = [](const Song& s) {
      std::vector<int> out;  // played pitches; tied continuations merged
      bool tied = false;
      for (auto& e : s.events) {
        if (e.kind == Kind::Bar) continue;
        if (e.kind == Kind::Note && !tied) out.push_back(e.midi);
        tied = e.kind == Kind::Note && e.tie;
      }
      return out;
    };
    auto want = melodyOf(tune);
    auto common = [](const std::vector<int>& a, const std::vector<int>& b) {  // longest common subsequence length
      std::vector<std::vector<int>> L(a.size() + 1, std::vector<int>(b.size() + 1, 0));
      for (size_t i = 1; i <= a.size(); i++)
        for (size_t j = 1; j <= b.size(); j++)
          L[i][j] = a[i - 1] == b[j - 1] ? L[i - 1][j - 1] + 1 : std::max(L[i - 1][j], L[i][j - 1]);
      return L[a.size()][b.size()];
    };
    {  // for a violin the melody is written at the pitch it sounds: an octave above the whistle's written notes
      TranscribeOptions o;
      o.bpm = 100;
      o.instrument = "violin";
      Transcription tr = transcribe(mix, sr, o);
      std::vector<int> up = want;
      for (int& m : up) m += 12;
      auto got = melodyOf(tr.song);
      assert(tr.song.instrument == "violin" && common(up, got) >= (int)up.size() * 9 / 10);
      assert(tr.report.find("Violin plays at the recording's pitch") != std::string::npos);
      bool threw = false;
      o.instrument = "kazoo";
      try { transcribe(mix, sr, o); } catch (const std::runtime_error&) { threw = true; }
      assert(threw);
    }
    {  // a melody narrower than an octave fits the whistle twice: take the low octave, easier for beginners
      Song narrow = importAbc("X:1\nT:n\nM:4/4\nL:1/4\nK:D\nd e f g|a g f e|d4|]\n").song;  // written D5..A5
      Performance pn = perform(narrow, 100, 0, false);
      std::vector<float> x((size_t)((pn.length + 1) * sr), 0.f);
      Synth sn;
      sn.sampleRate = sr;
      sn.metronome = false;
      long long at = 0;
      sn.render(pn, at, x.data(), (int)x.size());
      TranscribeOptions o;
      o.bpm = 100;
      std::vector<int> low = melodyOf(narrow);
      for (int& m : low) m -= 12;
      auto got = melodyOf(transcribe(x, sr, o).song);
      assert(common(low, got) >= (int)low.size() - 1);
    }
    {  // D minor from low C to high A fits no octave of the D whistle (low C is 19% of the time): transposed +2 to
       // E minor before folding (#9). transpose = 0 keeps the key and folds low C up; transpose = 4 is used as given.
      Song dm = importAbc("X:1\nT:m\nM:4/4\nL:1/4\nK:Dm\nC D F A|d f a f|e c A F|C4|D F A d|f e d c|A F E C|D4|]\n").song;
      Performance pm = perform(dm, 100, 0, false);
      std::vector<float> x((size_t)((pm.length + 1) * sr), 0.f);
      Synth sm;
      sm.sampleRate = sr;
      sm.metronome = false;
      long long at = 0;
      sm.render(pm, at, x.data(), (int)x.size());
      auto shifted = [&](int semis, bool foldC) {
        std::vector<int> v = melodyOf(dm);
        for (int& m : v) m += m == 60 && foldC ? 12 : semis;
        return v;
      };
      TranscribeOptions o;
      o.bpm = 100;
      Transcription tr = transcribe(x, sr, o);
      Song heard = tr.song;
      transpose(heard, -2);
      std::string line = tr.report.substr(tr.report.find("transposed"));
      printf("transcribe (D minor, low C to high A): %s", line.substr(0, line.find('\n') + 1).c_str());
      assert(melodyOf(tr.song) == shifted(2, false));
      assert(tr.report.find("\ntransposed +2 semitones (" + keyName(heard.key) + " -> " + keyName(tr.song.key) +
                            ") to fit the D whistle: no octave kept the notes in range\n") != std::string::npos);
      assert(tr.report.find("\nkey: " + keyName(heard.key) + " (fit") != std::string::npos && tr.report.find("moved an octave") == std::string::npos);
      o.transpose = 0;
      tr = transcribe(x, sr, o);
      assert(melodyOf(tr.song) == shifted(0, true) && tr.report.find("transposed") == std::string::npos);
      assert(tr.report.find("\n3 note(s) moved an octave") != std::string::npos);
      o.transpose = 4;
      tr = transcribe(x, sr, o);
      assert(melodyOf(tr.song) == shifted(4, false) && tr.report.find("\ntransposed +4 semitones (") != std::string::npos &&
             tr.report.find("), as given\n") != std::string::npos);
    }
    {  // notes of irregular lengths have no steady beat: the tempo is flagged as uncertain, with a runner-up
      Song odd = importAbc("X:1\nT:o\nM:4/4\nL:1/16\nK:D\nd3 e5 f2 g7 a3 b6 a2 g5 f3 e7 d2 e4 f9 g3 a5 b2 a6 g3 f7 e2 d5|]\n").song;
      Performance po = perform(odd, 100, 0, false);
      std::vector<float> x((size_t)((po.length + 1) * sr), 0.f);
      Synth so;
      so.sampleRate = sr;
      so.metronome = false;
      long long at = 0;
      so.render(po, at, x.data(), (int)x.size());
      std::string r = transcribe(x, sr, {}).report;
      printf("transcribe (irregular): %s", r.substr(0, r.find('\n') + 1).c_str());
      assert(r.find("confidence high") == std::string::npos && r.find("also plausible: ~") != std::string::npos);
    }
    {  // the meter is detected: a jig is 6/8 (a beat of three eighths), a waltz 3/4 (#1); the 4/4 mix stays 4/4 (below)
      const std::pair<const char*, const char*> tunes[] = {
          {"X:1\nT:j\nM:6/8\nL:1/8\nK:D\nd2f a2f|g2e c2A|dfa afd|e3 e2A|d2f a2f|g2e c2e|dcB AFA|d3 d3|]\n", "6/8"},
          {"X:1\nT:w\nM:3/4\nL:1/8\nK:D\nd4 f2|a4 f2|g4 e2|c6|d4 f2|a3 g f2|e2 d2 c2|d6|]\n", "3/4"}};
      for (auto [abc, meter] : tunes) {
        Song t = importAbc(abc).song;
        Performance pt = perform(t, 150, 0, false);
        std::vector<float> x((size_t)((pt.length + 1) * sr), 0.f);
        Synth st;
        st.sampleRate = sr;
        st.metronome = false;
        long long at = 0;
        st.render(pt, at, x.data(), (int)x.size());
        Transcription tr = transcribe(x, sr, {});
        std::string r = tr.report.substr(tr.report.find("meter: "));
        printf("transcribe (%s tune): %s", meter, r.substr(0, r.find('\n') + 1).c_str());
        assert(tr.song.meter == meter && r.rfind("meter: " + std::string(meter) + " (detected; " + meter + " ", 0) == 0);
        assert(common(melodyOf(t), melodyOf(tr.song)) >= (int)melodyOf(t).size() * 9 / 10);
        TranscribeOptions o;
        o.meter = "4/4";
        tr = transcribe(x, sr, o);
        assert(tr.song.meter == "4/4" && tr.report.find("\nmeter: 4/4, as given\n") != std::string::npos);
      }
    }
    {  // notes that cross a beat are split into tied values, the middle of a 4/4 bar stays visible; notes on a beat
       // spanning whole beats (half on beat 1 or 3, dotted half, whole, dotted quarter in 6/8) stay single (#3)
      auto written = [](const Song& s) {
        std::vector<std::tuple<int, int, bool>> out;  // pitch, length, tie
        for (auto& e : s.events) if (e.kind == Kind::Note) out.push_back({e.midi, e.len, e.tie});
        return out;
      };
      auto abcOf = [](const std::string& meter, const char* body) { return "X:1\nT:s\nL:1/8\nM:" + meter + "\nK:D\n" + body; };
      // 6 eighths a bar, read as 6/8 and as 3/4; narrow, so the whistle takes the low octave
      const char* jig = "d2 e2 f2|g3 a3|b2 a4|f6|d e2 f2 g|a3 g2 e|d6|]\n";
      const std::tuple<const char*, const char*, const char*> tunes[] = {
          {"4/4", "A2 d4 A2|d3 e3 f2|g8|f4 e4|d e2 f2 g2 a|b6 g2|e2 d6|f8|]\n",
           "A2 d2- d2 A2|d3 e- e2 f2|g8|f4 e4|d e- e f- f g- g a|b6 g2|e2 d2- d4|f8|]\n"},
          {"6/8", jig, "D2 E- E F2|G3 A3|B2 A- A3|F6|D E2 F2 G|A3 G2 E|D6|]\n"},
          {"3/4", jig, "D2 E2 F2|G3 A- A2|B2 A4|F6|D E- E F- F G|A3 G- G E|D6|]\n"}};
      for (auto [meter, abc, want] : tunes) {
        Song t = importAbc(abcOf(meter, abc)).song;
        Performance ps = perform(t, 100, 0, false);
        std::vector<float> x((size_t)((ps.length + 1) * sr), 0.f);
        Synth ss;
        ss.sampleRate = sr;
        ss.metronome = false;
        long long at = 0;
        ss.render(ps, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = 100;
        o.meter = meter;
        assert(written(transcribe(x, sr, o).song) == written(importAbc(abcOf(meter, want)).song));
      }
    }
    {  // eighth triplets come back as (3 groups among straight eighths and sixteenths; swung eighths (2:1) come back
       // straight, with a report line (#2)
      auto written = [](const Song& s) {
        std::vector<std::tuple<int, int, bool, int, bool>> out;  // pitch, length, tie, tuplet, tuplet start
        for (auto& e : s.events) if (e.kind == Kind::Note) out.push_back({e.midi, e.len, e.tie, e.tuplet, e.tupletStart});
        return out;
      };
      auto render = [&](const Song& t, double bpm) {
        Performance p = perform(t, bpm, 0, false);
        std::vector<float> x((size_t)((p.length + 1) * sr), 0.f);
        Synth sy;
        sy.sampleRate = sr;
        sy.metronome = false;
        long long at = 0;
        sy.render(p, at, x.data(), (int)x.size());
        return x;
      };
      Song trip = importAbc("X:1\nT:t\nM:4/4\nL:1/8\nK:D\nD2 (3EFG A2 (3GFE|D2 F2 (3GAB A2|(3BAG (3FED E4|D E F/G/A (3Bcd e2|]\n").song;
      TranscribeOptions o;
      o.bpm = 100;
      o.meter = "4/4";
      Transcription tr = transcribe(render(trip, 100), sr, o);
      std::string r = tr.report;
      size_t at = r.find("triplets: 6 beat(s) written as eighth triplets (3\n");
      assert(at != std::string::npos && r.find("rhythm:") == std::string::npos);
      printf("transcribe (triplets): %s", r.substr(at, r.find('\n', at) + 1 - at).c_str());
      assert(written(tr.song) == written(trip));
      std::string abc = toAbc(tr.song);  // written lengths in the group: (3E2 F2 G2
      assert(std::regex_search(abc, std::regex(R"(\(3=?[A-Ga-g]2 =?[A-Ga-g]2 =?[A-Ga-g]2 )")));
      assert(written(importAbc(abc).song) == written(tr.song));

      // the same straight eighths played long-short
      Song even = importAbc("X:1\nT:s\nM:4/4\nL:1/8\nK:D\nD E F G A B c d|e d c B A G F E|D F A d c A F D|E G B d e4|]\n").song;
      Song swung = even;
      for (size_t i = 0, tick = 0; i + 1 < swung.events.size(); tick += swung.events[i++].len)
        if (swung.events[i].len == 24 && tick % 48 == 0 && swung.events[i + 1].len == 24) swung.events[i].len = 32, swung.events[i + 1].len = 16;
      tr = transcribe(render(swung, 100), sr, o);
      r = tr.report;
      at = r.find("rhythm: swung eighths, about ");
      assert(at != std::string::npos);
      printf("transcribe (swing): %s", r.substr(at, r.find('\n', at) + 1 - at).c_str());
      double ratio = std::atof(r.c_str() + at + 29);
      assert(ratio > 1.7 && ratio < 2.3 && written(tr.song) == written(even) && r.find("triplets:") == std::string::npos);
    }
    {  // a ritardando and accelerando (80 -> 50 -> 80 bpm): with rubato the note values come back, without it they
       // don't (#11)
      Song t = importAbc("X:1\nT:r\nM:4/4\nL:1/8\nK:D\nD2 F2 A2 d2|c2 A2 B2 G2|F A d c B2 A2|G2 E2 F4|"
                         "A2 d2 f2 e2|d c B A G2 F2|E2 A2 c2 e2|d8|f2 e2 d2 c2|B A G F E2 D2|"
                         "F2 A2 d2 A2|B4 A4|G B d B A F D F|E2 G2 c2 e2|d2 c2 B2 c2|d8|]\n").song;
      Performance p = perform(t, 60, 0, false);  // a beat a second, then warped: tempo 65 - 15 cos(2 pi beat / beats)
      const double beats = p.length;
      auto at = [&](double b) {  // seconds at beat b
        double s = 0;
        for (int i = 0; i < 1000; i++) {
          double u = b * (i + 0.5) / 1000;
          s += b / 1000 * 60 / (65 + 15 * std::cos(2 * M_PI * u / beats));
        }
        return s;
      };
      for (auto& v : p.voices) {
        double end = at(v.start + v.dur);
        v.gap = end - at(v.start + v.dur - v.gap);
        v.start = at(v.start);
        v.dur = end - v.start;
      }
      p.length = at(beats);
      std::vector<float> x((size_t)((p.length + 1) * sr), 0.f);
      Synth sy;
      sy.sampleRate = sr;
      sy.metronome = false;
      long long pos = 0;
      sy.render(p, pos, x.data(), (int)x.size());
      auto lengths = [](const Song& s) {  // (pitch, length) of each played note, tied pieces merged
        std::vector<std::pair<int, int>> out;
        bool tied = false;
        for (auto& e : s.events) {
          if (e.kind != Kind::Note) continue;
          if (tied) out.back().second += e.len;
          else out.push_back({e.midi, e.len});
          tied = e.tie;
        }
        return out;
      };
      auto want = lengths(t);
      auto same = [&](const Song& s) {  // notes with the right pitch and length, in order (longest common subsequence)
        auto got = lengths(s);
        std::vector<std::vector<int>> L(want.size() + 1, std::vector<int>(got.size() + 1, 0));
        for (size_t i = 1; i <= want.size(); i++)
          for (size_t j = 1; j <= got.size(); j++)
            L[i][j] = want[i - 1] == got[j - 1] ? L[i - 1][j - 1] + 1 : std::max(L[i - 1][j], L[i][j - 1]);
        return L[want.size()][got.size()];
      };
      TranscribeOptions o;
      o.meter = "4/4";
      Transcription steady = transcribe(x, sr, o);
      o.rubato = true;
      Transcription rub = transcribe(x, sr, o);
      std::string r = rub.report.substr(0, rub.report.find('\n') + 1);
      printf("transcribe (rubato): %d of %zu notes right, %d without rubato; %s", same(rub.song), want.size(), same(steady.song),
             r.c_str());
      assert(lengths(rub.song) == want && same(steady.song) < (int)want.size() * 3 / 4);
      assert(steady.report.find("confidence low") != std::string::npos && steady.report.find("re-run with rubato: true\n") != std::string::npos);
      int med = 0, lo = 0, hi = 0;  // the tempo line: the median and the range of the beats
      assert(sscanf(r.c_str(), "tempo: %d bpm (quarter notes, the median); follows the recording: %d-%d bpm (rubato)\n", &med, &lo, &hi) == 3);
      assert(med >= 60 && med <= 70 && lo >= 45 && lo <= 55 && hi >= 75 && hi <= 85);
    }
    {  // a short low note that starts a wide leap (D up a tenth) is melody, not accompaniment: keep it (#7)
      Song leap = importAbc("X:1\nT:l\nM:4/4\nL:1/8\nK:D\nD f a f b a f a|D f g a b2 a2|D f a f g f e f|D f a g f4|]\n").song;
      Performance pl = perform(leap, 120, 0, false);
      std::vector<float> x((size_t)((pl.length + 1) * sr), 0.f);
      Synth sl;
      sl.sampleRate = sr;
      sl.metronome = false;
      long long at = 0;
      sl.render(pl, at, x.data(), (int)x.size());
      TranscribeOptions o;
      o.bpm = 120;
      auto got = melodyOf(transcribe(x, sr, o).song), want = melodyOf(leap);
      assert(std::count(got.begin(), got.end(), 62) == 4);
      assert(common(want, got) == (int)want.size() && got.size() == want.size());
    }
    {  // the pitch search reaches E7: a violin line up to D7/E7 and the whistle's top C#7 come back at pitch (#40)
      const std::pair<const char*, const char*> tunes[] = {
          {"X:1\nT:v\nM:4/4\nL:1/8\nK:D\nd'2 f'2 a'2 d''2|e''2 d''2 c''2 b'2|a'2 b'2 c''2 d''2|e''4 d''4|]\n", "violin"},
          {"X:1\nT:w\nM:4/4\nL:1/8\nK:D\nD4 A4|d2 f2 a2 b2|c'2 b2 a2 f2|g2 a2 b2 c'2|b4 a4|]\n", "whistle"}};
      for (auto [abc, inst] : tunes) {
        Song hi = importAbc(abc).song;
        hi.instrument = inst;
        Performance ph = perform(hi, 100, 0, false);
        std::vector<float> x((size_t)((ph.length + 1) * sr), 0.f);
        Synth sh;
        sh.sampleRate = sr;
        sh.metronome = false;
        long long at = 0;
        sh.render(ph, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = 100;
        o.instrument = inst;
        assert(melodyOf(transcribe(x, sr, o).song) == melodyOf(hi));
      }
    }
    {  // ornaments the synth plays (cuts and taps between repeated notes, long rolls) come back as techniques on the
       // main notes, with no grace notes left in the melody; only a whistle draft marks them, the report lists them (#12)
      Song orn = importAbc("X:1\nT:o\nM:4/4\nL:1/8\nK:D\nd2 !cut!d2 B2 !tap!B2|~A3 B A2 F2|G2 !cut!G2 !tap!G2 F2|~D3 E F4|]\n").song;
      auto notes = [](const Song& s) {
        std::vector<std::tuple<int, int, std::vector<std::string>>> out;  // pitch, length, techniques; ties merged
        bool tied = false;
        for (auto& e : s.events) {
          if (e.kind != Kind::Note) continue;
          if (tied) std::get<1>(out.back()) += e.len;
          else out.push_back({e.midi, e.len, e.techniques});
          tied = e.tie;
        }
        return out;
      };
      const char* where = "2 cuts (bar 1 beat 2, bar 3 beat 2); 2 taps (bar 1 beat 4, bar 3 beat 3); 2 rolls (bar 2 beat 1, bar 4 beat 1)";
      for (double bpm : {100.0, 120.0}) {
        Performance po = perform(orn, bpm, 0, false);
        std::vector<float> x((size_t)((po.length + 1) * sr), 0.f);
        Synth so;
        so.sampleRate = sr;
        so.metronome = false;
        long long at = 0;
        so.render(po, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = bpm;
        o.meter = "4/4";
        Transcription tr = transcribe(x, sr, o);
        assert(notes(tr.song) == notes(orn));
        assert(tr.report.find("\nornaments (candidates, check by ear): " + std::string(where) + "; marked in the ABC\n") != std::string::npos);
        assert(notes(importAbc(toAbc(tr.song)).song) == notes(orn));  // the draft ABC carries them to import_abc
        // a roll right after a cut on the same pitch, not re-articulated: split where the roll starts
        Song cutRoll = importAbc("X:1\nT:c\nM:4/4\nL:1/8\nK:D\nd2 !cut!d2 ~d3 B|A2 !cut!A2 ~A3 F|]\n").song;
        Performance pc = perform(cutRoll, bpm, 0, false);
        std::vector<float> y((size_t)((pc.length + 1) * sr), 0.f);
        Synth sc;
        sc.sampleRate = sr;
        sc.metronome = false;
        at = 0;
        sc.render(pc, at, y.data(), (int)y.size());
        assert(notes(transcribe(y, sr, o).song) == notes(cutRoll));
        if (bpm != 100) continue;
        o.instrument = "violin";
        tr = transcribe(x, sr, o);
        for (auto& e : tr.song.events) assert(e.techniques.empty());
        assert(tr.report.find(std::string(where) + "; not marked: the ABC is for the Violin\n") != std::string::npos);
      }
    }
    {  // piano notes fade under the voicing threshold before the next one: sustain them, no note + rest pairs (#6);
       // a melody with real rests keeps them
      auto played = [](const Song& s) {
        std::vector<std::tuple<Kind, int, int>> out;  // notes and rests (kind, pitch, length); ties and rests merged
        bool tied = false;
        for (auto& e : s.events) {
          if (e.kind == Kind::Bar) continue;
          if (!out.empty() && (tied || (e.kind == Kind::Rest && std::get<0>(out.back()) == Kind::Rest))) std::get<2>(out.back()) += e.len;
          else out.push_back({e.kind, e.midi, e.len});
          tied = e.kind == Kind::Note && e.tie;
        }
        if (std::get<0>(out.back()) == Kind::Rest) out.pop_back();  // the draft fills its last bar with a rest
        return out;
      };
      const std::pair<const char*, double> tunes[] = {{"X:1\nT:p\nM:4/4\nL:1/4\nK:G\nd2 B2|G4|A2 B c|d4|]\n", 40},
                                                      {"X:1\nT:r\nM:4/4\nL:1/8\nK:G\nd2 z2 B2 z2|G2 B2 d2 z2|g4 z4|e2 z2 c2 z2|]\n", 80}};
      for (auto [abc, bpm] : tunes) {
        Song t = importAbc(abc).song;
        Performance pp = perform(t, bpm, 0, false, "piano");
        std::vector<float> x((size_t)((pp.length + 1) * sr), 0.f);
        Synth sp;
        sp.sampleRate = sr;
        sp.metronome = false;
        long long at = 0;
        sp.render(pp, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = bpm;
        assert(played(transcribe(x, sr, o).song) == played(t));
      }
    }
    {  // the same whistle tune over a louder low accompaniment: only a search limited to the whistle's range keeps it (#10)
      std::vector<float> loud(mix.size());
      for (size_t i = 0; i < mix.size(); i++) loud[i] = mix[i] + acc[i] * 2 * melodyRms / accRms;  // 8 dB over the melody
      TranscribeOptions o;
      o.bpm = 100;
      auto got0 = melodyOf(transcribe(loud, sr, o).song);
      o.recordingInstrument = "whistle";
      Transcription tr = transcribe(loud, sr, o);
      auto got = melodyOf(tr.song);
      int all = common(want, got0), lim = common(want, got);
      printf("transcribe (loud accompaniment): unlimited %d/%zu notes, %zu extra; limited to the whistle %d/%zu, %zu extra\n", all,
             want.size(), got0.size() - all, lim, want.size(), got.size() - lim);
      // the limited search still hears the chords' harmonics that fall in the whistle's range (low D, G in rests)
      assert(all < (int)want.size() * 7 / 10 && lim >= (int)want.size() * 9 / 10 && got.size() - lim < (got0.size() - all) / 2);
      assert(tr.report.find("pitch search: limited to the D whistle's sounding range, C5..D#7\n") != std::string::npos);
      bool threw = false;
      o.recordingInstrument = "kazoo";
      try { transcribe(loud, sr, o); } catch (const std::runtime_error&) { threw = true; }
      assert(threw);
    }
    for (double bpm : {100.0, 0.0}) {
      TranscribeOptions o;
      o.bpm = bpm;
      Transcription tr = transcribe(mix, sr, o);
      auto got = melodyOf(tr.song);
      int hit = common(want, got);  // how many of the tune's notes came back, in order
      printf("transcribe (bpm %s): %d/%zu notes found, %zu extra, tempo %d, key %s\n", bpm > 0 ? "given" : "auto", hit,
             want.size(), got.size() - hit, tr.song.tempo, keyName(tr.song.key).c_str());
      assert(hit >= (int)want.size() * 9 / 10 && got.size() - hit <= want.size() / 10);
      assert(std::abs(tr.song.tempo - 100) <= 4);
      assert(tr.report.find(bpm > 0 ? "bpm (quarter notes), as given\n" : "bpm (quarter notes), confidence high\n") != std::string::npos);
      assert(tr.song.key.fifths == 1);
      assert(tr.song.meter == "4/4" && tr.report.find("\nmeter: 4/4 (detected; 4/4 ") != std::string::npos);
      assert(tr.report.find("triplets:") == std::string::npos && tr.report.find("rhythm:") == std::string::npos);  // straight (#2)
      // the per-frame pitch track comes back too, and its beat times put it under the right notes
      assert(tr.track.points.size() > mix.size() / 300 && tr.track.sync.size() > 20);
      for (auto& [tick, sec] : tr.track.sync) assert(std::abs(trackTick(tr.track, sec) - tick) < 1e-6);
      printf("transcribe (bpm %s): pitch track %zu frames, %.0f%% on its notes\n", bpm > 0 ? "given" : "auto", tr.track.points.size(),
             aligned(tr.song, tr.track) * 100);
      assert(aligned(tr.song, tr.track) >= 0.85);
      // the ABC we hand the agent reads back to the same song
      Song back = importAbc(toAbc(tr.song)).song;
      assert(back.events.size() == tr.song.events.size());
      for (size_t i = 0; i < back.events.size(); i++)
        assert(back.events[i].midi == tr.song.events[i].midi && back.events[i].len == tr.song.events[i].len &&
               back.events[i].tie == tr.song.events[i].tie);
    }
    {  // Import audio: a WAV through the same core function as the viewer gives a new song and its pitch track
      fs::create_directories(home);
      fs::path wav = home / "my tune.wav";
      writeWav(wav.string(), mix, sr);
      Song s = importAudio(wav.string(), 0, 0, {});
      auto saved = loadSong(s.id);
      assert(saved && saved->id == "my-tune" && saved->title == "my tune" && saved->source == "my tune.wav" && saved->folder.empty());
      assert(saved->notes.rfind("Automatic transcription\ntempo: ", 0) == 0 && saved->notes.find("\nmeter: 4/4") != std::string::npos &&
             saved->notes.find("\nkey: G major") != std::string::npos);
      assert(common(want, melodyOf(*saved)) >= (int)want.size() * 9 / 10);
      // notes the recording is unclear about: only the tune's rest in bar 4 (beats 3-4), which the transcription fills
      // with the accompaniment's G, and in the second import below also the faint bars 7 and 8
      auto unclearBars = [](const Song& x, int firstBar) {
        std::set<std::pair<int, bool>> r;  // (bar of the tune, in its second half)
        auto p = positions(x);
        for (size_t i = 0; i < x.events.size(); i++)
          if (x.events[i].comment == "unclear in the recording, check by ear") r.insert({p[i].bar + firstBar, p[i].tick >= 96});
        return r;
      };
      auto t = loadTrack(s.id);
      assert(t && t->points.size() > mix.size() / 300 && t->sync.size() > 20);
      printf("import audio: '%s', %zu notes, pitch track %.0f%% on its notes\n", s.id.c_str(), melodyOf(*saved).size(), aligned(*saved, *t) * 100);
      assert(aligned(*saved, *t) >= 0.85);
      for (auto [bar, late] : unclearBars(*saved, 0)) assert(bar == 4 && late);
      // a second import gets its own id; start/end trim the recording (here: the first two bars, 4.8 s, are left out).
      // The last two bars are made faint: their notes get the "check by ear" comment.
      std::vector<float> faint = mix;
      for (size_t i = (size_t)(6 * 2.4 * sr); i < faint.size(); i++) faint[i] *= 0.3f;
      writeWav(wav.string(), faint, sr);
      Song s2 = importAudio(wav.string(), 4.8, 0, {});
      assert(s2.id == "my-tune-2" && loadTrack("my-tune-2") && melodyOf(s2).size() < melodyOf(*saved).size());
      auto u2 = unclearBars(s2, 2);
      for (auto [bar, late] : u2) assert(bar >= 7 || (bar == 4 && late));
      assert(u2.count({7, false}) && u2.count({7, true}) && u2.count({8, false}));
      bool threw = false;
      try { importAudio((home / "missing.wav").string(), 0, 0, {}); } catch (const std::runtime_error&) { threw = true; }
      assert(threw && !loadTrack("missing") && !loadSong("missing"));
      fs::remove_all(home / "tracks");
      fs::remove(home / "songs" / "my-tune.json");
      fs::remove(home / "songs" / "my-tune-2.json");
    }
    {  // separate (#5): Demucs runs on the section and its stem is transcribed; without it the full mix, and the report says why
      fs::create_directories(home);
      fs::path wav = home / "dense mix.wav";
      writeWav(wav.string(), mix, sr);
      TranscribeOptions plain;
      plain.bpm = 100;
      Transcription ref = transcribeFile(wav.string(), 0, 0, plain);
      TranscribeOptions o = plain;
      o.separate = "vocals";
      auto sameAsMix = [&](const Transcription& t, const std::string& line) {
        assert(t.report == "separation: " + line + "; transcribed the full mix\n" + ref.report);
        assert(toAbc(t.song) == toAbc(ref.song));
      };
      std::string path = getenv("PATH") ? getenv("PATH") : "";
      setenv("PATH", home.string().c_str(), 1);  // no demucs here, even where one is installed
      unsetenv("JOHN_DEMUCS");
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "demucs not found (pip install demucs, or set JOHN_DEMUCS)");
      // Import audio (#69) copies the separation line into the song's notes, so the viewer shows why the full mix was used
      Song imp = importAudio(wav.string(), 0, 0, o);
      assert(imp.notes.rfind("Automatic transcription\nseparation: demucs not found (pip install demucs, or set JOHN_DEMUCS); "
                             "transcribed the full mix\ntempo: ", 0) == 0);
      fs::remove_all(home / "tracks");
      fs::remove(home / "songs" / (imp.id + ".json"));
      setenv("PATH", path.c_str(), 1);
      std::string missing = (home / "no demucs").string();
      setenv("JOHN_DEMUCS", missing.c_str(), 1);
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "JOHN_DEMUCS (" + missing + ") can't be run");
#ifndef _WIN32  // the stub is a shell script
      // a stub in a folder with a space, that writes another tune where Demucs puts the stem
      Song other = importAbc("X:1\nT:b\nM:4/4\nL:1/8\nK:D\nA2 F2 D2 F2|A2 d2 c2 B2|A2 F2 G2 E2|D8|]\n").song;
      Performance po = perform(other, 100, 0, false);
      std::vector<float> stem((size_t)((po.length + 1) * sr), 0.f);
      Synth so;
      so.sampleRate = sr;
      so.metronome = false;
      long long at = 0;
      so.render(po, at, stem.data(), (int)stem.size());
      fs::path dir = home / "demucs stub";
      fs::create_directories(dir);
      writeWav((dir / "stem.wav").string(), stem, sr);
      fs::path stub = dir / "demucs";
      {
        std::ofstream f(stub);
        f << "#!/bin/sh\n"
             "echo 'stub noise on stdout'; echo 'stub noise on stderr' >&2\n"
             "[ -n \"$STUB_EXIT\" ] && exit \"$STUB_EXIT\"\n"
             "for a; do case $a in --two-stems=*) stem=${a#--two-stems=};; esac; done\n"
             "here=$(dirname \"$0\"); echo \"$4\" > \"$here/input\"\n"
             "[ -n \"$STUB_NOSTEM\" ] && exit 0\n"
             "mkdir -p \"$3/htdemucs/$(basename \"$4\" .wav)\"\n"
             "cp \"$here/stem.wav\" \"$3/htdemucs/$(basename \"$4\" .wav)/$stem.wav\"\n";
      }
      fs::permissions(stub, fs::perms::owner_all, fs::perm_options::add);
      setenv("JOHN_DEMUCS", stub.string().c_str(), 1);
      Transcription t = transcribeFile(wav.string(), 0, 0, o);
      assert(t.report.rfind("separation: transcribed the demucs vocals stem\ntempo: ", 0) == 0);
      Song imp2 = importAudio(wav.string(), 0, 0, o);
      assert(imp2.notes.rfind("Automatic transcription\nseparation: transcribed the demucs vocals stem\ntempo: ", 0) == 0);
      fs::remove_all(home / "tracks");
      fs::remove(home / "songs" / (imp2.id + ".json"));
      int fromStem = common(melodyOf(other), melodyOf(t.song)), fromMix = common(want, melodyOf(t.song));
      printf("separate (stub): %d/%zu notes of the stem, %d/%zu of the mix\n", fromStem, melodyOf(other).size(), fromMix, want.size());
      assert(fromStem >= (int)melodyOf(other).size() * 9 / 10 && fromMix < (int)want.size() / 2);
      std::string input;
      std::getline(std::ifstream(dir / "input"), input);
      assert(fs::path(input).filename() == "section.wav" && !fs::exists(fs::path(input).parent_path()));  // temp files removed
      setenv("STUB_NOSTEM", "1", 1);  // real Demucs exits 0 when it can't read a track
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "demucs wrote no vocals.wav (stub noise on stderr)");
      unsetenv("STUB_NOSTEM");
      setenv("STUB_EXIT", "3", 1);
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "demucs failed with exit status 3 (stub noise on stderr)");
      unsetenv("STUB_EXIT");
#endif
      unsetenv("JOHN_DEMUCS");
      fs::remove(wav);
    }
  }

  // decoding: a 16-bit WAV of A5 written by hand, read back resampled to 22050 Hz mono
  {
    fs::create_directories(home);
    fs::path wav = home / "a5.wav";
    const int rate = 44100, n = rate / 2;
    std::vector<float> a5(n);
    for (int i = 0; i < n; i++) a5[i] = (float)(12000 / 32767.0 * std::sin(2 * M_PI * 880 * i / rate));
    writeWav(wav.string(), a5, rate);
    auto x = decodeAudio(wav.string(), 22050);
    assert(std::abs((int)x.size() - 22050 / 2) < 100);
    assert(std::abs(detectPitch(x.data() + 2000, 2048, 22050) - 880) < 5);
    // a non-ASCII path arrives as UTF-8 (JSON); Windows reads it as UTF-8 only with src/utf8.manifest
    fs::copy_file(wav, home / fs::path(u8"l\u00e1.wav"));
    assert(decodeAudio(home.string() + "/l\xc3\xa1.wav", 22050).size() == x.size());
    bool threw = false;
    try { decodeAudio((home / "missing.mp3").string(), 22050); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);
  }

  // analysis hints
  std::string an = analyze(s);
  assert(an.find("long roll") != std::string::npos);
  assert(an.find("cut (or tongue)") != std::string::npos);
  assert(an.find("tongue (phrase start") != std::string::npos);
  auto low = importAbc("X:1\nT:low\nM:4/4\nL:1/8\nK:C\nC2 D2 E4|\n").song;
  assert(analyze(low).find("OUT OF RANGE") != std::string::npos);

  // folders: saved only when set, standard ones first in course order, custom A-Z, unsorted last
  {
    Song f = s;
    assert(!toJson(f).contains("folder"));
    f.folder = "Intermediate";
    assert(fromJson(toJson(f)).folder == "Intermediate");
    std::vector<std::string> fs_ = {"", "Zebra", "Advanced", "Celtic", "Lessons", "Beginner"};
    std::sort(fs_.begin(), fs_.end(), folderBefore);
    assert((fs_ == std::vector<std::string>{"Lessons", "Beginner", "Advanced", "Celtic", "Zebra", ""}));
  }

  // listing and JSON round trip through disk
  s.id = "test-jig";
  saveSong(s);
  auto back = loadSong("test-jig");
  assert(back && toJson(*back) == toJson(s));
  assert(listing(s).find("XXO OOO") != std::string::npos);
  assert(!loadSong("../etc"));
  assert(listSongs() == std::vector<std::string>{"test-jig"});

  // #68: every X: tune of a multi-tune file becomes a song with `like`'s instrument and source and a unique id; the file
  // header's fields apply to each tune, empty tunes are reported, text without a tune is an error
  {
    Song like;
    like.instrument = "violin";
    like.source = "tunes.abc";
    const char* abc = "%abc-2.1\nL:1/4\n\nX:1\nT:Same\nK:G\nGABc|\n\nX:2\nT:Same\nM:3/4\nK:D\nd2e|f3|\nX:3\nT:Nothing\nK:D\n";
    auto r = importSongs(abc, like);
    assert(r.size() == 2 && r[0].song.id == "same" && r[1].song.id == "same-2");
    auto a1 = loadSong("same"), a2 = loadSong("same-2");
    assert(a1 && a2 && a1->instrument == "violin" && a2->source == "tunes.abc" && a2->meter == "3/4");
    assert(a1->events[0].len == WHOLE / 4 && a2->key.fifths == 2);  // L:1/4 from the file header
    assert(std::find(r[0].warnings.begin(), r[0].warnings.end(), "Nothing: no K: line or no notes, not imported") != r[0].warnings.end());
    like.id = "mine";
    assert(importSongs("T:Pasted\nK:D\nDEF|", like)[0].song.id == "mine");  // no X:: the whole text is one tune
    bool threw = false;
    try { importSongs("just some words\n", like); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);
    for (auto id : {"same", "same-2", "mine"}) fs::remove(dataDir() / "songs" / (std::string(id) + ".json"));
  }

  // #73: MusicXML. A score written by hand gives the same events as the same tune in ABC: repeat, grace cut, triplet,
  // rest, tie, 1st/2nd endings (divisions 6 per quarter: eighth 3, triplet eighth 2)
  {
    auto score = [](const std::string& attrs, const std::string& measures, const std::string& moreParts = "") {
      return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" "
             "\"http://www.musicxml.org/dtds/partwise.dtd\">\n<score-partwise version=\"4.0\"><work><work-title>Test Jig</work-title></work>"
             "<identification><creator type=\"composer\">Trad.</creator></identification><part-list><score-part id=\"P1\"/></part-list>"
             "<part id=\"P1\"><measure number=\"1\"><attributes>" + attrs + "</attributes>" + measures + "</part>" + moreParts + "</score-partwise>";
    };
    auto n = [](const char* step, int oct, int dur, const std::string& extra = "", int alter = 0) {
      return std::string("<note><pitch><step>") + step + "</step>" + (alter ? "<alter>" + std::to_string(alter) + "</alter>" : "") +
             "<octave>" + std::to_string(oct) + "</octave></pitch><duration>" + std::to_string(dur) + "</duration><voice>1</voice>" + extra + "</note>";
    };
    const std::string trip = "<time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>";
    const std::string jigAttrs = "<divisions>6</divisions><key><fifths>2</fifths></key><time><beats>6</beats><beat-type>8</beat-type></time>";
    const std::string jig =
        "<barline location=\"left\"><repeat direction=\"forward\"/></barline>" + n("A", 4, 9) +
        "<note><grace/><pitch><step>G</step><octave>5</octave></pitch><voice>1</voice></note>" + n("A", 4, 3) + n("F", 4, 3, "", 1) + n("A", 4, 3) +
        "</measure><measure number=\"2\">" + n("E", 5, 2, trip + "<notations><tuplet type=\"start\"/></notations>") + n("F", 5, 2, trip, 1) +
        n("G", 5, 2, trip + "<notations><tuplet type=\"stop\"/></notations>") + n("A", 5, 9) + "<note><rest/><duration>3</duration><voice>1</voice></note>" +
        "</measure><measure number=\"3\"><barline location=\"left\"><ending number=\"1\" type=\"start\"/></barline>" + n("D", 5, 9, "<tie type=\"start\"/>") +
        n("D", 5, 6, "<tie type=\"stop\"/>") +
        "<barline location=\"right\"><ending number=\"1\" type=\"stop\"/><repeat direction=\"backward\"/></barline></measure>"
        "<measure number=\"4\"><barline location=\"left\"><ending number=\"2\" type=\"start\"/></barline>" + n("D", 5, 9) + n("D", 5, 6) + n("B", 4, 3) +
        "<barline location=\"right\"><bar-style>light-light</bar-style><ending number=\"2\" type=\"discontinue\"/></barline></measure>";
    auto x = importMusicXml(score(jigAttrs, jig));
    Song abc = importAbc("X:1\nT:Test Jig\nM:6/8\nL:1/8\nK:D\n|:A3 {g}AFA|(3efg a3 z|1 d3- d2:|2 d3 d2B||\n").song;
    assert(x.warnings.empty() && x.song.title == "Test Jig" && x.song.source == "Trad." && x.song.meter == "6/8");
    assert(x.song.key.fifths == 2 && x.song.key.mode == "maj");
    assert(toJson(x.song)["events"] == toJson(abc)["events"]);
    assert(playOrder(x.song) == playOrder(abc));

    // key with a mode, tempo, breath mark, a chord (top note kept), voice 2 and a second part dropped, a key change
    // ignored, a length off John's grid (divisions 5: a fifth of a quarter is 9.6 ticks), all with warnings
    auto w = importMusicXml(score("<divisions>5</divisions><key><fifths>2</fifths><mode>dorian</mode></key><time symbol=\"common\"><beats>4</beats><beat-type>4</beat-type></time>",
        "<direction><direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>90</per-minute></metronome></direction-type></direction>" +
        n("E", 4, 5, "<notations><articulations><breath-mark/></articulations></notations>") + n("G", 4, 5) + n("B", 4, 5, "<chord/>") +
        n("C", 5, 4, "", 1) + n("D", 5, 1) + n("E", 5, 5) + "<backup><duration>20</duration></backup>" +
        "<note><pitch><step>C</step><octave>3</octave></pitch><duration>20</duration><voice>2</voice></note></measure>"
        "<measure number=\"2\"><attributes><key><fifths>-1</fifths></key></attributes>" + n("E", 4, 20) + "</measure>",
        "<part id=\"P2\"><measure number=\"1\"><attributes><divisions>1</divisions></attributes>" + n("C", 2, 4) + "</measure></part>"));
    const Song& ws = w.song;
    assert(ws.key.fifths == 2 && ws.key.mode == "dor" && keyName(ws.key) == "E dorian" && ws.meter == "C" && ws.tempo == 90);
    assert(nth(ws, 0).techniques == std::vector<std::string>{"breath"} && nth(ws, 1).midi == 71 && nth(ws, 2).midi == 73);
    assert(nth(ws, 0).len == 48 && nth(ws, 3).len == 10 && nth(ws, 5).midi == 64 && nth(ws, 5).len == WHOLE);
    for (auto& e : ws.events) assert(e.kind != Kind::Note || e.midi > 60);  // nothing from voice 2 or part 2
    auto has = [&](const char* m) { return std::find(w.warnings.begin(), w.warnings.end(), m) != w.warnings.end(); };
    assert(has("chords reduced to their top note") && has("only the first voice was imported, other voices dropped"));
    assert(has("only the first part was imported, 1 other part(s) dropped") && has("key change ignored (one key per song)"));
    assert(has("note lengths that don't fit John's timing grid (1/192 of a whole note) were rounded") && w.warnings.size() == 5);

    // .mxl: zipped, the score in a folder named by META-INF/container.xml; importSongs() saves it like ABC
    const std::string xml = score(jigAttrs, jig);
    mz_zip_archive z{};
    assert(mz_zip_writer_init_heap(&z, 0, 0));
    const std::string container = "<?xml version=\"1.0\"?><container><rootfiles><rootfile full-path=\"music/jig.musicxml\"/></rootfiles></container>";
    assert(mz_zip_writer_add_mem(&z, "mimetype", "application/vnd.recordare.musicxml", 34, MZ_NO_COMPRESSION));
    assert(mz_zip_writer_add_mem(&z, "META-INF/container.xml", container.data(), container.size(), MZ_DEFAULT_COMPRESSION));
    assert(mz_zip_writer_add_mem(&z, "music/jig.musicxml", xml.data(), xml.size(), MZ_DEFAULT_COMPRESSION));
    void* buf = nullptr;
    size_t size = 0;
    assert(mz_zip_writer_finalize_heap_archive(&z, &buf, &size));
    const std::string mxl((const char*)buf, size);
    mz_free(buf);
    mz_zip_writer_end(&z);
    assert(isMusicXml(mxl) && isMusicXml(xml) && !isMusicXml("X:1\nT:a\nK:D\nABc|"));
    assert(toJson(importMusicXml(mxl).song) == toJson(x.song));
    Song like;
    like.instrument = "violin";
    like.source = "jig.mxl";
    auto saved = importSongs(mxl, like);
    assert(saved.size() == 1 && saved[0].song.id == "test-jig-2");  // "test-jig" is taken
    auto back = loadSong("test-jig-2");
    assert(back && back->instrument == "violin" && back->source == "jig.mxl" && toJson(*back)["events"] == toJson(abc)["events"]);
    fs::remove(dataDir() / "songs" / "test-jig-2.json");
    for (const std::string& bad : {std::string("PK\x03\x04 not a zip"), std::string("<score-timewise/>"), score(jigAttrs, "</measure>")}) {
      bool threw = false;
      try { importMusicXml(bad); } catch (const std::runtime_error&) { threw = true; }
      assert(threw);
    }
  }

  // lessons: installed once, a deleted lesson stays deleted, and every bundled lesson loads
  {
    fs::path src = home / "lesson-src";
    fs::create_directories(src);
    Song l = s;
    l.id = "lesson-test";
    { std::ofstream f(src / "lesson-test.json"); f << toJson(l).dump(); }
    assert(installSongs(src) == 1 && loadSong("lesson-test"));
    assert(installSongs(src) == 0);
    fs::remove(dataDir() / "songs" / "lesson-test.json");
    assert(installSongs(src) == 0 && !loadSong("lesson-test"));
    // each bundled folder has its own marker: a "demo" folder installs even though lesson-src was done
    fs::path demo = home / "demo";
    fs::create_directories(demo);
    l.id = "demo-test";
    { std::ofstream f(demo / "demo-test.json"); f << toJson(l).dump(); }
    assert(installSongs(demo) == 1 && loadSong("demo-test") && fs::exists(dataDir() / "demo-installed.txt"));
    fs::remove(dataDir() / "songs" / "demo-test.json");
    assert(installSongs(demo) == 0 && !loadSong("demo-test"));
    // #47: the title, notes and every comment have a pt-BR translation (re-translate after editing the English)
    auto translated = [](const Song& b) {
      auto has = [&](const std::string& t) {
        auto l = b.i18n.find("pt-BR");
        bool ok = l != b.i18n.end() && l->second.count(t) && !l->second.at(t).empty();
        if (!ok) fprintf(stderr, "%s: no pt-BR for \"%s\"\n", b.id.c_str(), t.c_str());
        return ok;
      };
      bool ok = has(b.title) && has(b.notes);
      for (auto& ev : b.events) ok &= ev.comment.empty() || has(ev.comment);
      return ok;
    };
    int bundled = 0;
    for (auto& e : fs::directory_iterator(lessonsDir())) {
      std::ifstream f(e.path());
      Song b = fromJson(json::parse(f));
      assert(!b.events.empty() && !b.notes.empty() && translated(b));
      for (auto& ev : b.events) assert(ev.kind != Kind::Note || finger(b.whistle, ev.midi).ok);  // all playable
      bundled++;
    }
    assert(bundled >= 8);
    // release packages keep lessons/ and fonts/ next to the executable; dev builds use the build-time paths
#ifdef _WIN32
    assert(john::bundled("john-tests.exe", "/built") != "/built");  // this test binary sits next to itself
#else
    assert(john::bundled("john-tests", "/built") != "/built");  // this test binary sits next to itself
#endif
    assert(john::bundled("no-such-file", "/built") == "/built");
    assert(lessonsDir().filename() == "lessons");  // installSongs keeps using lessons-installed.txt
    // demo/ (web starter songs): every song loads, has notes, a source and a folder, and fits its instrument
    int demos = 0;
    for (auto& e : fs::directory_iterator(lessonsDir().parent_path() / "demo")) {
      std::ifstream f(e.path());
      Song b = fromJson(json::parse(f));
      assert(!b.events.empty() && !b.notes.empty() && !b.source.empty() && !b.folder.empty() && translated(b));
      assert(findInstrument(b.instrument, b.whistle));
      for (auto& ev : b.events) assert(ev.kind != Kind::Note || instrumentOf(b).plays(ev.midi));
      demos++;
    }
    assert(demos >= 12);
  }

  saveCustomTechnique({"bounce", "Bounce", "b", "test"});
  bool found = false;
  for (auto& x : loadTechniques()) found |= x.id == "bounce";
  assert(found);

  // viewer language: English until the GUI sets one (the MCP server never does), then pt-BR with fallback
  {
    Event dq{Kind::Note, 62, WHOLE * 3 / 8}, trip{Kind::Note, 62, WHOLE / 12, false, 3, 2};
    assert(!strcmp(tr("Play"), "Play") && keyName({2, "maj"}) == "D major" && durationName(dq) == "dotted quarter");
    assert(defaultTechniques()[0].name == "Cut");
    // song translations (#47): s.i18n round-trips, English until a language is set, exact-English match only
    Song ts = s;
    ts.title = "Hello";
    ts.i18n["pt-BR"]["Hello"] = "Olá";
    assert(fromJson(toJson(ts)).i18n == ts.i18n && fromJson(toJson(s)).i18n.empty());
    assert(songText(ts, ts.title) == "Hello");
    assert(setLanguage("pt-BR"));
    assert(songText(ts, ts.title) == "Olá" && songText(ts, "Hello!") == "Hello!");  // edited English: shown as is
    assert(!strcmp(tr("Play"), "Tocar") && tr(std::string("Settings")) == "Configurações");
    const char* missing = "No such text";
    assert(tr(missing) == missing);  // fallback: the English itself
    assert(trf("%s (song)", "Piano") == "Piano (da música)");
    assert(keyName({2, "maj"}) == "D maior" && keyName({1, "dor"}) == "A dórico" && keyName({3, "min"}) == "F# menor");
    assert(durationName(dq) == "semínima pontuada" && durationName(trip) == "colcheia (quiáltera de 3)");
    assert(durationName({Kind::Note, 62, WHOLE * 7 / 8}) == "mínima com dois pontos");
    assert(defaultTechniques()[0].name == "Corte (cut)");
    setSolfege(false, true);  // keys as Do Re Mi, note names letters
    assert(keyName({2, "maj"}) == "Ré maior" && keyName({-2, "maj"}) == "Sib maior" && keyName({4, "min"}) == "Dó# menor");
    assert(tonicName(*parseKey("D")) == "Ré" && tonicName(*parseKey("Bb")) == "Sib");  // the whistle's key
    assert(noteName(spell(d, 66)) == "F#4");
    setSolfege(true, false);  // note names Do Re Mi, keys letters
    assert(keyName({2, "maj"}) == "D maior" && tonicName(*parseKey("D")) == "D" && noteName(spell(d, 66)) == "Fá#4");
    assert(!setLanguage("en"));
    setSolfege(true, true);
    assert(keyName({2, "maj"}) == "D major" && tonicName(*parseKey("D")) == "D");  // English keys are always letters
    setSolfege(false, false);
    assert(!strcmp(tr("Play"), "Play") && keyName({2, "maj"}) == "D major");
    assert(!setLanguage("xx"));  // no such file: English
    assert(!strcmp(tr("Play"), "Play") && songText(ts, ts.title) == "Hello");

    // automatic language: LC_ALL/LANG matched to a file in i18n/, else English
    auto sys = [](const char* v) { setenv("LC_ALL", v, 1); return systemLanguage(); };
    assert(sys("pt_BR.UTF-8") == "pt-BR" && sys("pt_PT") == "pt-BR" && sys("de_DE.UTF-8") == "en" && sys("C") == "en");
    unsetenv("LC_ALL");
    // the matching behind it, also used for the macOS preferred language when LANG/LC_ALL are empty (#54)
    assert(matchLanguage("pt-BR") == "pt-BR" && matchLanguage("pt_BR") == "pt-BR" && matchLanguage("pt-PT") == "pt-BR");
    assert(matchLanguage("en-US") == "en" && matchLanguage("") == "en" && matchLanguage("zh-Hans-CN") == "en");
    bool listed = false;
    for (auto& [code, name] : languages()) listed |= code == "pt-BR" && name == "Português (Brasil)";
    assert(listed);

    // pt-BR.json: parses, no empty values, the same printf conversions as the English key
    std::ifstream f(i18nDir() / "pt-BR.json");
    json j = json::parse(f);  // throws on a syntax error
    auto specs = [](const std::string& t) {
      static const std::regex conv("%[-+ #0]*[0-9]*(\\.[0-9]+)?[a-zA-Z]");
      std::string r;
      for (std::sregex_iterator it(t.begin(), t.end(), conv), end; it != end; ++it) r += it->str();
      return r;
    };
    for (auto& [k, v] : j.items()) {
      if (!v.is_string() || v.get<std::string>().empty() || specs(k) != specs(v.get<std::string>()))
        fprintf(stderr, "pt-BR.json: bad value for \"%s\"\n", k.c_str());
      assert(v.is_string() && !v.get<std::string>().empty() && specs(k) == specs(v.get<std::string>()));
    }
    // every literal tr("..."), trf("...") and L("...") in the viewer and core has a Portuguese entry
    static const std::regex call("\\b(?:tr|trf|L)\\(\"((?:[^\"\\\\]|\\\\.)*)\"");
    for (const char* file : {"gui_main.cpp", "song.cpp"}) {
      std::ifstream in(i18nDir().parent_path() / "src" / file);
      std::string src((std::istreambuf_iterator<char>(in)), {});
      assert(!src.empty());
      for (std::sregex_iterator it(src.begin(), src.end(), call), end; it != end; ++it) {
        std::string key, lit = (*it)[1];
        for (size_t i = 0; i < lit.size(); i++) key += lit[i] == '\\' && ++i < lit.size() ? (lit[i] == 'n' ? '\n' : lit[i]) : lit[i];
        if (!j.contains(key)) fprintf(stderr, "pt-BR.json: missing \"%s\" (%s)\n", key.c_str(), file);
        assert(j.contains(key));
      }
    }
  }

  // settings: no file -> defaults; round trip; missing keys -> defaults; garbage -> all defaults
  {
    auto write = [&](const char* text) { std::ofstream(home / "settings.json") << text; };
    assert(loadSettings() == Settings{});
    Settings s;
    s.hints = false; s.zoom = 12; s.light = true; s.tempo = 90; s.countIn = false; s.metronome = false;
    s.volume = 0.3f; s.sound = "piano"; s.gate = 0.02f; s.latencyMs = 120; s.tolerance = 25; s.instrument = "violin"; s.songFilter = "piano";
    s.language = "pt-BR"; s.solfege = true; s.solfegeKeys = false; s.harp = "A";
    saveSettings(s);
    assert(loadSettings() == s);
    write(R"({"tempo": 80, "future_setting": 1})");
    Settings d;
    d.tempo = 80;
    assert(loadSettings() == d);
    write("{not json");
    assert(loadSettings() == Settings{});
    write(R"({"hints": "yes"})");  // wrong type
    assert(loadSettings() == Settings{});
    write(R"({"sound": "banjo", "instrument": "kazoo", "songFilter": "lute", "zoom": 0, "volume": 9, "harp": "H"})");  // values the GUI can't use
    Settings c = loadSettings();
    assert(c.sound.empty() && c.instrument == "whistle" && c.songFilter.empty() && c.zoom == 6 && c.volume == 1 && c.harp == "C");
    fs::remove(home / "settings.json");
  }

  fs::remove_all(home);
  puts("core tests passed");
}
