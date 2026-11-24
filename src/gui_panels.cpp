// The main window: the song list, the song area (transport, score, details panel) and the open song (open, poll,
// editSong). Reloads automatically when the agent (john-mcp) changes a song.
#include "gui.hpp"
#include <algorithm>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// "Written for": the song's instrument, or "<name> (deleted)" when it was a custom instrument that no longer exists.
static std::string deletedName(const Song& s) { return findInstrument(s.instrument, s.whistle, s.harp) ? "" : s.instrumentName; }
static std::string writtenFor(const Instrument& in, const std::string& deleted) {
  return deleted.empty() ? instName(in) : trf("%s (deleted)", deleted.c_str());
}

// Song list badge for a song that fits my instrument in another key: "in G", or the same key an octave away.
static std::string fitBadge(const Playability& p) {
  if (p.verdict != Playability::OtherKey) return "";
  if (p.shift % 12 == 0) return p.shift > 0 ? tr("octave up") : tr("octave down");
  return trf("in %s", tonicName(p.key).c_str());
}

fs::path songPath(const std::string& id) { return dataDir() / "songs" / (id + ".json"); }

void open(App& a, const std::string& id) {
  if (id != a.current) a.sel = -1;
  a.cursor = -1;
  a.song = loadSong(id);
  a.track = a.song ? loadTrack(id) : std::nullopt;
  a.current = a.song ? id : "";
  std::error_code ec;
  a.songMtime = fs::last_write_time(songPath(id), ec);
  if (!a.song || a.sel >= (int)a.song->events.size()) a.sel = -1;
  snprintf(a.comment, sizeof a.comment, "%s", a.sel >= 0 ? songText(*a.song, a.song->events[a.sel].comment).c_str() : "");
}

// Apply a user edit to the selected note on top of the latest file, so concurrent agent edits are not lost.
template <class F> static void editSong(App& a, F f) {
  std::error_code ec;
  if (fs::last_write_time(songPath(a.current), ec) != a.songMtime) open(a, a.current);
  if (!a.song) return;
  f(*a.song);
  try {
    saveSong(*a.song);
  } catch (const std::exception&) {
    return;  // next poll reloads whatever is on disk
  }
  a.songMtime = fs::last_write_time(songPath(a.current), ec);
}

template <class F> static void editSelected(App& a, F f) {
  editSong(a, [&](Song& s) {
    if (a.sel >= 0 && a.sel < (int)s.events.size() && s.events[a.sel].kind == Kind::Note) f(s.events[a.sel]);
  });
}

void poll(App& a) {
  const std::string mine = a.set.instrument + " " + a.set.whistle + " " + a.set.harp;
  if (mine != a.playFor) {  // judge every song again for the new instrument
    a.playFor = mine;
    for (auto& [id, m] : a.meta) m.mtime = {};
    a.lastPoll = -10;
  }
  if (ImGui::GetTime() - a.lastPoll < 0.5) return;
  a.lastPoll = ImGui::GetTime();
  a.ids = listSongs();
  a.techs = loadTechniques();
  reloadInstruments();  // the agent may have defined or deleted one (define_instrument)
  if (auto cat = instruments(); cat != a.catalog) {
    a.catalog = cat;
    for (auto& [id, m] : a.meta) m.mtime = {};  // judged again below
  }
  std::error_code ec;
  std::string newest;
  fs::file_time_type newestT{};
  for (auto& id : a.ids) {
    auto t = fs::last_write_time(songPath(id), ec);
    if (!ec && t > newestT) { newestT = t; newest = id; }
    auto& m = a.meta[id];
    if (!ec && m.mtime != t) {
      auto s = loadSong(id);
      Instrument in = instrumentOf(s.value_or(Song{}));
      m = {t, s ? s->folder : "", s && !s->title.empty() ? songText(*s, s->title) : id, in, s ? deletedName(*s) : "",
           s ? playability(*s, myInstrument(a.set)) : Playability{}};  // an unreadable file stays listed
    }
  }
  std::erase_if(a.meta, [&](auto& kv) { return std::find(a.ids.begin(), a.ids.end(), kv.first) == a.ids.end(); });
  if (a.follow && !newest.empty() && newestT > a.newestSeen && newest != a.current) open(a, newest);  // follow the agent
  a.newestSeen = std::max(a.newestSeen, newestT);
  if (!a.current.empty()) {
    auto t = fs::last_write_time(songPath(a.current), ec);
    if (ec) { a.song.reset(); a.current.clear(); }
    else if (t != a.songMtime) open(a, a.current);
  }
}

