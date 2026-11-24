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
if(MSVC)  # Windows: UTF-8 source files (symbols in strings), M_PI, no "use fopen_s" warnings
  add_compile_options(/utf-8)
  add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()

# Worktrees share downloads and dependency sources in JOHN_DEPS_CACHE (default ~/.cache/john; off in CI unless set):
# src/<name>-<hash of its options>/ and downloads/<sha256>, written under a file lock and renamed into place when complete.
if(NOT DEFINED JOHN_DEPS_CACHE)
  if(DEFINED ENV{JOHN_DEPS_CACHE})
    set(JOHN_DEPS_CACHE $ENV{JOHN_DEPS_CACHE})
  elseif(NOT DEFINED ENV{CI} AND (DEFINED ENV{HOME} OR DEFINED ENV{USERPROFILE}))
    set(home $ENV{HOME})
    if(NOT home)
      set(home $ENV{USERPROFILE})
    endif()
    set(JOHN_DEPS_CACHE ${home}/.cache/john)
  endif()
  set(JOHN_DEPS_CACHE "${JOHN_DEPS_CACHE}" CACHE PATH "Shared download and dependency cache (empty: none)")
endif()
if(JOHN_DEPS_CACHE)
  file(TO_CMAKE_PATH "${JOHN_DEPS_CACHE}" JOHN_DEPS_CACHE)
  set(JOHN_DOWNLOADS ${JOHN_DEPS_CACHE}/downloads)
else()
  set(JOHN_DOWNLOADS ${CMAKE_BINARY_DIR}/downloads)
endif()
function(john_download url)  # [sha256] checked if given, else the file is named by its URL; sets DOWNLOADED (empty on failure)
  if(ARGC GREATER 1)
    set(f ${JOHN_DOWNLOADS}/${ARGV1})
    set(check EXPECTED_HASH SHA256=${ARGV1})
  else()
    string(SHA256 key "${url}")
    set(f ${JOHN_DOWNLOADS}/url-${key})
  endif()
  if(NOT EXISTS ${f})
    file(MAKE_DIRECTORY ${JOHN_DOWNLOADS})
    file(LOCK ${f}.lock GUARD FUNCTION TIMEOUT 600)
    if(NOT EXISTS ${f})
      message(STATUS "Downloading ${url}")
      file(DOWNLOAD ${url} ${f}.part ${check} TLS_VERIFY ON STATUS st)
      list(GET st 0 code)
      if(NOT code EQUAL 0)
        file(REMOVE ${f}.part)
        message(WARNING "Download failed: ${url}: ${st}")
        set(DOWNLOADED "" PARENT_SCOPE)
        return()
      endif()
      file(RENAME ${f}.part ${f})
    endif()
  endif()
  set(DOWNLOADED ${f} PARENT_SCOPE)
endfunction()

include(FetchContent)
# FetchContent_Declare, with the sources taken from (and first fetched into) JOHN_DEPS_CACHE/src when it is set
macro(john_declare name)
  if(JOHN_DEPS_CACHE)
    string(SHA256 key "${ARGN}")
    string(SUBSTRING ${key} 0 12 key)
    string(TOUPPER ${name} upper)
    set(FETCHCONTENT_SOURCE_DIR_${upper} ${JOHN_DEPS_CACHE}/src/${name}-${key})
    if(NOT EXISTS ${FETCHCONTENT_SOURCE_DIR_${upper}})
      file(MAKE_DIRECTORY ${JOHN_DEPS_CACHE}/src)
      file(LOCK ${FETCHCONTENT_SOURCE_DIR_${upper}}.lock TIMEOUT 600)
      if(NOT EXISTS ${FETCHCONTENT_SOURCE_DIR_${upper}})
        set(tmp ${FETCHCONTENT_SOURCE_DIR_${upper}}.part)
        file(REMOVE_RECURSE ${tmp} ${CMAKE_BINARY_DIR}/_deps/${name}-cache)
        FetchContent_Populate(${name}-cache QUIET ${ARGN} SOURCE_DIR ${tmp}
          SUBBUILD_DIR ${CMAKE_BINARY_DIR}/_deps/${name}-cache/sub BINARY_DIR ${CMAKE_BINARY_DIR}/_deps/${name}-cache/bin)
        file(RENAME ${tmp} ${FETCHCONTENT_SOURCE_DIR_${upper}})
        file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/_deps/${name}-cache)
      endif()
      file(LOCK ${FETCHCONTENT_SOURCE_DIR_${upper}}.lock RELEASE)
    endif()
  endif()
  FetchContent_Declare(${name} ${ARGN})
