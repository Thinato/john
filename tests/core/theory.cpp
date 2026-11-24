// Core tests: keys, spelling, fingerings, score layout, positions, transpose, delete bars.
#include "test.hpp"

TEST(keys_spelling_fingerings) {

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
}

  // score layout: grand staff for piano (below middle C on the bass staff), room grows only for far notes
TEST(score_layout) {
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
}

  // pickup bar is bar 0; positions follow elapsed time
TEST(pickup_positions) {
  auto pk = importAbc("X:1\nT:p\nM:4/4\nL:1/8\nK:Edor\n|:D2|EBBA B2EB|]\n").song;
  auto ps = positions(pk);
  assert(ps[1].bar == 0 && ps[1].tick == WHOLE * 3 / 4);  // D2 pickup
  assert(ps[3].bar == 1 && ps[3].tick == 0);                // first E
}

  // transpose D major down a tone -> C major
TEST(transpose_down_a_tone) {
  Song s = importAbc(JIG).song;
  Song c = s;
  transpose(c, -2);
  assert(c.key.fifths == 0 && nth(c, 0).midi == 67);
}

  // delete bars: pickup is bar 0, closing repeat moves back, ties into deleted notes are dropped
TEST(delete_bars) {
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
