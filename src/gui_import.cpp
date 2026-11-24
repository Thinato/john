// Import song (paste ABC, open ABC or MusicXML; images and PDFs through the web's music reader, #75) and Import audio
// (desktop: transcription on a worker thread).
#include "gui.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef __EMSCRIPTEN__
std::string gDropped;  // a file dropped on the window (GLFW drop callback), for Import audio

void openImport(App& a) {
  a.showImport = true;
  a.importError.clear();
  a.importNote.clear();
  if (!a.busy) a.importInst = myInstrument(a.set).id;  // default from Settings (the D whistle if mine is missing)
}

// Import audio: transcribe a recording into a new song. importAudio() runs on a worker thread so the GUI never
// blocks; the main loop polls the future and opens the song when it is ready.
void importWindow(App& a) {
  if (!gDropped.empty()) {
    snprintf(a.importPath, sizeof a.importPath, "%s", gDropped.c_str());
    gDropped.clear();
    openImport(a);
  }
  if (a.busy && a.importing.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    a.busy = false;
    try {
      Song s = a.importing.get();
      open(a, s.id);
      std::istringstream notes(s.notes);  // with separation the window stays open to say whether Demucs ran
      for (std::string l; std::getline(notes, l);)
        if (l.rfind("separation:", 0) == 0) a.importNote = l;
      a.showImport = !a.importNote.empty();
    } catch (const std::exception& e) {
      a.importError = trf("Couldn't transcribe this file: %s", e.what());
    }
  }
  if (!a.showImport) return;
  ImGui::SetNextWindowPos({260, 80}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(1);
  if (!ImGui::Begin(L("Import audio").c_str(), &a.showImport, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
  ImGui::TextUnformatted(tr("Drop an audio file (mp3, wav or flac) on the John window, or paste its path."));
  ImGui::BeginDisabled(a.busy);
  ImGui::PushItemWidth(320);
  ImGui::InputText(L("File").c_str(), a.importPath, sizeof a.importPath);
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(150);
  ImGui::InputTextWithHint(L("Start (seconds)").c_str(), "0", a.importStart, sizeof a.importStart);
  ImGui::InputTextWithHint(L("End (seconds)").c_str(), tr("end of the file"), a.importEnd, sizeof a.importEnd);
  ImGui::InputTextWithHint(L("Tempo (bpm)").c_str(), tr("detect"), a.importTempo, sizeof a.importTempo);
  ImGui::InputTextWithHint(L("Meter").c_str(), tr("detect"), a.importMeter, sizeof a.importMeter);
  instrumentCombo(L("Instrument").c_str(), a.importInst, nullptr);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The instrument the sheet is for"));
  instrumentCombo(L("Recording instrument").c_str(), a.importRec, tr("not set"));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Who plays the melody in the recording, if you know: John then listens only in its range"));
  const char* stems[] = {tr("off"), tr("vocals (singer)"), tr("other (instrument)")};
  ImGui::Combo(L("Separate the melody (Demucs)").c_str(), &a.importSep, stems, 3);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", tr("For a dense mix where the draft follows the band: Demucs first pulls out the singer (vocals) or the lead instrument (other: whistle, flute, fiddle, piano), and John transcribes only that."));
  ImGui::PopItemWidth();
  ImGui::Checkbox(L("Rubato").c_str(), &a.importRubato);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", tr("Turn on when the tempo speeds up and slows down (slow airs, film music, free playing): the beats follow the recording. Leave it off for steady tunes."));
  if (a.importSep) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
    ImGui::TextDisabled("%s", tr("Demucs must be installed (pip install demucs, or set JOHN_DEMUCS to its path). It is slow: transcribe a short section with Start and End."));
    ImGui::PopTextWrapPos();
  }
  if (ImGui::Button(L("Transcribe").c_str())) {
    auto num = [](const char* t, double& v) {  // blank = 0
      char* e;
      v = strtod(t, &e);
      while (*e == ' ') e++;
      return *e == 0 && v >= 0;
    };
    double start, end, bpm;
    TranscribeOptions o;
    o.meter = parseMeter(a.importMeter);
    o.instrument = a.importInst;
    o.recordingInstrument = a.importRec;
    o.separate = a.importSep == 1 ? "vocals" : a.importSep == 2 ? "other" : "";
    o.rubato = a.importRubato;
    std::error_code ec;
    if (!fs::is_regular_file(a.importPath, ec)) a.importError = tr("File not found.");
    else if (!num(a.importStart, start) || !num(a.importEnd, end)) a.importError = tr("Start and end are seconds into the recording.");
    else if (!num(a.importTempo, bpm)) a.importError = tr("Tempo is quarter notes per minute, or blank to detect it.");
    else if (o.meter.empty() && a.importMeter[strspn(a.importMeter, " ")]) a.importError = tr("Meter is like 4/4, 3/4 or 6/8, or blank to detect it.");
    else {
      a.importError.clear();
      a.importNote.clear();
      o.bpm = bpm;
      a.busy = true;
      a.importing = std::async(std::launch::async, [path = std::string(a.importPath), start, end, o] { return importAudio(path, start, end, o); });
    }
  }
  ImGui::EndDisabled();
  if (a.busy) { ImGui::SameLine(); ImGui::TextUnformatted(tr("Transcribing...")); }
  if (!a.importError.empty()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
    ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", a.importError.c_str());
    ImGui::PopTextWrapPos();
  }
  if (!a.importNote.empty()) {  // the report's separation: line (English, like the song notes it comes from)
    bool mix = a.importNote.find("full mix") != std::string::npos;
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
    ImGui::TextColored(mix ? ImVec4{1, 0.4f, 0.4f, 1} : ImVec4{0.4f, 0.8f, 0.4f, 1}, "%s",
                       mix ? tr("The song is open, but Demucs didn't separate the melody: John transcribed the full mix.") : tr("The song is open. Demucs separated the melody."));
    ImGui::TextUnformatted(a.importNote.c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::End();
}
#endif

#ifdef __EMSCRIPTEN__
// Browser file picker. ABC and MusicXML files are written to /picked/<name> (memory, not IndexedDB); abcWindow()
// imports them on the next frame. Images and PDFs go to the sheet-music reader (web/omr/omr.js, #75), which reads them
// next to this module and writes /omr/result.musicxml and /omr/original when done. Called from an ImGui button, which
// runs within the click's user activation the browser requires.
EM_JS(void, pickFile, (), {
  var input = document.createElement('input');
  input.type = 'file';
  input.multiple = true;
  input.accept = '.abc,.txt,text/plain,.musicxml,.xml,.mxl,image/*,.heic,.heif,.pdf,application/pdf';
  input.onchange = async function() {
    var exts = ['pdf', 'png', 'jpg', 'jpeg', 'webp', 'gif', 'bmp', 'heic', 'heif', 'avif'];  // no regexes: EM_JS is a C string
    var picture = (f) => f.type.startsWith('image/') || f.type == 'application/pdf' || exts.includes(f.name.split('.').pop().toLowerCase());
    var files = Array.from(input.files);
    for (var f of files.filter((f) => !picture(f))) {
      var data = new Uint8Array(await f.arrayBuffer());
      try { FS.mkdir('/picked'); } catch (e) {}
      FS.writeFile('/picked/' + f.name, data);
    }
    var pictures = files.filter(picture);
    if (pictures.length) (await import('./omr/omr.js')).read(pictures, FS);
  };
  input.click();
});
EM_JS_DEPS(omr, "$stringToNewUTF8");
// The reader's state as JSON (omr.js `state`: stage idle/open/download/start/read/done/error, ...); free() it.
EM_JS(char*, omrState, (), { return stringToNewUTF8(JSON.stringify(globalThis.johnOmr ? globalThis.johnOmr.state : {stage: 'idle'})); });
EM_JS(void, omrCancel, (), { if (globalThis.johnOmr) globalThis.johnOmr.cancel(); });  // stop, and back to idle

static std::optional<Original> loadOriginal(const std::string& bytes) {  // width, height (uint32 LE), RGBA
  uint32_t w = 0, h = 0;
  if (bytes.size() < 8) return std::nullopt;
  memcpy(&w, bytes.data(), 4);
  memcpy(&h, bytes.data() + 4, 4);
  if (!w || !h || bytes.size() != 8 + (size_t)w * h * 4) return std::nullopt;
  Original o{0, (int)w, (int)h};
  glGenTextures(1, &o.tex);
  glBindTexture(GL_TEXTURE_2D, o.tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, o.w, o.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data() + 8);
  return o;
}
#endif

static std::optional<std::string> readText(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return std::nullopt;
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// The instrument the Import song window imports for: the user's pick, else Settings' instrument and harp.
static Song importLike(const App& a) {
  Song like;
  const Instrument me = myInstrument(a.set);  // the D whistle while my custom instrument's file is missing
  like.instrument = a.abcInst.empty() ? me.id : a.abcInst;
  like.whistle = a.abcWhistle.empty() ? (me.id == "whistle" && !me.key.empty() ? me.key : a.set.whistle) : a.abcWhistle;
  like.harp = a.abcHarp.empty() ? a.set.harp : a.abcHarp;
  return like;
}

// Every tune in the ABC, or the MusicXML score (text or .mxl bytes), becomes a song for the window's instrument
// (importLike); the first one is opened. A song with many notes out of that instrument's range gets a line pointing to
// Play in key (#67); nothing is transposed. Returns the new songs' ids.
static std::vector<std::string> importText(App& a, const std::string& data, const std::string& source) {
  Song like = importLike(a);
  like.source = source;
  a.abcError.clear();
  a.abcResult.clear();
  std::vector<std::string> ids;
  try {
    auto r = importSongs(data, like);  // saveSong() calls persist() on the web
    std::error_code ec;
    for (auto& x : r) {
      a.newestSeen = std::max(a.newestSeen, fs::last_write_time(songPath(x.song.id), ec));  // poll() mustn't follow to the last
      for (auto& w : x.warnings) a.abcResult += "\n" + w;
      ids.push_back(x.song.id);
      int notes = 0, out = rangeCost(x.song, instrumentOf(x.song), 0).out;
      for (auto& e : x.song.events) notes += e.kind == Kind::Note;
      if (out * 100 >= notes * PLAYABLE_PERCENT && out > 0)  // ponytail: "many" = PLAYABLE_PERCENT of the notes; a lone stray note isn't worth a line
        a.abcResult += "\n" + (r.size() > 1 ? songText(x.song, x.song.title) + ": " : std::string()) +
                       trf("%d of %d notes are out of the %s's range. To move the song, pick another key in Play in key next to the sheet.", out, notes, instName(instrumentOf(x.song)).c_str());
    }
    a.abcResult = trf("Imported %d song(s).", (int)r.size()) + a.abcResult;
    open(a, r[0].song.id);
    a.lastPoll = -1;  // list them now
  } catch (const std::exception& e) {
    a.abcError = tr(std::string(e.what()));
  }
  return ids;
}

#ifdef __EMSCRIPTEN__
// The image reader is done (omr.js wrote /omr/result.musicxml): check the first line's key (checkFirstLineKey), import
// the score and keep the first page to show next to the new songs.
static void importRead(App& a, std::string xml, const json& state) {
  auto key = checkFirstLineKey(xml);
  auto ids = importText(a, xml, state.value("name", ""));
  if (ids.empty()) return;
  auto sig = [](int fifths) { return keyName(Key{fifths, "maj"}); };
  if (key && key->fixed)
    a.abcResult += "\n" + trf("The first line had no key signature but the rest is in %s, so John used that key for the first line too. Check it against the original.", sig(key->rest).c_str());
  else if (key)
    a.abcResult += "\n" + trf("The first line is in %s but the rest is in %s. Check the key against the original.", sig(key->first).c_str(), sig(key->rest).c_str());
  if (state.value("upscaled", 0) > 0) a.abcResult += "\n" + std::string(tr("The image was small, so John enlarged it 2x before reading."));
  a.abcResult += "\n" + trf("Read %d page(s) in %.1f s.", state.value("pages", 1), state.value("ms", 0) / 1000.0);
  auto px = readText("/omr/original");
  std::error_code ec;
  fs::remove("/omr/original", ec);
  if (auto o = px ? loadOriginal(*px) : std::nullopt) {
    a.originals.push_back(*o);
    for (auto& id : ids) a.originalOf[id] = a.originals.size() - 1;
    a.showOriginal = true;
  }
}
#endif

#ifndef __EMSCRIPTEN__
static void importFile(App& a) {
  std::error_code ec;
  auto text = fs::is_regular_file(a.abcPath, ec) ? readText(a.abcPath) : std::nullopt;
  if (text) importText(a, *text, fs::path(a.abcPath).filename().string());
  else { a.abcResult.clear(); a.abcError = tr("File not found."); }
}
#endif

// Import song: paste ABC, or open an ABC or MusicXML file (web: browser file picker; desktop: a path, or drop the file on
// the window).
void abcWindow(App& a) {
#ifdef __EMSCRIPTEN__
  std::error_code ec;
  std::vector<fs::path> picked;
  for (auto& e : fs::directory_iterator("/picked", ec)) picked.push_back(e.path());
  for (auto& p : picked) {
    auto text = readText(p);
    fs::remove(p, ec);
    if (text) importText(a, *text, p.filename().string());
  }
  json omr = json::parse(std::unique_ptr<char, decltype(&free)>(omrState(), free).get());
  const std::string stage = omr.value("stage", "idle");
  if (auto xml = readText("/omr/result.musicxml")) {
    fs::remove("/omr/result.musicxml", ec);
    importRead(a, *xml, omr);
    omrCancel();
  } else if (stage == "error") {
    const std::string err = omr.value("error", "");
    a.abcResult.clear();
    a.abcError = err == "heic"       ? tr("This browser can't open HEIC photos. Save the photo as JPG or PNG and open that.")
                 : err == "decode"   ? tr("Couldn't open this image.")
                 : err == "pdf"      ? tr("Couldn't open this PDF.")
                 : err == "download" ? tr("Couldn't download the music reader. Check the internet connection and try again.")
                                     : tr("Couldn't read this sheet music.");
    a.abcError += " (" + omr.value("detail", "") + ")";
    omrCancel();
  }
#else
  if (!gDropped.empty()) {  // ABC and MusicXML only: anything else stays for Import audio
    std::string ext = fs::path(gDropped).extension().string();
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    if (ext == ".abc" || ext == ".musicxml" || ext == ".xml" || ext == ".mxl") {
      snprintf(a.abcPath, sizeof a.abcPath, "%s", gDropped.c_str());
      gDropped.clear();
      a.showAbc = true;
      importFile(a);
    }
  }
#endif
  if (!a.showAbc) return;
  ImGui::SetNextWindowPos({280, 100}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(1);
  if (!ImGui::Begin(L("Import song").c_str(), &a.showAbc, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
  const float w = ImGui::GetFontSize() * 32;
  // the instrument every import from this window is for; names as in the details panel (instName, keyLabel)
  const Song like = importLike(a);
  const Instrument as = instrumentOf(like);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
  if (ImGui::BeginCombo(L("Instrument").c_str(), instName(as).c_str(), ImGuiComboFlags_HeightLarge)) {
    for (auto& id : instrumentIds())
      if (ImGui::Selectable((instName(id, like.whistle, like.harp) + "###" + id).c_str(), id == as.id))
        a.abcInst = id;
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The instrument the sheet is for"));
  if (as.id == "whistle" || as.id == "harmonica") {
    const bool whistle = as.id == "whistle";
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    if (ImGui::BeginCombo(whistle ? L("Whistle key").c_str() : L("Harmonica key").c_str(), keyLabel(whistle ? like.whistle : like.harp).c_str())) {
      for (auto& k : HARP_KEYS)
        if (ImGui::Selectable((keyLabel(k) + "###" + k).c_str(), k == (whistle ? like.whistle : like.harp))) (whistle ? a.abcWhistle : a.abcHarp) = k;
      ImGui::EndCombo();
    }
  }
  ImGui::PushTextWrapPos(w);
  ImGui::TextUnformatted(tr("Find tunes on thesession.org or abcnotation.com, copy the ABC and paste it here."));
  ImGui::PopTextWrapPos();
  ImGui::InputTextMultiline("##abc", a.abcText.data(), a.abcText.size(), {w, ImGui::GetTextLineHeight() * 14});
  if (ImGui::Button(L("Import").c_str())) {
    importText(a, a.abcText.data(), tr("pasted"));
    if (a.abcError.empty()) a.abcText[0] = 0;
  }
  ImGui::SameLine();
#ifdef __EMSCRIPTEN__
  const bool reading = stage != "idle" && stage != "done" && stage != "error";
  if (ImGui::Button(L("Open a file...").c_str())) pickFile();  // images or a PDF replace a read in progress
  ImGui::SameLine();
  ImGui::TextDisabled("%s", tr("ABC, MusicXML, or a photo, scan or PDF of sheet music"));
  ImGui::PushTextWrapPos(w);
  ImGui::TextDisabled("%s", tr("Sheet music in an image or PDF is read by a music reader that runs in this browser: the image never leaves your device. Clean printed scores read best; photos and handwriting read worse, so compare the result with the original. The first time, the reader is downloaded once (about 22 MB)."));
  ImGui::PopTextWrapPos();
  if (reading) {
    int page = omr.value("page", 0), pages = omr.value("pages", 0);
    double ratio = omr.value("ratio", 0.0), loaded = omr.value("loaded", 0.0), total = omr.value("total", 0.0);
    float done = 0;
    std::string label;
    if (stage == "download") { done = total > 0 ? float(loaded / total) : 0; label = trf("Downloading the music reader (first time only): %d%%", int(done * 100)); }
    else if (stage == "start") label = tr("Starting the music reader...");
    else if (stage == "read") { done = pages ? float((page - 1 + ratio) / pages) : 0; label = trf("Reading page %d of %d...", page, pages); }
    else label = tr("Opening the file...");
    ImGui::ProgressBar(done, {w - ImGui::CalcTextSize(tr("Cancel")).x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x, 0}, label.c_str());
    ImGui::SameLine();
    if (ImGui::Button(L("Cancel").c_str())) omrCancel();
  }
#else
  ImGui::TextUnformatted(tr("or drop an ABC or MusicXML file on the window, or type its path:"));
  ImGui::SetNextItemWidth(w - ImGui::CalcTextSize(tr("Open")).x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x);
  ImGui::InputText("##abcPath", a.abcPath, sizeof a.abcPath);
  ImGui::SameLine();
  if (ImGui::Button(L("Open").c_str())) importFile(a);
#endif
  ImGui::PushTextWrapPos(w);
  if (!a.abcError.empty()) ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", a.abcError.c_str());
  if (!a.abcResult.empty()) ImGui::TextUnformatted(a.abcResult.c_str());
  ImGui::PopTextWrapPos();
#ifdef __EMSCRIPTEN__
  if (ImGui::TextLink(L("Licences of the music reader").c_str())) emscripten_run_script("window.open('omr/NOTICE.txt', '_blank')");
#endif
  ImGui::End();
}

#ifdef __EMSCRIPTEN__
// The image a song was read from (#75), in place of the details panel: zoom, and scroll to compare it with the sheet.
void originalPanel(App& a, const Original& o) {
  ImGui::TextDisabled("%s", tr("ORIGINAL"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
  ImGui::SliderFloat((std::string(tr("Zoom")) + "###originalZoom").c_str(), &a.originalZoom, 1, 4, "%.1fx");
  ImGui::PushTextWrapPos(0);
  ImGui::TextDisabled("%s", tr("Compare the sheet with the image it was read from: the reader can miss or misread notes, more often in photos and handwriting."));
  ImGui::PopTextWrapPos();
  ImGui::BeginChild("originalImage", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
  float w = ImGui::GetContentRegionAvail().x * a.originalZoom;
  ImGui::Image(ImTextureRef((ImTextureID)o.tex), {w, w * o.h / o.w});
  ImGui::EndChild();
}
#endif
