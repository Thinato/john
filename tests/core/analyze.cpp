// Core tests: play in key, playability, analysis hints.
#include "test.hpp"

  // play in another key (#67): smallest interval, then the octave that fits the instrument; back = the original
TEST(play_in_key) {
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
    // A3..E5 from the bottom of an A harp, blow and draw notes only (#85: D and F# there are bends, so the low octave
    // of "A,B,CD EFGA" would no longer tie with the octave up)
    Song h = importAbc("X:1\nT:h\nM:4/4\nL:1/8\nK:A\nA,B,CE GAce|\n").song;
    h.instrument = "harmonica";
    h.harp = "A";
    const Song hOrig = h;
    assert(transposeToKey(h, 7, instrumentOf(h)) == 10 && keyName(h.key) == "G major");  // down 2 would leave G3 out
    // back to A: +2 and -10 both fit; the tie goes to the original octave, so the sheet is exactly the original
    assert(transposeToKey(h, 9, instrumentOf(h)) == -10 && toJson(h) == toJson(hOrig));
    for (int k = 0; k < 12; k++) transposeToKey(h, k, instrumentOf(h));  // a walk through every key and back
    transposeToKey(h, 9, instrumentOf(h));
    assert(toJson(h) == toJson(hOrig));
    h.harp = "C";
    assert(transposeToKey(h, 9, instrumentOf(h)) == 12);  // a C harp starts on C4: the octave up fits
}

  // playability (#84): fits as written, in another key (or octave), or not at all, on any instrument
TEST(playability) {
    auto song = [](const char* body) { return importAbc(std::string("X:1\nT:p\nM:4/4\nL:1/8\n") + body).song; };
    const Instrument dw = *findInstrument("whistle", "D");
    Playability p = playability(song("K:D\nDEFG ABcd|\n"), dw);  // D4..D5
    assert(p.verdict == Playability::AsWritten && p.shift == 0 && p.notes == 8 && keyName(p.key) == "D major");
    assert(playability(song("K:D\nDEFG ABcd|\n"), *findInstrument("violin")).verdict == Playability::AsWritten);
    // hard notes: one half-hole (F natural) in 11 notes is under 10 %, in 10 notes it isn't
    assert(playability(song("K:D\nDEFG ABcd e=Fd|\n"), dw).verdict == Playability::AsWritten);
    assert(playability(song("K:D\nDEFG ABcd e=F|\n"), dw).verdict != Playability::AsWritten);
    assert(fits(RangeCost{}, 0) && !fits(RangeCost{1, 0, 0}, 100) && fits(RangeCost{0, 0, 4}, 9) && !fits(RangeCost{0, 0, 5}, 9));
    // F major: three half-holes; G major (+2, one cross-fingered C) is the nearest key that fits
    p = playability(song("K:F\nFGAB cdef|\n"), dw);
    assert(p.verdict == Playability::OtherKey && p.shift == 2 && keyName(p.key) == "G major" && p.cost.out == 0 && p.cost.cross == 1);
    Song moved = song("K:F\nFGAB cdef|\n");
    transpose(moved, p.shift);  // the GUI's one click
    assert(playability(moved, dw).verdict == Playability::AsWritten);
    // below the whistle: the same key an octave up, before any other key
    p = playability(song("K:G\nG,A,B,D|\n"), dw);
    assert(p.verdict == Playability::OtherKey && p.shift == 12 && keyName(p.key) == "G major");
    // D4 to E6 spans more than the whistle's 23 semitones: no key fits, one note stays out
    p = playability(song("K:D\nDAe'|\n"), dw);
    assert(p.verdict == Playability::NoFit && p.cost.out == 1 && p.notes == 3);
    // the whistle's key is the instrument's, not the song's: C4..C5 fits a C whistle as written, not a D whistle
    Song c = song("K:C\nCDEF GABc|\n");
    assert(c.whistle == "D" && playability(c, *findInstrument("whistle", "C")).verdict == Playability::AsWritten);
    p = playability(c, dw);
    assert(p.verdict == Playability::OtherKey && keyName(p.key) == "D major" && p.shift == 2);
}

  // analysis hints
TEST(analysis_hints) {
  Song s = importAbc(JIG).song;
  std::string an = analyze(s);
  assert(an.find("long roll") != std::string::npos);
  assert(an.find("cut (or tongue)") != std::string::npos);
  assert(an.find("tongue (phrase start") != std::string::npos);
  auto low = importAbc("X:1\nT:low\nM:4/4\nL:1/8\nK:C\nC2 D2 E4|\n").song;
  assert(analyze(low).find("OUT OF RANGE") != std::string::npos);
}
