// Playback and practice: the transport bar, the mic, wait-for-me and play-along grading.
#include "gui.hpp"
#include <algorithm>
#include <cmath>

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
    if (e.kind == Kind::Note && !tiedFromPrev(a, k) && myInstrument(a.set).plays(e.midi)) break;
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
  a.audio.play(perform(*a.song, a.bpm, fromOrder, countIn, a.set.sound, myInstrument(a.set)));
}

static void restartHere(App& a) {  // restart playback at the current note, e.g. with a new tempo or sound
  double t = a.audio.playTime();
  int from = a.playFrom;
  for (auto& v : a.audio.performance().voices) if (v.start <= t) from = v.ord;
  startPlayback(a, from, false);
}

void practiceUpdate(App& a, float dt) {
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
  const Instrument me = myInstrument(a.set);  // I sound in my instrument's octave, whatever the song was written for
  if (a.listen) {
    const MicSearch ms = micSearch(me, a.audio.micRate());
    static std::vector<float> buf;
    buf.resize(ms.window);
    int n = a.audio.latest(buf.data(), (int)buf.size());
    float level = n ? rms(buf.data(), n) : 0;
    recent = n ? rms(buf.data() + n - 512, 512) : 0;
    a.silentFor = level > 1e-5f ? 0 : a.silentFor + dt;
    if (level > a.set.gate)
      if (float hz = detectPitch(buf.data(), n, a.audio.micRate(), ms.fmin, ms.fmax)) a.heard = (float)writtenMidiFromFreq(hz, me.octave);
  }
  auto cents = [&](int midi) { return heardCents(a.heard, midi, me); };  // a voice: to the nearest octave of the note

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
          MicNote m = judgeNote(a.heard, v.midi, a.set.tolerance, me);
          a.hits += m == MicNote::Right;
          a.octs += m == MicNote::Octave;
        }
      }
    }
  } else {
    if (a.audio.performance().length > 0) a.audio.stop();  // finished
    if (a.listen && a.mode == App::Wait && a.waitK < (int)a.order.size()) {
      const Event& e = a.song->events[a.order[a.waitK]];
      a.cursor = a.order[a.waitK];
      bool right = judgeNote(a.heard, e.midi, a.set.tolerance, me) == MicNote::Right;
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
          a.needRelease = a.waitK < (int)a.order.size() && sameHeard(a.song->events[a.order[a.waitK]].midi, e.midi, me);  // a singer's octave leap too
          a.stable = 0;
        }
      } else a.stable = 0;
    }
  }
  if (a.cursor < 0 && prevCursor >= 0) a.cursorSys = -1;
}

void transportBar(App& a) {
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
  auto soundName = [&](const std::string& id) { return instName(id, a.set.whistle, a.set.harp); };
  std::string songSound = trf("%s (mine)", instName(myInstrument(a.set)).c_str());
  std::string soundLabel = trf("sound: %s", (a.set.sound.empty() ? songSound : soundName(a.set.sound)).c_str());
  ImGui::SetNextItemWidth(std::max(170.0f, ImGui::CalcTextSize(soundLabel.c_str()).x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2));  // longer in Portuguese
  if (ImGui::BeginCombo("##sound", soundLabel.c_str())) {
    if (ImGui::Selectable(songSound.c_str(), a.set.sound.empty())) { a.set.sound.clear(); if (playing) restartHere(a); }
    for (auto& id : SOUNDS)
      if (ImGui::Selectable(soundName(id).c_str(), a.set.sound == id)) { a.set.sound = id; if (playing) restartHere(a); }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Which instrument plays the tune (saved, also in Settings). The notes sound in your instrument's octave."));
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
      const Instrument me = myInstrument(a.set);
      bool whistle = me.hints && a.set.hints;
      ImGui::TextColored(col, tr("hearing %s %+.0fc  %s"), noteName(spell(a.song->key, m)).c_str(), c,
                         whistle ? fingeringText(finger(me.key, m)).c_str() : "");
      if (a.cursor >= 0 && a.cursor < (int)a.song->events.size() && !me.voice) {  // singers pick their own octave
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
