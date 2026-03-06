#include "game/ui/hud.h"
#include <SDL2/SDL.h>
#include <string>

namespace dom::ui {
namespace {
const char* victory_name(dom::sim::VictoryCondition c) {
  if (c == dom::sim::VictoryCondition::Conquest) return "conquest";
  if (c == dom::sim::VictoryCondition::Score) return "score";
  if (c == dom::sim::VictoryCondition::Wonder) return "wonder";
  return "none";
}
}
void draw_hud(SDL_Window* window, const dom::sim::World& world) {
  const auto& p = world.players[0];
  int capitals = 0;
  for (const auto& city : world.cities) if (city.capital) ++capitals;
  int queueItems = 0;
  for (const auto& b : world.buildings) if (b.team == 0) queueItems += (int)b.queue.size();
  std::string title = "DOMiNATION | Food " + std::to_string((int)p.resources[0]) +
    " Wood " + std::to_string((int)p.resources[1]) +
    " Metal " + std::to_string((int)p.resources[2]) +
    " Wealth " + std::to_string((int)p.resources[3]) +
    " Knowledge " + std::to_string((int)p.resources[4]) +
    " | Age " + std::to_string((int)p.age + 1) +
    " | Pop " + std::to_string(p.popUsed) + "/" + std::to_string(p.popCap) +
    " | Queue " + std::to_string(queueItems) +
    (world.uiBuildMenu ? " | [B] Build" : "") +
    (world.uiTrainMenu ? " | [T] Train" : "") +
    (world.uiResearchMenu ? " | [R] Research" : "") +
    (world.godMode ? " | GOD MODE (All Visible)" : "") +
    " | Minimap [M] | Groups Ctrl+1..9 / 1..9 | Overlays [F1/F2/F3] | Capitals " + std::to_string(capitals);
  if (world.match.phase != dom::sim::MatchPhase::Running) {
    title += " | POSTMATCH winner=" + std::to_string(world.match.winner) + " cond=" + victory_name(world.match.condition);
    for (const auto& p2 : world.players) {
      title += " | P" + std::to_string(p2.id) + " score=" + std::to_string(p2.finalScore) + " lostU=" + std::to_string(p2.unitsLost) + " lostB=" + std::to_string(p2.buildingsLost);
    }
    if (world.wonder.owner != UINT16_MAX) title += " | Wonder P" + std::to_string(world.wonder.owner) + " hold=" + std::to_string(world.wonder.heldTicks);
  }
  SDL_SetWindowTitle(window, title.c_str());
}
}
