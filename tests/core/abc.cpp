// Core tests: ABC import: notes, ornaments, voltas, K: options, multi-tune files, lyrics.
#include "test.hpp"

  // ABC: roll, grace cut, tuplet, rest, tie, voltas, key signature
TEST(abc_import) {
  auto r = importAbc(JIG);
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
}

  // broken rhythm and accidentals that last for the bar
TEST(broken_rhythm_and_accidentals) {
  auto b = importAbc("X:1\nT:b\nM:4/4\nL:1/8\nK:G\nA>B c<d|\n").song;
  assert(nth(b, 0).len == 36 && nth(b, 1).len == 12 && nth(b, 2).len == 12 && nth(b, 3).len == 36);
  auto a = importAbc("X:1\nT:a\nM:4/4\nL:1/8\nK:D\n=F F f2 | F4|\n").song;
  assert(nth(a, 0).midi == 65 && nth(a, 1).midi == 65 && nth(a, 2).midi == 78 && nth(a, 3).midi == 66);
  auto acc = accidentals(a);
  assert(acc[0].shown && acc[0].alter == 0 && !acc[1].shown);
}

  // thesession.org bare body: no headers, "!" line breaks
TEST(thesession_bare_body) {
  auto t = importAbc("A2FA A2dB|A2FA BEE2|! A2FA A2Bd|egfd edBd|");
  assert(t.warnings == std::vector<std::string>{"no K: field, assumed D major"} && nth(t.song, 1).midi == 66 && t.song.title == "Untitled");
}

  // review regressions: header comments, volta at start, zero/huge lengths, odd meters, voltas restart time
TEST(review_regressions) {
  auto hc = importAbc("%abc-2.1\nX:1\nT:G jig\n% comment\nM:6/8\nL:1/8\nK:G\nGAB cBc|\n");
  assert(hc.song.key.fifths == 1 && hc.song.meter == "6/8" && nth(hc.song, 3).midi == 72);
  // K: clef and octave options (#81): clef names are not part of the key; octave= moves the written notes, clef +8/-8 doesn't
  auto kb = importAbc("X:1\nK:G bass\nG,|\n");
  assert(kb.warnings.empty() && kb.song.key.fifths == 1 && kb.song.key.mode == "maj" && nth(kb.song, 0).midi == 55);
  auto kc = importAbc("X:1\nK:Am clef=bass3 transpose=-2 middle=d\nA,|\n");
  assert(kc.warnings.empty() && kc.song.key.fifths == 0 && kc.song.key.mode == "min" && nth(kc.song, 0).midi == 57);
  auto ko = importAbc("X:1\nK:D clef=bass octave=-1\nD d ^c|\n");
  assert(ko.warnings.empty() && nth(ko.song, 0).midi == 50 && nth(ko.song, 1).midi == 62 && nth(ko.song, 2).midi == 61);
  auto k8 = importAbc("X:1\nK:D clef=bass-8\nD|\n");
  assert(k8.warnings.empty() && nth(k8.song, 0).midi == 62);
  k8 = importAbc("X:1\nK:D treble+8\nD|\n");
  assert(k8.warnings.empty() && k8.song.key.fifths == 2 && nth(k8.song, 0).midi == 62);
  auto ku = importAbc("X:1\nK:D clef=banjo\nD|\n");
  assert(ku.song.key.fifths == 2 && ku.warnings == std::vector<std::string>{"unknown clef 'banjo', ignored"} && nth(ku.song, 0).midi == 62);
  Song kr = importAbc(toAbc(ko.song)).song;  // stored pitches already carry the shift; toAbc writes no octave=
  assert(nth(kr, 0).midi == 50 && nth(kr, 1).midi == 62 && nth(kr, 2).midi == 61);
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
}

  // #68: every X: tune of a multi-tune file becomes a song with `like`'s instrument and source and a unique id; the file
  // header's fields apply to each tune, empty tunes are reported, text without a tune is an error
TEST(multi_tune_files) {
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

  // #87: ABC w: lyrics (words written for this test). Each symbol of ABC 2.1: "-" between syllables, "_" holds,
  // "*" skips, "~" joins words on one note, "\-" is a hyphen in the text, "|" jumps to the next bar; rests and tied
  // continuations take no syllable. Extra syllables and a second verse warn. W: is kept as text.
TEST(lyrics) {
    auto lyrics = [](const Song& s) {
      std::vector<std::tuple<std::string, bool, bool>> r;
      for (auto& e : s.events) if (e.kind == Kind::Note) r.push_back({e.lyric, e.lyricHyphen, e.lyricHeld});
      return r;
    };
    using L = std::tuple<std::string, bool, bool>;
    auto r = importAbc("X:1\nT:Lyric Test\nM:4/4\nL:1/4\nK:D\nD E F G|A B c d|\nw:Hap-py_ | on~it * co\\-op\n"
                       "e2 z2|f2- f2|g4|]\nw:rest tie end extra\nw:second verse\nW:Words after the tune,\nW:two lines.\n");
    auto got = lyrics(r.song);
    std::vector<L> want = {{"Hap", true, false}, {"py", false, false}, {"", false, true}, {"", false, false},       // D E F G
                           {"on it", false, false}, {"", false, false}, {"co-op", false, false}, {"", false, false},  // A B c d
                           {"rest", false, false}, {"tie", false, false}, {"", false, false}, {"end", false, false}};  // e f- f g
    assert(got == want);
    assert(std::count(r.warnings.begin(), r.warnings.end(), "a w: line has more syllables than its notes; the extra ones were dropped") == 1);
    assert(std::count(r.warnings.begin(), r.warnings.end(), "only the first verse of the lyrics (w:) was imported") == 1);
    assert(r.song.words == "Words after the tune,\ntwo lines.");
    // a hyphen alone is a syllable: the note gets nothing and the word goes on; a w: under two music lines covers both
    auto two = importAbc("X:1\nT:t\nM:3/4\nL:1/4\nK:D\nD E F|\nG A B|\nw:a--b c d e\n").song;
    assert(lyrics(two) == (std::vector<L>{{"a", true, false}, {"", false, false}, {"b", false, false}, {"c", false, false},
                                          {"d", false, false}, {"e", false, false}}));
    // a song without lyrics writes nothing new: no w: lines, no lyric keys
    Song plainSong = importAbc("X:1\nT:t\nM:3/4\nL:1/4\nK:D\nD E F|\n").song;
    assert(toAbc(plainSong).find("w:") == std::string::npos && toJson(plainSong).dump().find("lyric") == std::string::npos);
    // an old song file (no lyric or words keys) loads with no lyrics
    json old = toJson(plainSong);
    assert(lyrics(fromJson(old)) == (std::vector<L>(3)) && fromJson(old).words.empty());
    // saved and reloaded, and written by toAbc and imported again: the same lyrics and words
    Song s = r.song;
    s.id = "lyric-test";
    saveSong(s);
    assert(toJson(*loadSong("lyric-test")) == toJson(s));
    fs::remove(dataDir() / "songs" / "lyric-test.json");
    std::string abc = toAbc(s);
    assert(abc.find("w:Hap-py _ * on~it * co\\-op") != std::string::npos);
    Song back = importAbc(abc).song;
    assert(lyrics(back) == lyrics(s) && back.words == s.words);
    assert(lyrics(importAbc(toAbc(two)).song) == lyrics(two));
}
