// Core self-checks: key parsing, spelling, fingerings, ABC import, transpose, analysis, JSON round trip.
#include "john.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>

using namespace john;

static const Event& nth(const Song& s, int n) {  // n-th note/rest (skipping bars)
  for (auto& e : s.events) if (e.kind != Kind::Bar && n-- == 0) return e;
  assert(false);
  return s.events[0];
}

int main() {
  fs::path home = fs::temp_directory_path() / ("john-test-" + std::to_string(getpid()));
  setenv("JOHN_HOME", home.c_str(), 1);

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
    assert(std::abs(writtenMidiFromFreq(hz) - 69) < 0.2);
    std::vector<float> quiet(n, 0.0f);
    assert(detectPitch(quiet.data(), n, sr) == 0 && rms(quiet.data(), n) == 0);
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

  // analysis hints
  std::string an = analyze(s);
  assert(an.find("long roll") != std::string::npos);
  assert(an.find("cut (or tongue)") != std::string::npos);
  assert(an.find("tongue (phrase start") != std::string::npos);
  auto low = importAbc("X:1\nT:low\nM:4/4\nL:1/8\nK:C\nC2 D2 E4|\n").song;
  assert(analyze(low).find("OUT OF RANGE") != std::string::npos);

  // listing and JSON round trip through disk
  s.id = "test-jig";
  saveSong(s);
  auto back = loadSong("test-jig");
  assert(back && toJson(*back) == toJson(s));
  assert(listing(s).find("XXO OOO") != std::string::npos);
  assert(!loadSong("../etc"));
  assert(listSongs() == std::vector<std::string>{"test-jig"});

  saveCustomTechnique({"bounce", "Bounce", "b", "test"});
  bool found = false;
  for (auto& x : loadTechniques()) found |= x.id == "bounce";
  assert(found);

  fs::remove_all(home);
  puts("core tests passed");
}
