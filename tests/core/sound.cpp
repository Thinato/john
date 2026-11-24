// Core tests: play order, performance timing, pitch detection, playback voices, singing voices.
#include "test.hpp"

  // play order: repeats and 1st/2nd endings
TEST(play_order) {
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
}

  // ties merge only the tied notes; the next same-pitch note is re-articulated
TEST(ties) {
  auto tie = perform(importAbc("X:1\nT:t\nM:4/4\nL:1/4\nK:D\nA- A A B|\n").song, 120, 0, false);
  assert(tie.voices.size() == 3 && std::abs(tie.voices[0].dur - 1.0) < 1e-9);
}

  // count-in stays on the beat grid for a jig pickup: clicks every dotted quarter
TEST(count_in) {
  auto jig = perform(importAbc("X:1\nT:j\nM:6/8\nL:1/8\nK:D\nA|d2d fed|\n").song, 120, 0, true);
  for (size_t i = 1; i < jig.clicks.size(); i++) assert(std::abs(jig.clicks[i].first - jig.clicks[i - 1].first - 0.75) < 1e-9);
  assert(std::abs(jig.voices[0].start - (jig.countIn)) < 1e-9 && std::abs(jig.voices[1].start - jig.clicks[2].first - 0.75) < 1e-9);
}

  // performance timing: quarter = 0.5 s at 120 bpm, one bar count-in, ties merge, cut adds a grace
TEST(performance_timing) {
  auto pf = importAbc("X:1\nT:p\nM:4/4\nL:1/4\nK:D\n{g}A B- B c|\n").song;
  Performance perf = perform(pf, 120);
  assert(std::abs(perf.countIn - 2.0) < 1e-9 && perf.voices.size() == 3);
  assert(std::abs(perf.voices[0].start - 2.0) < 1e-9 && std::abs(perf.voices[1].dur - 1.0) < 1e-9);
  assert(perf.voices[0].graces.size() == 1 && std::abs(perf.length - 4.0) < 1e-9);
  assert(perf.clicks.size() == 8 && perf.clicks[0].second && !perf.clicks[1].second);
}

  // pitch detection: pure sine, synthesized whistle note, silence
