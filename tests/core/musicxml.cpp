// Core tests: MusicXML import.
#include "test.hpp"

  // #73: MusicXML. A score written by hand gives the same events as the same tune in ABC: repeat, grace cut, triplet,
  // rest, tie, 1st/2nd endings (divisions 6 per quarter: eighth 3, triplet eighth 2)
TEST(musicxml) {
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
    Song taken = importAbc(JIG).song;  // a saved "test-jig", so the import has to pick a free id
    taken.id = "test-jig";
    saveSong(taken);
    auto saved = importSongs(mxl, like);
    assert(saved.size() == 1 && saved[0].song.id == "test-jig-2");  // "test-jig" is taken
    auto back = loadSong("test-jig-2");
    assert(back && back->instrument == "violin" && back->source == "jig.mxl" && toJson(*back)["events"] == toJson(abc)["events"]);
    fs::remove(dataDir() / "songs" / "test-jig-2.json");
    // the viewer's Import song instrument picker (#75 review): the whistle or harp key comes from `like` too
    for (auto [inst, whistle, harp] : {std::tuple{"whistle", "Bb", "C"}, std::tuple{"harmonica", "D", "G"}}) {
      Song as;
      as.instrument = inst;
      as.whistle = whistle;
      as.harp = harp;
      auto got = importSongs("X:1\nT:Picker Test\nK:D\nDEF|\n", as);
      auto s = loadSong(got[0].song.id);
      assert(s && s->instrument == inst && s->whistle == whistle && s->harp == harp);
      assert(instrumentOf(*s).name == (std::string(inst) == "whistle" ? "Bb whistle" : "G harmonica"));
      fs::remove(dataDir() / "songs" / (got[0].song.id + ".json"));
    }
    for (const std::string& bad : {std::string("PK\x03\x04 not a zip"), std::string("<score-timewise/>"), score(jigAttrs, "</measure>")}) {
      bool threw = false;
      try { importMusicXml(bad); } catch (const std::runtime_error&) { threw = true; }
      assert(threw);
    }
    // zip bomb: an entry one byte over MXL_MAX_ENTRY (spaces, so it zips to a few KB) is refused before unzipping,
    // whether it is the score the container names or container.xml itself
    const std::string huge(MXL_MAX_ENTRY + 1, ' ');
    for (const char* name : {"music/jig.musicxml", "META-INF/container.xml"}) {
      mz_zip_archive b{};
      assert(mz_zip_writer_init_heap(&b, 0, 0));
      if (strcmp(name, "META-INF/container.xml") != 0)
        assert(mz_zip_writer_add_mem(&b, "META-INF/container.xml", container.data(), container.size(), MZ_DEFAULT_COMPRESSION));
      assert(mz_zip_writer_add_mem(&b, name, huge.data(), huge.size(), MZ_BEST_SPEED));
      void* bbuf = nullptr;
      size_t bsize = 0;
      assert(mz_zip_writer_finalize_heap_archive(&b, &bbuf, &bsize));
      const std::string bomb((const char*)bbuf, bsize);
      mz_free(bbuf);
      mz_zip_writer_end(&b);
      assert(bomb.size() < MXL_MAX_ENTRY / 100);
      std::string err;
      try { importMusicXml(bomb); } catch (const std::runtime_error& e) { err = e.what(); }
      assert(err == "The .mxl file is too large.");
    }

    // #75: the image reader missed the first line's key signature. Line 1 (measures 1-2) has none, line 2 (from measure
    // 3, <print new-system>) has 2 sharps: line 1 gets them, except a written natural and the same note after it in the
    // bar; line 2's notes already carry their sharps
    auto line1 = [&](const std::string& end2) {
      return n("F", 4, 1) + n("F", 4, 1, "<accidental>natural</accidental>") + n("F", 4, 1) + n("C", 5, 1) + "</measure><measure number=\"2\">" +
             n("F", 5, 4) + end2 + "</measure>";
    };
    const std::string line2 = "<measure number=\"3\"><print new-system=\"yes\"/><attributes><key><fifths>2</fifths></key></attributes>" +
                              n("F", 4, 2, "", 1) + n("C", 5, 2, "", 1) + "</measure>";
    const std::string plain = "<divisions>1</divisions><key><fifths>0</fifths></key><time><beats>4</beats><beat-type>4</beat-type></time>";
    std::string read = score(plain, line1("") + line2);
    auto k = checkFirstLineKey(read);
    assert(k && k->first == 0 && k->rest == 2 && k->fixed);
    auto fixed = importMusicXml(read);
    assert(fixed.song.key.fifths == 2 && fixed.warnings.empty());
    std::vector<int> midis;
    for (auto& e : fixed.song.events) if (e.kind == Kind::Note) midis.push_back(e.midi);
    assert((midis == std::vector<int>{66, 65, 65, 73, 78, 66, 73}));
    std::string again = read;
    assert(!checkFirstLineKey(again) && again == read);  // fixed: one key now
    // a written natural tied into the next bar stays natural there (the tied note has no accidental of its own)
    std::string tied = score(plain, n("F", 4, 4, "<accidental>natural</accidental><tie type=\"start\"/>") + "</measure><measure number=\"2\">" +
                                        n("F", 4, 4, "<tie type=\"stop\"/>") + "</measure>" + line2);
    assert(checkFirstLineKey(tied)->fixed);
    auto t = importMusicXml(tied).song;
    assert(nth(t, 0).midi == 65 && nth(t, 1).midi == 65 && nth(t, 0).tie);
    // a repeat where the lines meet changes nothing (Drowsy Maggie's first line is its A part)
    std::string atRepeat = score(plain, line1("<barline location=\"right\"><repeat direction=\"backward\"/></barline>") + line2);
    assert(checkFirstLineKey(atRepeat)->fixed && importMusicXml(atRepeat).song.key.fifths == 2);
    // not fixed, only reported: line 1 writes a sharp on an F (with 2 sharps in the key it wouldn't need one, so that
    // line really has no key signature), or it has a key of its own
    for (auto [attrs, sharpF, first] : {std::tuple{plain, n("F", 4, 1, "<accidental>sharp</accidental>", 1), 0},
                                        std::tuple{std::regex_replace(plain, std::regex("<fifths>0"), "<fifths>1"), std::string(), 1}}) {
      std::string x = score(attrs, sharpF + line1("") + line2), before = x;
      auto c = checkFirstLineKey(x);
      assert(c && c->first == first && c->rest == 2 && !c->fixed && x == before);
    }
    // nothing to report: one key throughout, no line breaks, or the key changes later than line 2
    std::string same = score(jigAttrs, jig), nolines = score(plain, line1("") + std::regex_replace(line2, std::regex("<print new-system=\"yes\"/>"), "")),
                later = score(plain, std::regex_replace(line1(""), std::regex("<measure number=\"2\">"), "<measure number=\"2\"><print new-system=\"yes\"/>") + line2);
    assert(!checkFirstLineKey(same) && !checkFirstLineKey(nolines) && !checkFirstLineKey(later));

    // #87: <lyric> verse 1 (another verse warns), syllabic begin/end, an <extend> held over the next notes until
    // the stop; a note without lyrics after the stop holds nothing
    auto lyr = [](const std::string& body) { return "<lyric number=\"1\">" + body + "</lyric>"; };
    auto ly = importMusicXml(score(plain, n("D", 4, 1, lyr("<syllabic>begin</syllabic><text>Sun</text>") +
                                                         "<lyric number=\"2\"><syllabic>single</syllabic><text>x</text></lyric>") +
                                              n("E", 4, 1, lyr("<syllabic>end</syllabic><text>ny</text><extend type=\"start\"/>")) +
                                              n("F", 4, 1) + n("G", 4, 1, lyr("<extend type=\"stop\"/>")) + n("A", 4, 1) + "</measure>"));
    assert(std::count(ly.warnings.begin(), ly.warnings.end(), "only the first verse of the lyrics was imported") == 1);
    assert(nth(ly.song, 0).lyric == "Sun" && nth(ly.song, 0).lyricHyphen && nth(ly.song, 1).lyric == "ny" && !nth(ly.song, 1).lyricHyphen);
    assert(nth(ly.song, 2).lyricHeld && nth(ly.song, 3).lyricHeld && !nth(ly.song, 4).lyricHeld && nth(ly.song, 4).lyric.empty());
}
