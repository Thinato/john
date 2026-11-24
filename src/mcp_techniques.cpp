// MCP tools for the technique catalog: list, define.
#include "mcp.hpp"
#include <algorithm>

static std::string list_techniques(const json& a) {
  std::string r;
  for (auto& t : loadTechniques()) r += t.id + " [" + t.symbol + "] " + t.name + ": " + t.description + "\n";
  return r;
}

static std::string define_technique(const json& a) {
  Technique t{a.value("id", ""), a.value("name", ""), a.value("symbol", ""), a.value("description", "")};
  if (t.id.empty() || !std::all_of(t.id.begin(), t.id.end(), [](char c) { return islower(c) || isdigit(c) || c == '_'; }))
    throw std::runtime_error("id must be lowercase letters, digits or _");
  saveCustomTechnique(t);
  return "Technique '" + t.id + "' saved.";
}

std::vector<Tool> techniqueTools() {
  return {
      tool("list_techniques", "List the technique catalog: id, GUI symbol, name, how to play it on the whistle.", json::object(), list_techniques),
      tool("define_technique", "Add or replace a technique in the catalog (for techniques the user wants documented).",
           {{"id", {{"type", "string"}, {"description", "lowercase id like 'bounce'"}}}, {"name", str},
            {"symbol", {{"type", "string"}, {"description", "short text drawn above the note, 1-3 chars"}}},
            {"description", str}},
           {"id", "name", "symbol", "description"}, define_technique),
  };
}