TEST(pitch_detection) {
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

  // playback sounds: every voice keeps the song's pitch, struck/plucked voices decay, no NaN or clipping
TEST(playback_sounds) {
    const int sr = 48000, n = 2048;
    Song v = importAbc("X:1\nT:v\nM:4/4\nL:1/4\nK:D\nA4|\n").song;  // 4 s whole note
    v.instrument = "violin";                                         // written A4 sounds 440 Hz
    auto window = [&](const std::vector<float>& x, double at) { return x.data() + (int)(at * sr); };
    for (auto& id : SOUNDS) {
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
    Song w = v;  // the octave comes from the player's instrument (by default the song's), not the sound: a whistle song on guitar still sounds A5
    w.instrument = "whistle";
    Performance pg = perform(w, 60, 0, false, "guitar");
    assert(pg.octave == 1 && perform(w, 60, 0, false, "kazoo").sound == "whistle" && perform(w, 60).sound == "whistle");
    Synth syn;
    syn.metronome = false;
    long long sp = sr / 10;
    std::vector<float> x(n);
    syn.render(pg, sp, x.data(), n);
    assert(std::abs(detectPitch(x.data(), n, sr) - 880) < 5);

    // played on my instrument (#90): a violin-written A4 on a D whistle sounds A5 (written + 12), with the whistle's
    // timbre by default; the mic reads it back with my octave as the written A4, so play-along counts it right
    const Instrument dw = *findInstrument("whistle", "D");
    Performance pm = perform(v, 60, 0, false, "", dw);
    assert(pm.octave == 1 && pm.sound == "whistle" && perform(v, 60, 0, false, "piano", dw).octave == 1);
    Synth mine;
    mine.metronome = false;
    sp = sr / 10;
    mine.render(pm, sp, x.data(), n);
    float heard = detectPitch(x.data(), n, sr, (float)soundingFreq(dw.low, dw.octave) * 0.8f, (float)soundingFreq(dw.high, dw.octave) * 1.15f);
    assert(std::abs(heard - 880) < 5 && std::lround(writtenMidiFromFreq(heard, dw.octave)) == pm.voices[0].midi);
    // a song written for my instrument plays as before (whistle song on a D whistle; violin song on a violin)
    Performance same = perform(w, 60, 0, false, "", dw), before = perform(w, 60, 0, false);
    assert(same.octave == before.octave && same.sound == before.sound && same.voices.size() == before.voices.size());
    assert(perform(v, 60, 0, false, "", *findInstrument("violin")).octave == 0);

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

  // singing voices (#87): the presets, the range test's maths on rendered takes, the mic's octave rule, breaths
TEST(singing_voices) {
    const Instrument sop = *findInstrument("voice-soprano"), alt = *findInstrument("voice-alto"), ten = *findInstrument("voice-tenor"),
                     bas = *findInstrument("voice-bass"), vln = *findInstrument("violin");
    assert(sop.low == parseNote("C4") && sop.high == parseNote("A5") && sop.octave == 0 && alt.low == parseNote("G3") && alt.high == parseNote("E5"));
    assert(ten.octave == -1 && ten.low == parseNote("C4") && ten.high == parseNote("A5"));  // sounds C3..A4
    assert(bas.octave == -1 && bas.low == parseNote("E3") && bas.high == parseNote("E5"));  // sounds E2..E4
    assert((ten.techniques == std::vector<std::string>{"breath", "vibrato", "slide"}) && ten.notes.empty() && !vln.voice);
    const int sr = 48000;
    // one take: `secs` of a sung note (harmonics, a 5.5 Hz vibrato of +-30 cents) after `lead` s of silence, read the
    // way the GUI does: one 2048-sample window per 60 fps frame, gated, YIN over the range test's search
    auto take = [&](double hz, double lead, double secs = 3) {
      std::vector<float> x((size_t)(sr * secs), 0.0f);
      double ph = 0;
      for (size_t i = (size_t)(sr * lead); i < x.size(); i++) {
        double f = hz * std::pow(2.0, 0.3 / 12 * std::sin(6.2831853 * 5.5 * i / sr));
        ph += 6.2831853 * f / sr;
        x[i] = float(0.3 * std::sin(ph) + 0.15 * std::sin(2 * ph) + 0.08 * std::sin(3 * ph));
      }
      std::vector<float> frames;
      for (size_t at = 2048; at <= x.size(); at += sr / 60)
        frames.push_back(rms(&x[at - 2048], 2048) > 0.01f ? detectPitch(&x[at - 2048], 2048, sr, VOICE_MIN_HZ, VOICE_MAX_HZ) : 0);
      return sungNote(frames);
    };
    assert(take(soundingFreq(40, 0), 0) == 40);     // E2, a bass's lowest
    assert(take(soundingFreq(67, 0), 1) == 67);     // G4 after a second of silence
    assert(take(soundingFreq(81, 0), 0.5) == 81);   // A5
    assert(take(soundingFreq(54, 0) * 1.01, 0) == 54);  // 17 cents sharp is still F#3
    assert(!take(440, 2.5) && !take(440, 3));       // sung too late, or nothing: no note
    assert(!sungNote({}) && sungNote({0, 0, 220, 220, 221, 30000}) == 57);  // frames outside the search don't count
    // My voice: the sung range written in the preset's octave, with its techniques and sound; a fifth at least
    auto mv = myVoice(bas, 64, 40);  // E4 and E2, either order
    assert(mv && mv->id == "my-voice" && mv->name == "My voice" && mv->voice && mv->custom && mv->octave == -1);
    assert(mv->low == 52 && mv->high == 76 && mv->techniques == bas.techniques && mv->sound == bas.sound && mv->notes.empty());
    assert(myVoice(sop, 60, 67) && !myVoice(sop, 60, 66) && myVoice(sop, 60, 67)->low == 60);
    assert(toJson(*mv)["voice"] == true && !toJson(sop).contains("hints"));
    json bad = toJson(*mv);
    bad["id"] = "x", bad["voice"] = "yes";
    try { parseInstrument(bad); assert(false); } catch (const std::runtime_error&) {}
    saveInstrument(*mv);
    auto saved = findInstrument("my-voice");
    assert(saved && saved->voice && saved->custom && saved->low == 52 && saved->high == 76 && saved->octave == -1);
    assert(deleteInstrument("my-voice") && !findInstrument("my-voice"));
    // the mic (#87): a tenor sings a written A4 as A3 (220 Hz). Read back as a tenor it is the A4; read with octave 0
    // (a soprano's or a violinist's mic) it is A3, an octave off: right for a voice, "wrong octave" for the violin
    Song a4 = importAbc("X:1\nT:a\nM:4/4\nL:1/4\nK:D\nA4|\n").song;
    a4.instrument = "violin";
    Performance pt = perform(a4, 60, 0, false, "", ten);
    assert(pt.octave == -1 && pt.sound == "violin");
    Synth sy;
    sy.metronome = false;
    long long at = sr / 2;
    std::vector<float> x(2048);
    sy.render(pt, at, x.data(), (int)x.size());
    float hz = detectPitch(x.data(), (int)x.size(), sr, VOICE_MIN_HZ, VOICE_MAX_HZ);
    assert(std::abs(hz - 220) < 3);
    float low = (float)writtenMidiFromFreq(hz, 0);
    assert(judgeNote((float)writtenMidiFromFreq(hz, ten.octave), 69, 40, ten) == MicNote::Right);
    assert(judgeNote(low, 69, 40, sop) == MicNote::Right && judgeNote(low, 69, 40, vln) == MicNote::Octave);
    assert(std::abs(heardCents(low, 69, sop)) < 40 && std::abs(heardCents(low, 69, vln) + 1200) < 40);
    assert(judgeNote(low + 24, 69, 40, sop) == MicNote::Right && judgeNote(low + 1, 69, 40, sop) == MicNote::Wrong);
    assert(judgeNote(69.3f, 69, 40, vln) == MicNote::Right && judgeNote(69.5f, 69, 40, vln) == MicNote::Wrong);
    assert(judgeNote(81.2f, 69, 40, vln) == MicNote::Octave && judgeNote(0, 69, 40, sop) == MicNote::Wrong);
    // play-along's pitch search (micSearch, what the GUI uses): a voice is heard an octave beyond its range, so every
    // preset's lowest note sung an octave down, and its highest an octave up, is found and counts as right
    auto sing = [&](double f, int n) {
      std::vector<float> y(n);
      double ph = 0;
      for (int i = 0; i < n; i++) {
        ph += 6.2831853 * f * std::pow(2.0, 0.3 / 12 * std::sin(6.2831853 * 5.5 * i / sr)) / sr;
        y[i] = float(0.3 * std::sin(ph) + 0.15 * std::sin(2 * ph) + 0.08 * std::sin(3 * ph));
      }
      return y;
    };
    for (const Instrument* v : {&sop, &alt, &ten, &bas})
      for (auto [note, f] : {std::pair{v->low, soundingFreq(v->low, v->octave) / 2}, std::pair{v->high, soundingFreq(v->high, v->octave) * 2}}) {
        const MicSearch ms = micSearch(*v, sr);
        std::vector<float> y = sing(f, ms.window);
        float got = detectPitch(y.data(), ms.window, sr, ms.fmin, ms.fmax);
        if (std::abs(got - f) > f * 0.03) fprintf(stderr, "%s: %.1f Hz heard as %.1f\n", v->id.c_str(), f, got);
        assert(std::abs(got - f) < f * 0.03 && judgeNote((float)writtenMidiFromFreq(got, v->octave), note, 40, *v) == MicNote::Right);
      }
    {  // the instrument-only search (other instruments, and voices before the fix) can't hear the soprano's C3
      Instrument plain = sop;
      plain.voice = false;
      const MicSearch ms = micSearch(plain, sr);
      std::vector<float> y = sing(soundingFreq(sop.low, 0) / 2, ms.window);
      assert(ms.window == 2048 && std::abs(detectPitch(y.data(), ms.window, sr, ms.fmin, ms.fmax) - soundingFreq(sop.low, 0) / 2) > 5);
      const MicSearch mv = micSearch(vln, sr);  // unchanged for the violin
      assert(mv.window == 2048 && std::abs(mv.fmin - soundingFreq(vln.low, 0) * 0.8) < 0.01 && std::abs(mv.fmax - soundingFreq(vln.high, 0) * 1.15) < 0.1);
      assert(micSearch(bas, sr).window == 4096);  // two periods of the bass's E1
    }
    // Wait for me asks for a new attack between notes the mic can't tell apart: a repeat, or a singer's octave leap
    assert(sameHeard(62, 62, vln) && !sameHeard(62, 74, vln) && sameHeard(62, 74, sop) && sameHeard(74, 50, ten) && !sameHeard(62, 63, sop));
    // a singer breathes whatever the sound; the same violin sound on a non-voice doesn't
    Song br = importAbc("X:1\nT:b\nM:4/4\nL:1/4\nK:D\nA !breath! B c d|\n").song;
    assert(perform(br, 60, 0, false, "", sop).voices[0].gap > 0.1 && perform(br, 60, 0, false, "", vln).voices[0].gap == 0);
}
