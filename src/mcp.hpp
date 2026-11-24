// The MCP server's tools. Each area's tools are in mcp_<area>.cpp, one tool(...) entry each (name, description,
// input schema and handler together), in the order tools/list shows them; mcp_main.cpp has the JSON-RPC loop.
// Nothing but JSON-RPC may be written to stdout.
#pragma once
#include "john.hpp"
#include <string>
#include <vector>

using namespace john;

using Handler = std::string (*)(const json& args);  // the reply's text; throw for an error reply
struct Tool {
  json schema;  // name, description, inputSchema: what tools/list shows
  Handler call;
};
Tool tool(const char* name, const char* desc, json props, std::vector<std::string> required, Handler call);
Tool tool(const char* name, const char* desc, json props, Handler call);

// schema pieces more than one area uses
inline const json str = {{"type", "string"}};
inline const json instrumentSchema = {{"type", "string"}, {"description", "an instrument id (list_instruments); instrument the song is for, default whistle"}};

// each area's tools, in tools/list order: songs, audio, techniques, instruments
std::vector<Tool> songTools();        // mcp_songs.cpp
std::vector<Tool> audioTools();       // mcp_audio.cpp
std::vector<Tool> techniqueTools();   // mcp_techniques.cpp
std::vector<Tool> instrumentTools();  // mcp_instruments.cpp
