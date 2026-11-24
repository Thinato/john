// john: GUI viewer. Shows songs from the data folder as staff notation with a whistle fingering chart and technique
// marks under/over each note. Reloads automatically when the agent (john-mcp) changes a song.
#include "gui.hpp"
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <GLFW/emscripten_glfw3.h>
#include <emscripten.h>
#endif
#ifndef GL_BGRA  // Windows' gl.h stops at OpenGL 1.1
#define GL_BGRA 0x80E1
#endif

ImFont* gMusic = nullptr;  // Bravura (SMuFL)
ImFont* gText = nullptr;
float gMusicScale = 1.0f;  // ImGui sizes fonts by line height; Bravura's is tall, so rescale to 1 em = size

std::string utf8(unsigned cp) {
  std::string s;
  if (cp < 0x80) s += char(cp);
  else if (cp < 0x800) { s += char(0xC0 | (cp >> 6)); s += char(0x80 | (cp & 0x3F)); }
  else { s += char(0xE0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 0x3F)); s += char(0x80 | (cp & 0x3F)); }
  return s;
}

// Translated widget label: "###" + the English text keeps the ImGui ID the same in every language ("##" would not).
std::string L(const char* english) { return std::string(tr(english)) + "###" + english; }

// A whistle or harmonica key ("Bb") spelled like key names ("Sib" with Do Re Mi keys).
std::string keyLabel(const std::string& k) {
  auto key = parseKey(k);
  return key ? tonicName(*key) : k;
}

// Instrument name in the viewer's language; a whistle's or harp's key is spelled like key names ("Flauta irlandesa em Ré").
// A custom profile (#85) keeps its own name, even with a built-in's id ("Low D Clarke" for whistle).
std::string instName(const Instrument& in) {
  if (!in.custom && in.id == "whistle") return trf("%s whistle", keyLabel(in.key).c_str());
  if (!in.custom && in.id == "harmonica" && in.key != "C") return trf("%s harmonica", keyLabel(in.key).c_str());  // C = "Harmonica"
  return tr(in.name);
}

// The Settings instrument (#84): the song list shows what it can play. One whose custom file is missing, broken or
// deleted reads as the D whistle; Settings keep its id until a real delete (#85).
Instrument myInstrument(const Settings& s) {
  if (auto i = findInstrument(s.instrument, s.whistle, s.harp)) return *i;
  return instrumentOf(Song{});
}

// An instrument's name by id, in the given whistle/harp key if it has one; the id itself if it is gone.
std::string instName(const std::string& id, const std::string& whistle, const std::string& harp) {
  auto in = findInstrument(id, whistle, harp);
  if (!in) in = findInstrument(id);
  return in ? instName(*in) : id;
}

// The text font and Bravura: from the release package, else the build folder. gMusic stays null without Bravura.
static void loadFonts(ImGuiIO& io) {
  std::string textFont = bundled("fonts/NotoSans-Regular.ttf", JOHN_TEXT_FONT).string();  // release package, else build dir
  std::string musicFont = bundled("fonts/Bravura.otf", JOHN_FONT).string();
  if (fs::exists(textFont)) gText = io.Fonts->AddFontFromFileTTF(textFont.c_str(), 16.0f);
  if (!gText) gText = io.Fonts->AddFontDefault();
  if (fs::exists(musicFont)) gMusic = io.Fonts->AddFontFromFileTTF(musicFont.c_str(), 36.0f);
}

// JOHN_SNAPSHOT (dev aid): the frame just rendered, as an uncompressed 32-bit .tga.
static void writeSnapshot(const char* snapshot, int w, int h) {
  std::vector<unsigned char> px((size_t)w * h * 4);
  glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
  FILE* f = fopen(snapshot, "wb");
  unsigned char hdr[18] = {0, 0, 2};
  hdr[12] = w & 255; hdr[13] = w >> 8; hdr[14] = h & 255; hdr[15] = h >> 8; hdr[16] = 32;
  if (f) { fwrite(hdr, 1, 18, f); fwrite(px.data(), 1, px.size(), f); fclose(f); }
}