endmacro()

# ccache, when installed, for every compile (not Visual Studio, which ignores launchers). base_dir makes paths inside the
# worktree relative, so worktrees hit each other's cache entries.
find_program(CCACHE ccache)
if(CCACHE AND NOT CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
  set(launcher ${CMAKE_COMMAND} -E env CCACHE_BASEDIR=${CMAKE_SOURCE_DIR} CCACHE_NOHASHDIR=1 ${CCACHE})
  set(CMAKE_C_COMPILER_LAUNCHER ${launcher})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${launcher})
endif()

john_declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
john_declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)
john_declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9 GIT_SHALLOW TRUE)
# miniaudio (public domain / MIT-0): speaker + microphone + mp3/wav/flac decoding, header only
john_declare(miniaudio GIT_REPOSITORY https://github.com/mackron/miniaudio.git GIT_TAG 0.11.26 GIT_SHALLOW TRUE)
FetchContent_Populate(miniaudio)
# MusicXML import: pugixml (MIT) reads the XML, miniz (MIT) unzips .mxl; both compile into john_core
john_declare(pugixml URL https://github.com/zeux/pugixml/releases/download/v1.15/pugixml-1.15.tar.gz)
john_declare(miniz URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json imgui pugixml miniz)
if(NOT EMSCRIPTEN)  # the web uses emscripten's GLFW port
  FetchContent_MakeAvailable(glfw)
endif()

# Bravura (SIL OFL) music font, used to draw clefs, notes and rests.
set(BRAVURA ${CMAKE_BINARY_DIR}/Bravura.otf)
if(NOT EXISTS ${BRAVURA})
  john_download(https://github.com/steinbergmedia/bravura/raw/master/redist/otf/Bravura.otf)
  if(DOWNLOADED)
    file(COPY_FILE ${DOWNLOADED} ${BRAVURA})
  endif()
endif()
# Noto Sans (SIL OFL 1.1) text font: Latin-1 and more, the same on every platform.
set(NOTO ${CMAKE_BINARY_DIR}/NotoSans-Regular.ttf)
if(NOT EXISTS ${NOTO})
  john_download(https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf)
  if(DOWNLOADED)
    file(COPY_FILE ${DOWNLOADED} ${NOTO})
  endif()
endif()

add_library(john_core STATIC src/song.cpp src/theory.cpp src/catalog.cpp src/settings.cpp src/i18n.cpp src/import_audio.cpp
  src/abc.cpp src/analyze.cpp src/sound.cpp src/transcribe.cpp src/miniaudio.cpp src/musicxml.cpp ${miniz_SOURCE_DIR}/miniz.c)
target_include_directories(john_core PUBLIC src ${miniaudio_SOURCE_DIR} ${miniz_SOURCE_DIR})
target_compile_definitions(john_core PUBLIC MA_NO_ENCODING MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE MA_NO_GENERATION)
# transcription crunches minutes of audio; keep it fast in Debug builds too
set_source_files_properties(src/transcribe.cpp src/miniaudio.cpp PROPERTIES COMPILE_OPTIONS $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2>)  # MSVC: /O2 clashes with Debug /RTC1
target_link_libraries(john_core PUBLIC nlohmann_json::nlohmann_json PRIVATE pugixml::pugixml)
if(APPLE)
  target_link_libraries(john_core PUBLIC "-framework CoreFoundation")  # systemLanguage(): CFLocaleCopyPreferredLanguages
endif()

add_executable(john src/gui_main.cpp src/gui_panels.cpp src/gui_score.cpp src/gui_practice.cpp src/gui_settings.cpp
  src/gui_import.cpp src/audio.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(john PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(john PRIVATE john_core)

if(EMSCRIPTEN)
  # john.html/.js/.wasm/.data. Fonts, lessons/, demo/, i18n/ and instruments/ are bundled in john.data, the data folder lives in IndexedDB
  # (web/pre.js), audio is miniaudio's Web Audio backend. No MCP server, no tests.
  target_compile_definitions(john_core PRIVATE JOHN_LESSONS="/lessons" JOHN_I18N="/i18n" JOHN_INSTRUMENTS="/instruments")
  target_compile_definitions(john PRIVATE JOHN_FONT="/fonts/Bravura.otf" JOHN_TEXT_FONT="/fonts/NotoSans-Regular.ttf")
  target_compile_options(john PRIVATE --use-port=contrib.glfw3)
  target_link_options(john PRIVATE --use-port=contrib.glfw3 -sALLOW_MEMORY_GROWTH -lidbfs.js  # SHELL: stops de-duplication
    "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/pre.js" "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
    "SHELL:--preload-file ${BRAVURA}@/fonts/Bravura.otf" "SHELL:--preload-file ${NOTO}@/fonts/NotoSans-Regular.ttf"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/lessons@/lessons" "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/demo@/demo"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/i18n@/i18n" "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/instruments@/instruments")
  set_target_properties(john PROPERTIES SUFFIX .html LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/pre.js;${CMAKE_SOURCE_DIR}/web/shell.html")

  # Image import (#75): the sheet-music reader is served next to john.html, in omr/. web/omr/ (John's scripts and the
  # vendored copisteria port) is copied on every build. The copista-2m model (its ONNX export, mirrored on this
  # repo's models-v1 release, #80), ONNX Runtime Web and pdf.js are downloaded at pinned versions, SHA-256 checked, into
  # downloads/<sha256> (a new pin downloads again). omr/manifest.json lists what the reader caches in the browser.
  set(OMR ${CMAKE_BINARY_DIR}/omr)
  # The upstream commit of huggingface.co/pauldubois98/musicocr-models the mirror was taken from. It names the
  # manifest version, so the same bytes keep the browser's cached copy. A new model means a new release (models-v2).
  set(OMR_MODELS_COMMIT 049d45cf85545b169392bc4376b7cf0570408f44)
  set(OMR_MODELS https://github.com/Thinato/john/releases/download/models-v1)
  set(ORT_VERSION 1.30.0)
  set(PDFJS_VERSION 6.5.438)
  function(omr_download url sha256)  # sets DOWNLOADED to the checked file
    john_download(${url} ${sha256})
    if(NOT DOWNLOADED)
      message(FATAL_ERROR "Could not download ${url}")
    endif()
    set(DOWNLOADED ${DOWNLOADED} PARENT_SCOPE)
  endfunction()
  function(omr_file url sha256 dest)
    omr_download(${url} ${sha256})
    configure_file(${DOWNLOADED} ${OMR}/${dest} COPYONLY)
  endfunction()
  function(omr_npm url sha256 dest)  # then the package's files to copy into omr/<dest>/, dist/ and build/ dropped
    omr_download(${url} ${sha256})
    set(dir ${CMAKE_BINARY_DIR}/downloads/${sha256}-x)
    list(TRANSFORM ARGN PREPEND package/ OUTPUT_VARIABLE members)
    file(ARCHIVE_EXTRACT INPUT ${DOWNLOADED} DESTINATION ${dir} PATTERNS ${members})
    foreach(f ${ARGN})
      string(REGEX REPLACE "^(dist|build)/" "" to ${f})
      configure_file(${dir}/package/${f} ${OMR}/${dest}/${to} COPYONLY)
    endforeach()
  endfunction()
  omr_file(${OMR_MODELS}/copista-2m-trunk.onnx 48963688a5bc542ae2f45e5a3f92a67395980dbf4fe10523aefb2e8ea734489d models/copista-2m-trunk.onnx)
  omr_file(${OMR_MODELS}/copista-2m-verifier.onnx 2a3ace8af11b134d270fef6f2af902d692ff656db22d6f0c19e40e6fd9b48d2a models/copista-2m-verifier.onnx)
  omr_file(${OMR_MODELS}/copista-evidence.onnx 37257aacbfab45243fe47fd24d22cee4d0e24e69c78e06e2736fb9e55fcac847 models/copista-evidence.onnx)
  omr_file(${OMR_MODELS}/copista-meta.json ae34e810a43c7d0833fcb94e8a8cfc2be7d1e82ec4e0bca68a810a659134876b models/copista-meta.json)
  omr_file(${OMR_MODELS}/LICENSE cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30 licenses/copista-models-LICENSE.txt)
  omr_file(${OMR_MODELS}/NOTICE 02beb1af1c2a10ded1f008db587f28c6a36180607cb305ef4f597d0388bc29fe licenses/copista-models-NOTICE.txt)
  omr_npm(https://registry.npmjs.org/onnxruntime-web/-/onnxruntime-web-${ORT_VERSION}.tgz
    d2228df7e4616bc3348bf504ee888f3bec43789a273f0a63f3e68d203ce3bf71 ort
    dist/ort.min.mjs dist/ort-wasm-simd-threaded.jsep.mjs dist/ort-wasm-simd-threaded.jsep.wasm)
  omr_npm(https://registry.npmjs.org/pdfjs-dist/-/pdfjs-dist-${PDFJS_VERSION}.tgz
    7461aefcd27c4ef0dd76f9ca5760f09466eb6b3f934432670c12132e7b3d6faa pdfjs
    build/pdf.min.mjs build/pdf.worker.min.mjs LICENSE wasm/jbig2.wasm wasm/openjpeg.wasm wasm/qcms_bg.wasm
    wasm/LICENSE_JBIG2 wasm/LICENSE_OPENJPEG wasm/LICENSE_QCMS wasm/LICENSE_PDFJS_JBIG2 wasm/LICENSE_PDFJS_OPENJPEG wasm/LICENSE_PDFJS_QCMS)
  set(cached "")
  foreach(f models/copista-2m-trunk.onnx models/copista-2m-verifier.onnx models/copista-evidence.onnx models/copista-meta.json
            ort/ort-wasm-simd-threaded.jsep.wasm)
    file(SIZE ${OMR}/${f} size)
    list(APPEND cached "\"${f}\": ${size}")
  endforeach()
  list(JOIN cached ", " cached)
  string(SUBSTRING ${OMR_MODELS_COMMIT} 0 7 short)
  file(WRITE ${OMR}/manifest.json "{\"version\": \"${short}-ort${ORT_VERSION}\", \"files\": {${cached}}}\n")
  add_custom_target(omr ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/web/omr ${OMR}
    COMMENT "Copying the image reader (web/omr) next to john.html")
  return()
endif()

# the checkout's paths go only to the files that use them, so ccache can share the other objects between worktrees
set_property(SOURCE src/settings.cpp APPEND PROPERTY COMPILE_DEFINITIONS JOHN_LESSONS="${CMAKE_SOURCE_DIR}/lessons")
set_property(SOURCE src/i18n.cpp APPEND PROPERTY COMPILE_DEFINITIONS JOHN_I18N="${CMAKE_SOURCE_DIR}/i18n")
set_property(SOURCE src/catalog.cpp APPEND PROPERTY COMPILE_DEFINITIONS JOHN_INSTRUMENTS="${CMAKE_SOURCE_DIR}/instruments")
set_property(SOURCE src/gui_main.cpp APPEND PROPERTY COMPILE_DEFINITIONS JOHN_FONT="${BRAVURA}" JOHN_TEXT_FONT="${NOTO}")
target_compile_definitions(john PRIVATE GL_SILENCE_DEPRECATION)
find_package(OpenGL REQUIRED)
target_link_libraries(john PRIVATE glfw OpenGL::GL)

add_executable(john-mcp src/mcp_main.cpp src/mcp_songs.cpp src/mcp_audio.cpp src/mcp_techniques.cpp src/mcp_instruments.cpp)
target_link_libraries(john-mcp PRIVATE john_core)
if(WIN32)  # char strings and paths are UTF-8, as on macOS/Linux; john.exe opens no console window
  target_sources(john PRIVATE src/utf8.manifest)
  target_sources(john-mcp PRIVATE src/utf8.manifest)
  set_target_properties(john PROPERTIES WIN32_EXECUTABLE ON)
  target_link_options(john PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/ENTRY:mainCRTStartup>)
endif()

enable_testing()
file(GLOB core_tests CONFIGURE_DEPENDS tests/core/*.cpp)  # a new area file needs no line here
add_executable(john-tests ${core_tests})
target_link_libraries(john-tests PRIVATE john_core)
add_test(NAME core COMMAND john-tests)
if(WIN32)  # Git for Windows' sh runs the script
  target_sources(john-tests PRIVATE src/utf8.manifest)
  find_program(SH sh REQUIRED)
endif()
add_test(NAME mcp COMMAND ${SH} ${CMAKE_SOURCE_DIR}/tests/mcp_smoke.sh $<TARGET_FILE:john-mcp>)
# knowledge base (kb/): offline checks and the notes' format; no API key needed
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME kb COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/kb/test_kb.py)
  add_test(NAME kb-notes COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/kb/kb.py check)
else()
  message(WARNING "Python 3 not found: the knowledge base tests (kb, kb-notes) are skipped")
endif()
