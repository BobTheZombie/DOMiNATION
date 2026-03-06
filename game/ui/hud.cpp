#include "game/ui/hud.h"
#include <SDL2/SDL.h>
#include <string>

namespace dom::ui {
namespace {
const char* op_name(dom::sim::OperationType t) {
  if (t == dom::sim::OperationType::AssaultCity) return "ASSAULT_CITY";
  if (t == dom::sim::OperationType::DefendBorder) return "DEFEND_BORDER";
  if (t == dom::sim::OperationType::SecureRoute) return "SECURE_ROUTE";
  if (t == dom::sim::OperationType::RaidEconomy) return "RAID_ECONOMY";
  return "RALLY_AND_PUSH";
}

const char* victory_name(dom::sim::VictoryCondition c) {
  if (c == dom::sim::VictoryCondition::Conquest) return "conquest";
  if (c == dom::sim::VictoryCondition::Score) return "score";
  if (c == dom::sim::VictoryCondition::Wonder) return "wonder";
  return "none";
}
}
void draw_hud(SDL_Window* window, const dom::sim::World& world, const std::string& overlay) {
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
    " | Minimap [M] | Groups Ctrl+1..9 / 1..9 | Overlays [F1/F2/F3] | Capitals " + std::to_string(capitals) +
    " | Supply " + std::to_string(world.suppliedUnits) + "/" + std::to_string(world.lowSupplyUnits) + "/" + std::to_string(world.outOfSupplyUnits);
  if (world.match.phase != dom::sim::MatchPhase::Running) {
    title += " | POSTMATCH winner=" + std::to_string(world.match.winner) + " cond=" + victory_name(world.match.condition);
    for (const auto& p2 : world.players) {
      title += " | P" + std::to_string(p2.id) + " score=" + std::to_string(p2.finalScore) + " lostU=" + std::to_string(p2.unitsLost) + " lostB=" + std::to_string(p2.buildingsLost);
    }
    if (world.wonder.owner != UINT16_MAX) title += " | Wonder P" + std::to_string(world.wonder.owner) + " hold=" + std::to_string(world.wonder.heldTicks);
  }
  if (!world.objectives.empty()) {
    int active = 0, done = 0;
    for (const auto& o : world.objectives) { if (o.state == dom::sim::ObjectiveState::Active) ++active; if (o.state == dom::sim::ObjectiveState::Completed) ++done; }
    title += " | Obj A=" + std::to_string(active) + " C=" + std::to_string(done);
  }
  for (const auto& op : world.operations) { if (op.team == 0 && op.active) { title += std::string(" | Op ") + op_name(op.type); break; } }
  if (!overlay.empty()) title += " | " + overlay;
  SDL_SetWindowTitle(window, title.c_str());
}
}