// JOHN_SNAPSHOT_* (dev aid, read once and only with JOHN_SNAPSHOT): the state to snapshot, set without clicking.
// How to snapshot each window: kb/notes/gui-snapshot-convert-full-image.md, scripts/snap.sh.
struct Snap { int frames = 5, cursor = -1; std::vector<float> mic; };

static Snap snapSetup(App& a) {
  auto env = [](const char* k) { const char* v = std::getenv(k); return std::string(v ? v : ""); };
  auto list = [](const std::string& v) {
    std::vector<std::string> out;
    for (size_t i = 0; i < v.size();) {
      size_t j = std::min(v.find(',', i), v.size());
      if (j > i) out.push_back(v.substr(i, j - i));
      i = j + 1;
    }
    return out;
  };
  auto num = [&](const char* k, int def) { auto v = env(k); return v.empty() ? def : std::atoi(v.c_str()); };
  Snap sn;
  sn.frames = std::max(1, num("JOHN_SNAPSHOT_FRAMES", 5));
  sn.cursor = num("JOHN_SNAPSHOT_CURSOR", -1);
  a.snapHover = num("JOHN_SNAPSHOT_HOVER", -1);
  if (auto id = env("JOHN_SNAPSHOT_SONG"); !id.empty()) { a.follow = false; open(a, id); }
  for (auto& w : list(env("JOHN_SNAPSHOT_OPEN"))) {
    if (w == "settings") a.showSettings = true;
    else if (w == "range-test") a.showRange = true, a.rangeFrom = myInstrument(a.set);
    else if (w == "import") a.showAbc = true;
#ifndef __EMSCRIPTEN__
    else if (w == "import-audio") openImport(a);
#endif
    else if (auto in = w.rfind("instrument-editor:", 0) == 0 ? findInstrument(w.substr(18)) : std::nullopt) {
      a.editing = in, a.editingNew = false;  // as Settings' Edit button
      snprintf(a.editName, sizeof a.editName, "%s", in->name.c_str());
    } else fprintf(stderr, "JOHN_SNAPSHOT_OPEN: unknown window or instrument %s\n", w.c_str());
  }
  for (auto& hz : list(env("JOHN_SNAPSHOT_MIC"))) sn.mic.push_back(std::strtof(hz.c_str(), nullptr));
  if (a.showRange)  // one Hz per take: lowest, highest
    for (size_t i = 0; i < 2 && i < sn.mic.size(); i++) a.rangeNote[i] = sungNote({sn.mic[i]});
  else if (!sn.mic.empty()) a.listen = true, a.mode = App::PlayAlong;  // never opens the mic: snapFrame() fills in what it heard
  return sn;
}

// Each frame after practiceUpdate(): the cursor, and play-along with the faked mic. The k-th Hz is what the mic heard
// on the k-th note played (graded like play-along, 0 = silence), the last one is what it hears now.
static void snapFrame(App& a, const Snap& sn) {
  if (sn.cursor >= 0) a.cursor = sn.cursor;
  if (!a.listen || sn.mic.empty() || !a.song) return;
  const Instrument me = myInstrument(a.set);
  auto heard = [&](float hz) { return hz > 0 ? (float)writtenMidiFromFreq(hz, me.octave) : 0.0f; };
  size_t k = 0;
  App::Grade g = App::None;
  for (int ev : a.order) {
    const Event& e = a.song->events[ev];
    if (e.kind != Kind::Note) continue;
    if (!tiedFrom(*a.song, ev)) {  // a tied note keeps the grade of the note it continues
      if (k == sn.mic.size()) break;
      MicNote m = judgeNote(heard(sn.mic[k++]), e.midi, a.set.tolerance, me);
      g = m == MicNote::Right ? App::Good : m == MicNote::Octave ? App::Octave : App::Miss;
    }
    a.grades[ev] = g;
  }
  a.heard = heard(sn.mic.back());
}

