cmake_minimum_required(VERSION 3.20)
project(DOMiNATION_RTS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RTS_ENABLE_IMGUI "Enable Dear ImGui tooling/debug UI" ON)

find_package(SDL2 CONFIG QUIET)

# Some environments ship CMake's FindSDL2 module but not SDL2Config.cmake.
if(NOT TARGET SDL2::SDL2)
  find_package(SDL2 MODULE QUIET)
endif()

# Ubuntu/Debian often provide sdl2.pc without a CMake package.
if(NOT TARGET SDL2::SDL2)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

if(NOT TARGET SDL2::SDL2)
  message(FATAL_ERROR "Unable to find SDL2 via CMake config package, FindSDL2 module, or pkg-config")
endif()

find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(Lua 5.4 QUIET)
if(NOT Lua_FOUND)
  find_package(Lua QUIET)
endif()

if(RTS_ENABLE_IMGUI)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    message(FATAL_ERROR "RTS_ENABLE_IMGUI is ON but third_party/imgui is missing. Run: git submodule update --init --recursive")
  endif()

  add_library(thirdparty_imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
    third_party/imgui/misc/cpp/imgui_stdlib.cpp
  )

  target_include_directories(thirdparty_imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
  )

  target_link_libraries(thirdparty_imgui PUBLIC SDL2::SDL2 OpenGL::GL)

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_sources(thirdparty_imgui PRIVATE third_party/imgui/imgui_demo.cpp)
  endif()
endif()

add_executable(rts
  engine/core/time.cpp
  engine/platform/app.cpp
  engine/render/renderer.cpp
  engine/render/shader_program.cpp
  engine/render/terrain_shader.cpp
  engine/render/model_shader.cpp
  engine/render/content_resolution.cpp
  engine/render/render_stylesheet.cpp
  engine/render/gltf_runtime_loader.cpp
  engine/render/model_cache.cpp
  engine/render/runtime_animation.cpp
  engine/render/model_render_pass.cpp
  engine/render/terrain_materials.cpp
  engine/render/terrain_chunk_mesh.cpp
  engine/sim/simulation.cpp
  engine/audio/audio_events.cpp
  engine/audio/audio_resolution.cpp
  engine/audio/audio_system.cpp
  engine/assets/asset_manager.cpp
  engine/assets/asset_registry.cpp
  engine/assets/texture_atlas.cpp
  engine/tools/asset_browser.cpp
  engine/ui/hud.cpp
  engine/ui/ui_icons.cpp
  engine/ui/ui_alerts.cpp
  engine/ui/ui_theme.cpp
  engine/ui/production_menu.cpp
  engine/ui/research_panel.cpp
  engine/ui/diplomacy_panel.cpp
  engine/editor/scenario_editor.cpp
  engine/editor/terrain_editor.cpp
  engine/editor/object_placement.cpp
  engine/debug/debug_panels.cpp
  engine/debug/debug_visuals.cpp
  game/rules/game_rules.cpp
  game/ai/simple_ai.cpp
  main.cpp
)


add_executable(dom_asset_studio
  engine/render/content_resolution.cpp
  engine/render/render_stylesheet.cpp
  engine/render/shader_program.cpp
  engine/render/model_shader.cpp
  engine/render/gltf_runtime_loader.cpp
  engine/render/model_cache.cpp
  engine/render/runtime_animation.cpp
  engine/render/model_render_pass.cpp
  engine/assets/asset_manager.cpp
  engine/assets/asset_registry.cpp
  engine/assets/texture_atlas.cpp
  tools/dom_asset_studio/file_watch.cpp
  tools/dom_asset_studio/studio_app.cpp
  tools/dom_asset_studio_main.cpp
)

target_include_directories(dom_asset_studio PRIVATE .)
target_link_libraries(dom_asset_studio PRIVATE SDL2::SDL2 OpenGL::GL glm::glm nlohmann_json::nlohmann_json)
if(RTS_ENABLE_IMGUI)
  target_link_libraries(dom_asset_studio PRIVATE thirdparty_imgui)
  target_compile_definitions(dom_asset_studio PRIVATE DOM_HAS_IMGUI=1)
endif()

target_include_directories(rts PRIVATE .)
target_link_libraries(rts PRIVATE SDL2::SDL2 OpenGL::GL glm::glm nlohmann_json::nlohmann_json)
if(Lua_FOUND)
  if(TARGET Lua::Lua)
    target_link_libraries(rts PRIVATE Lua::Lua)
  else()
    target_include_directories(rts PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(rts PRIVATE ${LUA_LIBRARIES})
  endif()
  target_compile_definitions(rts PRIVATE DOM_HAS_LUA=1)
endif()

if(RTS_ENABLE_IMGUI)
  target_link_libraries(rts PRIVATE thirdparty_imgui)
  target_compile_definitions(rts PRIVATE DOM_HAS_IMGUI=1)
endif()
