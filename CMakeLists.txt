cmake_minimum_required(VERSION 3.24)
project(john CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(EMSCRIPTEN)  # web build (emcmake): the viewer only, see the john target below
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
  endif()
  add_compile_options(-fexceptions)
  add_link_options(-fexceptions)
endif()

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9 GIT_SHALLOW TRUE)
# miniaudio (public domain / MIT-0): speaker + microphone + mp3/wav/flac decoding, header only
FetchContent_Declare(miniaudio GIT_REPOSITORY https://github.com/mackron/miniaudio.git GIT_TAG 0.11.26 GIT_SHALLOW TRUE)
FetchContent_Populate(miniaudio)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json imgui)
if(NOT EMSCRIPTEN)  # the web uses emscripten's GLFW port
  FetchContent_MakeAvailable(glfw)
endif()

# Bravura (SIL OFL) music font, used to draw clefs, notes and rests.
set(BRAVURA ${CMAKE_BINARY_DIR}/Bravura.otf)
if(NOT EXISTS ${BRAVURA})
  file(DOWNLOAD https://github.com/steinbergmedia/bravura/raw/master/redist/otf/Bravura.otf ${BRAVURA} STATUS st)
endif()
# Noto Sans (SIL OFL 1.1) text font: Latin-1 and more, the same on every platform.
set(NOTO ${CMAKE_BINARY_DIR}/NotoSans-Regular.ttf)
if(NOT EXISTS ${NOTO})
  file(DOWNLOAD https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf ${NOTO} STATUS st)
endif()

add_library(john_core STATIC src/song.cpp src/abc.cpp src/analyze.cpp src/sound.cpp src/transcribe.cpp src/miniaudio.cpp)
target_include_directories(john_core PUBLIC src ${miniaudio_SOURCE_DIR})
target_compile_definitions(john_core PUBLIC MA_NO_ENCODING MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE MA_NO_GENERATION)
# transcription crunches minutes of audio; keep it fast in Debug builds too
set_source_files_properties(src/transcribe.cpp src/miniaudio.cpp PROPERTIES COMPILE_OPTIONS -O2)
target_link_libraries(john_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(john src/gui_main.cpp src/audio.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(john PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(john PRIVATE john_core)

if(EMSCRIPTEN)
  # john.html/.js/.wasm/.data. Fonts and lessons/ are bundled in john.data, the data folder lives in IndexedDB
  # (web/pre.js), audio is miniaudio's Web Audio backend. No MCP server, no tests.
  target_compile_definitions(john_core PRIVATE JOHN_LESSONS="/lessons")
  target_compile_definitions(john PRIVATE JOHN_FONT="/fonts/Bravura.otf" JOHN_TEXT_FONT="/fonts/NotoSans-Regular.ttf")
  target_compile_options(john PRIVATE --use-port=contrib.glfw3)
  target_link_options(john PRIVATE --use-port=contrib.glfw3 -sALLOW_MEMORY_GROWTH -lidbfs.js  # SHELL: stops de-duplication
    "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/pre.js" "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
    "SHELL:--preload-file ${BRAVURA}@/fonts/Bravura.otf" "SHELL:--preload-file ${NOTO}@/fonts/NotoSans-Regular.ttf"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/lessons@/lessons")
  set_target_properties(john PROPERTIES SUFFIX .html LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/pre.js;${CMAKE_SOURCE_DIR}/web/shell.html")
  return()
endif()

target_compile_definitions(john_core PRIVATE JOHN_LESSONS="${CMAKE_SOURCE_DIR}/lessons")
target_compile_definitions(john PRIVATE JOHN_FONT="${BRAVURA}" JOHN_TEXT_FONT="${NOTO}" GL_SILENCE_DEPRECATION)
find_package(OpenGL REQUIRED)
target_link_libraries(john PRIVATE glfw OpenGL::GL)

add_executable(john-mcp src/mcp_main.cpp)
target_link_libraries(john-mcp PRIVATE john_core)

enable_testing()
add_executable(john-tests tests/test_core.cpp)
target_link_libraries(john-tests PRIVATE john_core)
add_test(NAME core COMMAND john-tests)
add_test(NAME mcp COMMAND ${CMAKE_SOURCE_DIR}/tests/mcp_smoke.sh $<TARGET_FILE:john-mcp>)
# knowledge base (kb/): offline checks and the notes' format; no API key needed
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME kb COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/kb/test_kb.py)
  add_test(NAME kb-notes COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/kb/kb.py check)
else()
  message(WARNING "Python 3 not found: the knowledge base tests (kb, kb-notes) are skipped")
endif()