// ---------------- panels ----------------
static void detailsPanel(App& a) {
  Song& s = *a.song;
  ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.4f);
  ImGui::TextWrapped("%s", songText(s, s.title).c_str());
  ImGui::PopFont();
  const Instrument inst = instrumentOf(s), me = myInstrument(a.set);  // written for vs what I play (#90)
  const bool whistle = me.hints && a.set.hints;
  const std::string written = writtenFor(inst, deletedName(s));
  ImGui::TextDisabled("%s  |  %s  |  %s%s", keyName(s.key).c_str(), s.meter.c_str(), written.c_str(),
                      s.tempo ? ("  |  " + std::to_string(s.tempo) + " bpm").c_str() : "");
  if (!s.source.empty()) ImGui::TextWrapped(tr("Source: %s"), s.source.c_str());
  std::vector<std::string> choices = FOLDERS;
  if (!s.folder.empty() && std::find(choices.begin(), choices.end(), s.folder) == choices.end()) choices.push_back(s.folder);
  choices.push_back("");
  bool edited = false;  // editSong may reload the song or empty a.song: don't use `s` after an edit this frame
  ImGui::SetNextItemWidth(200);
  if (ImGui::BeginCombo(L("Folder").c_str(), s.folder.empty() ? tr("Unsorted") : tr(s.folder.c_str()))) {
    for (auto& f : choices)
      if (ImGui::Selectable(f.empty() ? tr("Unsorted") : tr(f.c_str()), f == s.folder)) {
        std::string pick = f;
        editSong(a, [&](Song& x) { x.folder = pick; });
        edited = true;
        break;
      }
    ImGui::EndCombo();
  }
  if (edited) return;  // redraw from the fresh song next frame
  ImGui::SetNextItemWidth(200);
  if (ImGui::BeginCombo(L("Written for").c_str(), written.c_str(), ImGuiComboFlags_HeightLarge)) {
    for (auto& id : instrumentIds()) {
      std::string pick = id;
      std::string harp = id == "harmonica" && inst.id != "harmonica" ? a.set.harp : s.harp;  // a new harmonica song: Settings' harp
      if (ImGui::Selectable((instName(id, s.whistle, harp) + "###" + id).c_str(), id == s.instrument)) {
        editSong(a, [&](Song& x) {
          if (pick == "harmonica" && x.instrument != "harmonica") x.harp = harp;
          x.instrument = pick;
        });
        edited = true;
        break;
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The instrument this sheet was written for: its range and fingerings. Your own instrument is in Settings."));
  if (edited) return;
  if (inst.id == "harmonica") {
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo(L("Harmonica key").c_str(), keyLabel(s.harp).c_str())) {
      for (auto& k : HARP_KEYS)
        if (ImGui::Selectable((keyLabel(k) + "###" + k).c_str(), k == s.harp)) {
          std::string pick = k;
          editSong(a, [&](Song& x) { x.harp = pick; });
          edited = true;
          break;
        }
      ImGui::EndCombo();
    }
    if (edited) return;
  }
  // Play in key: the 12 tonics in the song's mode; the octave follows the instrument (transposeToKey)
  const int cur = tonicPc(s.key), orig = ((cur - s.transposed) % 12 + 12) % 12;
  ImGui::SetNextItemWidth(200);
  if (ImGui::BeginCombo(L("Play in key").c_str(), keyName(s.key).c_str(), ImGuiComboFlags_HeightLargest)) {  // all 12, no scrolling
    for (int pc = 0; pc < 12; pc++) {
      Song k;
      k.key = s.key;
      transpose(k, pc - cur);  // no notes: only the key
      std::string name = pc == orig ? trf("%s (original)", keyName(k.key).c_str()) : keyName(k.key);
      if (ImGui::Selectable((name + "###key" + std::to_string(pc)).c_str(), pc == cur)) {
        const int tonic = pc;
        if (tonic != cur) editSong(a, [&](Song& x) { transposeToKey(x, tonic, instrumentOf(x)); });
        edited = true;
        break;
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Transposes the song and saves it. Pick the original key to get it back."));
  if (edited) return;
  // The song fits my instrument (Settings) only transposed (the song list's badge, #84): one click. The cached verdict
  // is used only while it belongs to this version of the file, so a second click can't transpose twice.
  if (auto m = a.meta.find(a.current); m != a.meta.end() && m->second.mtime == a.songMtime && m->second.play.verdict == Playability::OtherKey) {
    const int shift = m->second.play.shift;
    const std::string mine = instName(myInstrument(a.set));
    ImGui::TextWrapped(tr("Your %s can play it transposed:"), mine.c_str());
    std::string label = shift % 12 ? trf("Play in %s", keyName(m->second.play.key).c_str())
                      : shift > 0  ? std::string(tr("Play an octave higher")) : std::string(tr("Play an octave lower"));
    if (ImGui::Button((label + "###fit").c_str())) {
      editSong(a, [&](Song& x) { transpose(x, shift); });
      a.lastPoll = -10;  // judge it again now
      return;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Transposes the song and saves it. Pick the original key to get it back."));
  }
  if (me.id == "harmonica") {  // the usual keys for my harp
    Key harp = parseKey(a.set.harp).value_or(Key{0, "maj"}), second{harp.fifths + 1, "maj"};
    ImGui::TextDisabled(tr("1st position: %s  |  2nd position: %s"), tonicName(harp).c_str(), tonicName(second).c_str());
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", tr("1st position plays in the harmonica's key. 2nd position (cross harp), a fifth above, is the usual choice for blues and folk."));
  }
  if (!s.notes.empty()) { ImGui::Separator(); ImGui::TextWrapped("%s", songText(s, s.notes).c_str()); }
  ImGui::Separator();

  if (a.sel >= 0 && a.sel < (int)s.events.size() && s.events[a.sel].kind == Kind::Note) {
    Event& e = s.events[a.sel];
    ImGui::Text(tr("Note #%d: %s, %s"), a.sel, noteName(spell(s.key, e.midi)).c_str(), durationName(e).c_str());
    if (whistle) ImGui::Text(tr("Fingering: %s"), fingeringText(finger(me.key, e.midi)).c_str());
    else if (!me.plays(e.midi)) ImGui::TextColored({1, 0.4f, 0.4f, 1}, tr("Out of range for the %s"), instName(me).c_str());
    for (auto& t : a.techs) {
      bool on = std::find(e.techniques.begin(), e.techniques.end(), t.id) != e.techniques.end();
      if (ImGui::Checkbox((t.symbol + "  " + t.name + "###" + t.id).c_str(), &on)) {
        editSelected(a, [&](Event& n) {
          n.techniques.erase(std::remove(n.techniques.begin(), n.techniques.end(), t.id), n.techniques.end());
          if (on) n.techniques.push_back(t.id);
        });
        break;  // the song may have been reloaded; redraw next frame
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.description.c_str());
    }
    ImGui::InputTextMultiline("##comment", a.comment, sizeof a.comment, {-1, ImGui::GetTextLineHeight() * 4});
    // unchanged box = the translation it was filled with: don't overwrite the English with it
    if (ImGui::Button(L("Save comment").c_str()) && a.comment != songText(s, e.comment))
      editSelected(a, [&](Event& n) { n.comment = a.comment; });
  } else ImGui::TextDisabled("%s", tr("Click a note to see and edit its techniques."));

  ImGui::Separator();
  if (ImGui::CollapsingHeader(L("Legend").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    if (whistle) ImGui::TextWrapped("%s", tr("Fingering chart: top three holes = left hand, bottom three = right hand. Filled = covered, empty = open, half = half-covered. + = blow harder (second octave)."));
    // marks my instrument can't play are hidden (#86): say how many, and let them all be shown
    if (int n = a.set.allMarks ? 0 : hiddenMarks(s, me)) {
      ImGui::TextColored({0.95f, 0.55f, 0.15f, 1}, "%s", (n == 1 ? trf("1 %s ornament hidden", written.c_str()) : trf("%d %s ornaments hidden", n, written.c_str())).c_str());
      if (ImGui::IsItemHovered()) ImGui::SetTooltip(tr("Your %s doesn't play them. The song keeps them: hover a note to see its marks."), instName(me).c_str());
    }
    ImGui::Checkbox(L("Show all marks").c_str(), &a.set.allMarks);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Show every technique mark of the song, also those your instrument doesn't play"));
    if (std::any_of(s.events.begin(), s.events.end(), [](const Event& e) { return !e.lyric.empty(); }))
      ImGui::Checkbox(L("Show lyrics").c_str(), &a.set.lyrics);
    const bool breathes = std::count(me.techniques.begin(), me.techniques.end(), "breath") > 0;
    for (auto& t : a.techs) {
      if (!a.set.allMarks && marks({t.id}, me).shown != std::vector<std::string>{t.id}) continue;  // never on my sheet
      const bool lift = t.id == "breath" && !breathes && !a.set.allMarks;  // the same mark ends a phrase on a fiddle or piano
      ImGui::TextColored({0.95f, 0.55f, 0.15f, 1}, "%-4s", t.symbol.c_str());
      ImGui::SameLine();
      ImGui::Text("%s", lift ? tr("Phrase end (lift)") : t.name.c_str());
      ImGui::Indent();
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      ImGui::TextWrapped("%s", lift ? tr("End of a phrase: lift off for a moment after this note before going on.") : t.description.c_str());
      ImGui::PopStyleColor();
      ImGui::Unindent();
    }
  }
}

// The left column: the songs by folder (those that fit my instrument, #84), the sheet toggles and the window buttons.
// Returns the image a song was read from (#75, web) when it replaces the details panel, else null.
const Original* songList(App& a) {
  ImGui::BeginChild("songs", {220, 0}, ImGuiChildFlags_Borders);
  ImGui::TextDisabled("%s", tr("SONGS"));
  // Playable on my instrument (#84): songs are melodies; show those that fit the Settings instrument, in any key
  const std::string mine = instName(myInstrument(a.set)), playable = trf("Playable on my %s", mine.c_str());
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##songFilter", a.set.allSongs ? tr("All songs") : playable.c_str())) {
    if (ImGui::Selectable(playable.c_str(), !a.set.allSongs)) a.set.allSongs = false;
    if (ImGui::Selectable(tr("All songs"), a.set.allSongs)) a.set.allSongs = true;
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", playable.c_str(), tr("Change your instrument in Settings"));  // the label may be clipped
  std::map<std::string, std::vector<std::string>, decltype(&folderBefore)> byFolder(&folderBefore);
  for (auto& id : a.ids)
    if (a.set.allSongs || a.meta[id].play.verdict != Playability::NoFit) byFolder[a.meta[id].folder].push_back(id);
  for (auto& [folder, ids] : byFolder) {
    std::string label = tr(folder.empty() ? "Unsorted" : folder.c_str()) + std::string(" (") + std::to_string(ids.size()) + ")###folder:" + folder;  // never an empty ID: Unsorted is folder ""
    if (!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) continue;
    for (auto& id : ids) {
      auto& m = a.meta[id];
      std::string tag = fitBadge(m.play);  // after the title: the key it fits my instrument in
      if (m.play.verdict == Playability::NoFit) tag = tr("doesn't fit");  // only in All songs
      float x = ImGui::GetCursorPosX();
      if (ImGui::Selectable((m.title + "##" + id).c_str(), id == a.current)) { a.sel = -1; open(a, id); }
      if (ImGui::IsItemHovered()) {  // long titles get clipped
        std::string fit = m.play.verdict == Playability::AsWritten ? trf("Fits your %s as written", mine.c_str())
                        : m.play.verdict == Playability::OtherKey  ? trf("Fits your %s transposed (%s): open it to transpose in one click", mine.c_str(), tag.c_str())
                        : m.play.cost.out ? trf("Doesn't fit your %s: %d notes out of range at best", mine.c_str(), m.play.cost.out)
                                          : trf("Doesn't fit your %s: too many hard notes in every key", mine.c_str());
        ImGui::SetTooltip("%s\n%s\n%s", m.title.c_str(), trf("Written for %s", writtenFor(m.in, m.deleted).c_str()).c_str(), fit.c_str());
      }
      if (!tag.empty()) {
        ImGui::SameLine(x + ImGui::CalcTextSize(m.title.c_str()).x + 6);
        ImGui::TextDisabled("%s", tag.c_str());
      }
    }
    ImGui::TreePop();
  }
#ifdef __EMSCRIPTEN__  // no agent on the web: point to the desktop app instead
  if (a.ids.empty()) ImGui::TextWrapped("%s", tr("No songs yet. Add one with Import song below."));
#else
  if (a.ids.empty()) ImGui::TextWrapped("%s", tr("No songs yet. Ask the agent to import one via the john MCP server."));
#endif
  else if (byFolder.empty()) ImGui::TextWrapped("%s", tr("No song fits your instrument. Pick All songs to see every song."));
  ImGui::Separator();
#ifdef __EMSCRIPTEN__
  if (ImGui::TextLink(L("Get the desktop app").c_str())) emscripten_run_script("window.open('https://github.com/Thinato/john/releases', '_blank')");
  ImGui::TextWrapped("%s", tr("Adds the AI agent (MCP), audio transcription and songs on disk."));
#else
  ImGui::Checkbox(L("Follow agent").c_str(), &a.follow);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Switch to a song as soon as the agent changes it"));
#endif
  const bool noHints = !myInstrument(a.set).hints;  // only the whistle has hints so far
  ImGui::BeginDisabled(noHints);
  bool shown = a.set.hints && !noHints;  // unticked while disabled; a.set.hints keeps the choice for a whistle
  if (ImGui::Checkbox(L("Instrument hints").c_str(), &shown) && !noHints) a.set.hints = shown;
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (noHints) ImGui::SetTooltip(tr("No hints for the %s yet"), instName(myInstrument(a.set)).c_str());
    else ImGui::SetTooltip("%s", tr("Show fingering charts and fingerings for the notes"));
  }
  ImGui::SliderFloat(L("Zoom").c_str(), &a.set.zoom, 6, 16, "%.0f");
#ifdef __EMSCRIPTEN__
  const Original* original = a.originalOf.count(a.current) ? &a.originals[a.originalOf[a.current]] : nullptr;
  if (original) ImGui::Checkbox(L("Show the original image").c_str(), &a.showOriginal);
  if (original && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The image this song was read from, next to the sheet"));
  if (!a.showOriginal) original = nullptr;
#endif
  if (ImGui::Button(L("Import song").c_str())) a.showAbc = !a.showAbc;
#ifdef __EMSCRIPTEN__
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Add songs from ABC notation, MusicXML, or a photo, scan or PDF of sheet music"));
#else
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Add songs from ABC notation or MusicXML: paste ABC or open a file"));
#endif
  if (ImGui::Button(L("Settings").c_str())) a.showSettings = !a.showSettings;
#ifndef __EMSCRIPTEN__  // the web keeps songs in the browser (IndexedDB) and has no transcription
  ImGui::SameLine();
  if (ImGui::Button(L("Import audio").c_str())) {
    if (a.showImport) a.showImport = false;
    else openImport(a);
  }
  ImGui::TextDisabled("%s", tr("Data folder:"));
  ImGui::TextWrapped("%s", dataDir().string().c_str());
#endif
  ImGui::EndChild();
#ifdef __EMSCRIPTEN__
  return original;
#else
  return nullptr;
#endif
}

// Next to the song list: the transport bar and the score, then the details panel (or the original image, web).
void songArea(App& a, const Original* original) {
  if (a.song) {
    float detailsW = 340;
#ifdef __EMSCRIPTEN__
    if (original) detailsW = ImGui::GetContentRegionAvail().x * 0.45f;  // the original takes the details panel's place
#endif
    float scoreW = ImGui::GetContentRegionAvail().x - detailsW - 8;
    ImGui::BeginGroup();
    ImGui::BeginChild("transport", {scoreW, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    transportBar(a);
    ImGui::EndChild();
    ImGui::BeginChild("score", {scoreW, 0}, ImGuiChildFlags_Borders);
    drawScore(a, *a.song);
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginChild("details", {0, 0}, ImGuiChildFlags_Borders);
#ifdef __EMSCRIPTEN__
    if (original) originalPanel(a, *original);
    else
#endif
      detailsPanel(a);
    ImGui::EndChild();
  }
}
