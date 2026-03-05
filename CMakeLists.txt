cmake_minimum_required(VERSION 3.20)
project(DOMiNATION_RTS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui QUIET)

add_executable(rts
  engine/core/time.cpp
  engine/platform/app.cpp
  engine/render/renderer.cpp
  engine/sim/simulation.cpp
  game/rules/game_rules.cpp
  game/ai/simple_ai.cpp
  game/ui/hud.cpp
  main.cpp
)

target_include_directories(rts PRIVATE .)
target_link_libraries(rts PRIVATE SDL2::SDL2 OpenGL::GL glm::glm nlohmann_json::nlohmann_json)
if(imgui_FOUND)
  target_link_libraries(rts PRIVATE imgui::imgui)
  target_compile_definitions(rts PRIVATE DOM_HAS_IMGUI=1)
else()
  message(WARNING "imgui package not found; building without debug overlay")
endif()
