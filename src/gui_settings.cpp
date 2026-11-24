// The Settings window, the custom instrument editor (#85) and Find my range (#87).
#include "gui.hpp"
#include <algorithm>

// none: label for "", or null. ids: the choices, default every instrument (the Sound combo passes SOUNDS).
void instrumentCombo(const char* label, std::string& v, const char* none, const std::string& whistle,
                     const std::vector<std::string>& ids) {
  auto name = [&](const std::string& id) { return id.empty() ? std::string(none) : instName(id, whistle); };
  if (!ImGui::BeginCombo(label, name(v).c_str(), ImGuiComboFlags_HeightLarge)) return;  // every instrument without scrolling
  if (none && ImGui::Selectable(none, v.empty())) v.clear();
  for (auto& id : ids) if (ImGui::Selectable((name(id) + "###" + id).c_str(), v == id)) v = id;
  ImGui::EndCombo();
}

// The editor for a custom instrument (#85), opened from Settings. Works on a copy (a.editing); Save writes the file.
void instrumentEditor(App& a) {
  if (!a.editing) return;
  Instrument& in = *a.editing;
  bool open = true;
  ImGui::SetNextWindowPos({660, 80}, ImGuiCond_FirstUseEver);  // right of Settings
  ImGui::SetNextWindowBgAlpha(1);
  if (!ImGui::Begin(L("Instrument").c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    if (!open) a.editing.reset();
    return;
  }
  ImGui::PushItemWidth(200);
  ImGui::InputText(L("Name").c_str(), a.editName, sizeof a.editName);
  auto note = [](int m) { return noteName(spell(Key{0, "maj"}, m)); };  // sharps: C#4
  auto noteCombo = [&](const char* label, int& v, int lo, int hi) {
    if (!ImGui::BeginCombo(label, note(v).c_str(), ImGuiComboFlags_HeightLarge)) return;
    for (int m = lo; m <= hi; m++) {
      if (ImGui::Selectable((note(m) + "###n" + std::to_string(m)).c_str(), m == v)) v = m;
      if (m == v) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  };
  // The note table is relative to the bottom note: moving it keeps each note's level where it was on the staff.
  int low = in.low;
  noteCombo(L("Lowest note").c_str(), low, 21, in.high);
  if (low != in.low) {
    std::map<int, NoteLevel> moved;
    for (auto& [at, n] : in.notes) if (in.low + at >= low) moved[in.low + at - low] = n;
    in.notes = moved;
    in.low = low;
  }
  noteCombo(L("Highest note").c_str(), in.high, in.low, 108);
  std::erase_if(in.notes, [&](auto& kv) { return kv.first > in.high - in.low; });
  if (ImGui::BeginCombo(L("Key").c_str(), in.key.empty() ? tr("none") : keyLabel(in.key).c_str())) {
    if (ImGui::Selectable(tr("none"), in.key.empty())) in.key.clear();
    for (auto& k : HARP_KEYS) if (ImGui::Selectable((keyLabel(k) + "###" + k).c_str(), k == in.key)) in.key = k;
    ImGui::EndCombo();
    // a profile that comes in several keys (a copy of the whistle) must list its own key; otherwise it has just this one
    if (!in.keys.empty() && !std::count(in.keys.begin(), in.keys.end(), in.key)) in.keys.clear(), in.keyedName.clear();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("For instruments made in one key (a C flute): Diatonic marks the notes outside its major scale"));
  const char* octaves[] = {tr("an octave lower than written"), tr("as written"), tr("an octave higher than written")};
  int oct = std::clamp(in.octave, -1, 1) + 1;
  if (ImGui::Combo(L("Sounds").c_str(), &oct, octaves, 3)) in.octave = oct - 1;
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Whistle music is written an octave below how it sounds, guitar music an octave above"));
  instrumentCombo(L("Sound").c_str(), in.sound, nullptr, "D", SOUNDS);
  ImGui::PopItemWidth();

  ImGui::SeparatorText(tr("Notes"));
  if (ImGui::Button(L("Chromatic").c_str())) applyPreset(in, false);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Every note in the range is easy"));
  ImGui::SameLine();
  if (ImGui::Button(L("Diatonic").c_str())) applyPreset(in, true);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Notes outside the major scale of the key (or of the lowest note) are hard"));
  ImGui::TextDisabled("%s", tr("Click a note: easy, hard, impossible."));
  const ImVec4 colors[] = {{0.25f, 0.55f, 0.3f, 1}, {0.75f, 0.5f, 0.15f, 1}, {0.7f, 0.25f, 0.25f, 1}};
  const char* levels[] = {tr("easy"), tr("hard"), tr("impossible")};
  for (int m = in.low; m <= in.high; m++) {
    const int at = m - in.low;
    if (at % 12) ImGui::SameLine(0, 2);  // one octave per row
    NoteLevel n = in.level(m);
    ImGui::PushStyleColor(ImGuiCol_Button, colors[n.level]);
    if (ImGui::Button((note(m) + "###cell" + std::to_string(at)).c_str(), {ImGui::GetFontSize() * 2.6f, 0})) {
      n.level = NoteLevel::Level((n.level + 1) % 3);
      in.notes[at] = n;  // how and weight stay for when it comes back to hard (toJson drops easy entries)
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s%s", levels[n.level], n.how.empty() ? "" : (" (" + tr(n.how) + ")").c_str());  // NOTE_HOWS in pt-BR/instruments.json
  }

  ImGui::SeparatorText(tr("Techniques it can play"));
  int col = 0;
  for (auto& t : a.techs) {
    bool on = std::count(in.techniques.begin(), in.techniques.end(), t.id) > 0;
    if (col % 3) ImGui::SameLine(ImGui::GetStyle().WindowPadding.x + ImGui::GetFontSize() * 10 * (col % 3));  // three columns
    col++;
    if (ImGui::Checkbox((t.name + "###tech:" + t.id).c_str(), &on)) {
      std::erase(in.techniques, t.id);
      if (on) in.techniques.push_back(t.id);
    }
  }
  // the others are hidden, or shown and played as one it can play (#86); entries that no longer apply go, and so do
  // ids the catalog doesn't have (a shared profile), which the editor couldn't show and saveInstrument() refuses
  auto known = [&](const std::string& id) { return std::any_of(a.techs.begin(), a.techs.end(), [&](auto& t) { return t.id == id; }); };
  std::erase_if(in.techniques, [&](auto& id) { return !known(id); });
  auto lists = [&](const std::string& id) { return std::count(in.techniques.begin(), in.techniques.end(), id) > 0; };
  std::erase_if(in.instead, [&](auto& kv) { return !known(kv.first) || lists(kv.first) || !lists(kv.second); });
  ImGui::SeparatorText(tr("Played instead"));
  ImGui::TextDisabled("%s", tr("Techniques it can't play are hidden, or shown as one it can."));
  auto techName = [&](const std::string& id) {
    auto it = std::find_if(a.techs.begin(), a.techs.end(), [&](auto& x) { return x.id == id; });
    return it != a.techs.end() ? it->name : id;
  };
  col = 0;
  for (auto& t : a.techs) {
    if (lists(t.id) || !builtinTechnique(t.id) || t.id == "breath") continue;  // custom techniques and breath always show
    if (col % 2) ImGui::SameLine(ImGui::GetStyle().WindowPadding.x + ImGui::GetFontSize() * 15);  // two columns
    col++;
    auto it = in.instead.find(t.id);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    if (ImGui::BeginCombo((t.name + "###instead:" + t.id).c_str(), it == in.instead.end() ? tr("hidden") : techName(it->second).c_str())) {
      if (ImGui::Selectable(tr("hidden"), it == in.instead.end())) in.instead.erase(t.id);
      for (auto& to : in.techniques)
        if (ImGui::Selectable((techName(to) + "###" + to).c_str(), it != in.instead.end() && it->second == to)) in.instead[t.id] = to;
      ImGui::EndCombo();
    }
  }

  ImGui::Separator();
  if (ImGui::Button(L("Save").c_str())) {
    in.name = a.editName;
    if (in.name.empty()) a.editError = tr("Give it a name.");
    else {
      if (a.editingNew) {  // a new id from the name, never one that exists (that would replace a built-in)
        std::string base = slug(in.name), id = base;
        auto ids = instrumentIds();
        for (int n = 2; id.empty() || std::count(ids.begin(), ids.end(), id); n++) id = (base.empty() ? "instrument" : base) + "-" + std::to_string(n);
        in.id = id;
      }
      try {
        saveInstrument(in);
        a.editing.reset();
        a.lastPoll = -10;  // judge the songs again now
      } catch (const std::exception& e) {
        a.editError = e.what();
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(L("Cancel").c_str())) a.editing.reset();
  if (!a.editError.empty()) ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", a.editError.c_str());
  ImGui::End();
  if (!open) a.editing.reset();
}

// Settings' list of custom instruments: edit, delete (asks first, with the number of songs written for it), new.
static void customInstruments(App& a) {
  static std::string toDelete;
  for (auto& in : instruments()) {
    if (!in.custom) continue;
    ImGui::PushID(in.id.c_str());
    ImGui::TextUnformatted(instName(in).c_str());  // "My voice" (Find my range) is translated
    ImGui::SameLine();
    if (ImGui::SmallButton(L("Edit").c_str())) {
      a.editing = in;
      a.editingNew = false;
      a.editError.clear();
      snprintf(a.editName, sizeof a.editName, "%s", in.name.c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(L("Delete").c_str())) {
      toDelete = in.id;
      a.deleteSongs = 0;
      for (auto& id : listSongs()) if (auto s = loadSong(id)) a.deleteSongs += s->instrument == in.id;
      ImGui::OpenPopup("###deleteInstrument");
    }
    if (ImGui::BeginPopupModal((tr("Delete instrument") + std::string("###deleteInstrument")).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text(tr("Delete \"%s\"?"), in.name.c_str());
      if (in.builtin) ImGui::TextUnformatted(tr("John's own instrument with this name comes back."));
      else ImGui::Text(tr("%d songs are written for it. They keep their notes and show it as deleted."), a.deleteSongs);
      if (ImGui::Button(L("Delete").c_str())) {
        deleteInstrument(toDelete);
        if (a.set.instrument == toDelete && !findInstrument(toDelete)) a.set.instrument = "whistle", a.set.whistle = "D";
        if (a.editing && a.editing->id == toDelete) a.editing.reset();
        a.lastPoll = -10;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button(L("Cancel").c_str())) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
  if (ImGui::Button(L("New instrument").c_str())) {
    Instrument in;
    in.name = tr("My instrument");
    in.low = 62, in.high = 86, in.sound = "whistle";  // D4..D6, chromatic: the user narrows it down
    a.editing = in;
    a.editingNew = true;
    a.editError.clear();
    snprintf(a.editName, sizeof a.editName, "%s", in.name.c_str());
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Describe an instrument John doesn't know: its range, which notes are hard, what it sounds like"));
  for (auto& w : instrumentWarnings()) ImGui::TextColored({1, 0.5f, 0.4f, 1}, "%s", w.c_str());
}

// One line per Settings field; App::set is saved by the main loop whenever it changes.
void settingsWindow(App& a) {
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
  ImGui::SeparatorText(tr("My instrument"));
  instrumentCombo(L("Instrument").c_str(), s.instrument, nullptr, s.whistle);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The instrument you play: the song list shows the songs that fit it, and new songs are made for it"));
  if (myInstrument(s).voice) {  // the button and its tooltip together: otherwise the tooltip lands on the combo above
    if (ImGui::Button(L("Find my range").c_str())) {
      a.showRange = true;
      a.rangeFrom = myInstrument(s);
      a.rangeNote[0].reset(), a.rangeNote[1].reset();
      a.rangeError.clear();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Sing your lowest and highest notes: John saves them as your own instrument, My voice"));
  }
  if (s.instrument == "whistle" && ImGui::BeginCombo(L("Whistle key").c_str(), keyLabel(s.whistle).c_str())) {
    for (auto& k : HARP_KEYS) if (ImGui::Selectable((keyLabel(k) + "###" + k).c_str(), k == s.whistle)) s.whistle = k;
    ImGui::EndCombo();
  }
  if (ImGui::BeginCombo(L("Harmonica key").c_str(), keyLabel(s.harp).c_str())) {
    for (auto& k : HARP_KEYS) if (ImGui::Selectable((keyLabel(k) + "###" + k).c_str(), k == s.harp)) s.harp = k;
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Your harmonica's key, used when a song is switched to harmonica or imported for it"));
  ImGui::SeparatorText(tr("Custom instruments"));
  customInstruments(a);
  ImGui::SeparatorText(tr("Sheet"));
  ImGui::Checkbox(L("Instrument hints (whistle fingerings)").c_str(), &s.hints);
  ImGui::Checkbox(L("Show all marks").c_str(), &s.allMarks);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Show every technique mark of the song, also those your instrument doesn't play"));
  ImGui::Checkbox(L("Show lyrics").c_str(), &s.lyrics);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Show the song's words under the notes"));
  ImGui::SliderFloat(L("Zoom").c_str(), &s.zoom, 6, 16, "%.0f");
  ImGui::Checkbox(L("Light theme").c_str(), &s.light);
  ImGui::SeparatorText(tr("Playback"));
  ImGui::SliderInt(L("Tempo for songs without one").c_str(), &s.tempo, 40, 220);
  ImGui::Checkbox(L("Count-in bar before playing").c_str(), &s.countIn);
  ImGui::Checkbox(L("Metronome").c_str(), &s.metronome);
  ImGui::SliderFloat(L("Volume").c_str(), &s.volume, 0, 1, "%.1f");
  instrumentCombo(L("Sound").c_str(), s.sound, tr("My instrument"), "D", SOUNDS);
  ImGui::SeparatorText(tr("Microphone"));
  ImGui::SliderFloat(L("Tolerance (cents)").c_str(), &s.tolerance, 10, 100, "%.0f");
  ImGui::SliderFloat(L("Noise gate").c_str(), &s.gate, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat(L("Latency (ms)").c_str(), &s.latencyMs, 0, 300, "%.0f");
  ImGui::PopItemWidth();
  ImGui::End();
}

// A small treble staff with one note (Find my range), written as the voice reads it: a voice that sounds an octave
// below the written notes gets the tenor clef (a G clef with an 8 below). sounding: MIDI; nullopt = an empty staff.
// A note far off the staff is drawn an octave (or two) nearer, marked 8va/15ma above or 8vb/15mb below.
static void noteStaff(const Instrument& in, std::optional<int> sounding) {
  const float sp = 7, w = sp * 9, h = sp * 15;
  const ImVec2 o = ImGui::GetCursorScreenPos();
  ImGui::Dummy({w, h});
  if (!gMusic) return;
  Score sc{ImGui::GetWindowDrawList(), sp};
  sc.dl->PushClipRect(o, {o.x + w, o.y + h}, true);
  sc.dl->AddRectFilled(o, {o.x + w, o.y + h}, IM_COL32(252, 250, 244, 255), 3);
  const float top = o.y + sp * 5;  // top line, step 38
  auto yOf = [&](int step) { return top + (38 - step) * sp / 2; };
  for (int l = 30; l <= 38; l += 2) sc.line(o.x + sp * 0.5f, yOf(l), o.x + w - sp * 0.5f, yOf(l), 1, sc.ink);
  sc.glyph(in.octave < 0 ? GClef8vb : GClef, o.x + sp, yOf(32), sc.ink);
  if (sounding) {
    const Spelled n = spell(Key{0, "maj"}, *sounding - 12 * in.octave);
    int step = n.step(), ottava = 0;  // octaves moved: + drawn lower (8va), - drawn higher (8vb)
    while (step > 43) step -= 7, ottava++;  // more than three ledger lines above, or four below, don't fit the box
    while (step < 23) step += 7, ottava--;
    const float x = o.x + sp * 5.5f, y = yOf(step), hw = sc.glyphW(HeadWhole);
    if (ottava) sc.text(ottava == 1 ? "8va" : ottava == -1 ? "8vb" : ottava > 0 ? "15ma" : "15mb", x + hw / 2, ottava > 0 ? o.y + sp * 0.1f : o.y + h - sp * 1.6f, sp * 1.5f, sc.ink);
    for (int l = 28; l >= step; l -= 2) sc.line(x - sp * 0.4f, yOf(l), x + hw + sp * 0.4f, yOf(l), 1, sc.ink);
    for (int l = 40; l <= step; l += 2) sc.line(x - sp * 0.4f, yOf(l), x + hw + sp * 0.4f, yOf(l), 1, sc.ink);
    if (n.alter) sc.glyph(n.alter > 0 ? Sharp : Flat, x - sp * 1.3f, y, sc.ink);
    sc.glyph(HeadWhole, x, y, sc.ink);
  }
  sc.dl->PopClipRect();
}

// Find my range (#87): the singer sings the lowest, then the highest comfortable note for RANGE_TAKE seconds each.
// The GUI only records one pitch per frame; sungNote() and myVoice() in core do the maths. Works on the web too.
// The take counts mic samples, not seconds: it starts with the first audio after Sing (the web's getUserMedia answers
// late, and an open mic holds older audio), and reads only windows made of new samples, once per new batch.
constexpr double RANGE_TAKE = 3;
constexpr int RANGE_WINDOW = 2048;  // two periods of VOICE_MIN_HZ at 48 kHz
void rangeWindow(App& a) {
  if (!a.showRange) return;
  const long long takeLen = RANGE_WINDOW + (long long)(RANGE_TAKE * a.audio.micRate());
  if (a.rangeTake >= 0) {
    const long long now = a.audio.micSamples(), fresh = now - a.rangeMark;
    if (fresh >= RANGE_WINDOW && now != a.rangeLast) {
      a.rangeLast = now;
      static std::vector<float> buf(RANGE_WINDOW);
      int n = a.audio.latest(buf.data(), RANGE_WINDOW);
      a.rangeHz.push_back(n && rms(buf.data(), n) > a.set.gate ? detectPitch(buf.data(), n, a.audio.micRate(), VOICE_MIN_HZ, VOICE_MAX_HZ) : 0);
    }
    const bool done = fresh >= takeLen, deaf = fresh == 0 && ImGui::GetTime() - a.rangeClick > 6;
    if (done) {
      a.rangeNote[a.rangeTake] = sungNote(a.rangeHz);
      if (!a.rangeNote[a.rangeTake]) a.rangeError = tr("No clear note heard. Sing a little louder or closer to the microphone, and try again.");
    }
    if (deaf) a.rangeError = tr("no sound from the mic: allow microphone access for this app/terminal in System Settings > Privacy & Security");
    if (done || deaf) {
      a.rangeTake = -1;
      if (!a.listen) a.audio.stopMic();
    }
  }
  bool open = true;
  ImGui::SetNextWindowPos({300, 120}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(1);
  if (ImGui::Begin(L("Find my range").c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    ImGui::TextUnformatted(tr("Sing each note on a long \"ah\" for about 3 seconds, as steady as you can."));
    const char* asks[2] = {tr("Sing your lowest comfortable note"), tr("Then your highest comfortable note")};
    for (int i = 0; i < 2; i++) {
      ImGui::PushID(i);
      noteStaff(a.rangeFrom, a.rangeNote[i]);
      ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::TextUnformatted(asks[i]);
      ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.6f);
      if (a.rangeNote[i]) ImGui::TextUnformatted(noteName(spell(Key{0, "maj"}, *a.rangeNote[i])).c_str());
      else ImGui::TextDisabled("-");
      ImGui::PopFont();
      if (a.rangeTake == i) ImGui::ProgressBar(std::clamp(float(a.audio.micSamples() - a.rangeMark) / takeLen, 0.0f, 1.0f), {220, 0}, tr("listening..."));
      else {
        ImGui::BeginDisabled(a.rangeTake >= 0 || (i == 1 && !a.rangeNote[0]));
        if (ImGui::Button((a.rangeNote[i] ? L("Retry") : L("Sing")).c_str())) {
          a.rangeError.clear();
          if (a.audio.startMic()) {
            a.rangeTake = i, a.rangeClick = ImGui::GetTime();
            a.rangeMark = a.rangeLast = a.audio.micSamples();  // only what comes after this counts
            a.rangeHz.clear();
            a.rangeNote[i].reset();
          } else a.rangeError = a.audio.error;
        }
        ImGui::EndDisabled();
      }
      ImGui::EndGroup();
      ImGui::PopID();
    }
    if (a.rangeNote[0] && a.rangeNote[1] && *a.rangeNote[0] > *a.rangeNote[1] && a.rangeTake < 0) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420);
      ImGui::TextColored({1, 0.75f, 0.3f, 1}, "%s", tr("Your lowest note came out higher than your highest. Retry them, or save: the lower one counts as your lowest."));
      ImGui::PopTextWrapPos();
    }
    if (!a.rangeError.empty()) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420);
      ImGui::TextColored({1, 0.5f, 0.4f, 1}, "%s", a.rangeError.c_str());
      ImGui::PopTextWrapPos();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(!a.rangeNote[0] || !a.rangeNote[1] || a.rangeTake >= 0);
    if (ImGui::Button(L("Save").c_str())) {
      if (auto v = myVoice(a.rangeFrom, *a.rangeNote[0], *a.rangeNote[1])) {
        try {
          saveInstrument(*v);
          a.set.instrument = v->id;
          a.lastPoll = -10;  // judge the songs again now
          open = false;
        } catch (const std::exception& e) {
          a.rangeError = e.what();
        }
      } else a.rangeError = tr("These notes are less than a fifth apart. Sing the lowest and the highest note you can hold comfortably.");
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tr("Saves this range as My voice and makes it your instrument"));
    ImGui::SameLine();
    if (ImGui::Button(L("Cancel").c_str())) open = false;
  }
  ImGui::End();
  if (!open) {
    a.showRange = false;
    if (a.rangeTake >= 0 && !a.listen) a.audio.stopMic();
    a.rangeTake = -1;
  }
}