int main() {
  try { installSongs(lessonsDir()); } catch (...) {}
#ifdef __EMSCRIPTEN__
  try { installSongs("/demo"); } catch (...) {}  // starter songs, web only: desktop users have the agent
#endif
  if (!glfwInit()) return 1;
#ifndef __EMSCRIPTEN__  // the web gets WebGL and imgui's default "#version 100" shaders
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
  GLFWwindow* win = glfwCreateWindow(1400, 900, "John", nullptr, nullptr);
  if (!win) return 1;
#ifdef __EMSCRIPTEN__
  emscripten_glfw_make_canvas_resizable(win, "window", nullptr);  // the canvas fills the browser window
#endif
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui::GetStyle().FontSizeBase = 16.0f;
  loadFonts(io);
  const char* snapshot = std::getenv("JOHN_SNAPSHOT");  // dev aid: write the window to a .tga after a few frames and exit
  ImGui_ImplGlfw_InitForOpenGL(win, !snapshot);  // a snapshot ignores the real mouse and keyboard: same image every run
#ifdef __EMSCRIPTEN__
  ImGui_ImplOpenGL3_Init(nullptr);
#else
  ImGui_ImplOpenGL3_Init("#version 150");
  glfwSetDropCallback(win, [](GLFWwindow*, int n, const char** paths) { if (n > 0) gDropped = paths[0]; });
#endif

  App a;
  auto applyLanguage = [&a](const std::string& code) {
    a.english = !setLanguage(code.empty() ? systemLanguage() : code);
    for (auto& [id, m] : a.meta) m.mtime = {};  // reload the song list's titles in the new language
    a.lastPoll = -1;
  };
  std::string lang = a.set.language;  // language currently applied
  applyLanguage(lang);
  const Snap sn = snapshot ? snapSetup(a) : Snap{};
  bool light = false;  // theme currently applied
  int frame = 0;
  auto step = [&]() -> bool {  // one frame; false = stop
    if (gMusic && frame == 1) {  // measure a notehead (1.18 staff spaces = 0.295 em) once the font is usable
      float w = gMusic->CalcTextSizeA(100, FLT_MAX, 0, utf8(HeadBlack).c_str()).x;
      if (w > 0) gMusicScale = 29.5f / w;
    }
    bool live = snapshot || a.audio.playing() || a.audio.micOn();  // the mic: play-along or Find my range
#ifdef __EMSCRIPTEN__
    live = true;  // the browser paces the frames; the GLFW port has no glfwWaitEventsTimeout
#endif
    if (live) glfwPollEvents(); else glfwWaitEventsTimeout(0.5);  // idle cheaply, still notice agent edits
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    if (snapshot) io.AddMousePosEvent(a.snapMouse.x, a.snapMouse.y);  // overrides the polled real cursor
    ImGui::NewFrame();
    try { poll(a); } catch (const std::exception&) {}  // e.g. data folder briefly unavailable; retry next poll
    practiceUpdate(a, io.DeltaTime);
    if (snapshot) snapFrame(a, sn);
    if (a.set.light != light) { light = a.set.light; light ? ImGui::StyleColorsLight() : ImGui::StyleColorsDark(); }
    if (!a.busy) {  // the transcription thread reads them (tr(), keyName())
      if (a.set.language != lang) applyLanguage(lang = a.set.language);
      setSolfege(a.set.solfege, a.set.solfegeKeys);
    }
    a.audio.metronome = a.set.metronome;
    a.audio.volume = a.set.volume;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
    const Original* original = songList(a);
    ImGui::SameLine();
    songArea(a, original);
    ImGui::End();
    settingsWindow(a);
    instrumentEditor(a);
    rangeWindow(a);
    abcWindow(a);  // before importWindow: takes dropped ABC and MusicXML files
#ifndef __EMSCRIPTEN__
    importWindow(a);
#endif
    if (a.set != a.saved && !ImGui::IsAnyItemActive()) {  // save on change, once a slider drag ends
      a.saved = a.set;
      try { saveSettings(a.set); } catch (const std::exception&) {}
    }

    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (snapshot && frame == sn.frames) {
      writeSnapshot(snapshot, w, h);
      return false;
    }
    glfwSwapBuffers(win);
    frame++;
    return true;
  };
#ifdef __EMSCRIPTEN__
  // the browser drives the frames; main() never returns, so App and the window stay alive
  emscripten_set_main_loop_arg([](void* f) { (*static_cast<decltype(step)*>(f))(); }, &step, 0, true);
#else
  while (!glfwWindowShouldClose(win) && step()) {}
#endif
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwTerminate();
}
