// Helpers shared by the core's .cpp files (song, theory, catalog, settings, i18n, import_audio). Not part of the API:
// callers use john.hpp.
#pragma once
#include "john.hpp"
#include <fstream>
#include <random>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace john {

inline int mod12(int x) { return ((x % 12) + 12) % 12; }
extern bool gLocalized;  // a language other than English is loaded (i18n.cpp; theory.cpp's tonicName reads it)

// Web: copy the data folder to IndexedDB after every save, so it survives a reload. Desktop: nothing to do.
inline void persist() {
#ifdef __EMSCRIPTEN__
  EM_ASM(FS.syncfs(false, function(e) { if (e) console.error("John: saving to IndexedDB failed", e); }););
#endif
}

inline json readJson(const fs::path& p) {
  std::ifstream f(p);
  if (!f) return nullptr;
  return json::parse(f, nullptr, false);  // discarded value on error
}

inline void writeJson(const fs::path& p, const json& j, int indent = 1, bool sync = true) {
  // write-then-rename so the GUI never reads a half-written file
  fs::path tmp = p;
  tmp += ".tmp" + std::to_string(std::random_device{}());  // unique per writer (GUI, MCP server)
  { std::ofstream f(tmp); f << j.dump(indent) << "\n"; }
  fs::rename(tmp, p);
  if (sync) persist();
}

}  // namespace john
