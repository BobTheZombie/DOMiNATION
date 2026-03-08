#include "engine/debug/debug_panels.h"
#include "engine/ui/ui_theme.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::debug {
void draw_debug_panels(const dom::sim::World& world, DebugVisualState& state) {
#ifndef DOM_HAS_IMGUI
  (void)world; (void)state;
#else
  if (!ImGui::Begin("Debug Visualization")) { ImGui::End(); return; }
  dom::ui::theme::section_header("Debug Overlays");
  ImGui::Checkbox("pathfinding grid", &state.pathfindingGrid);
  ImGui::Checkbox("chunk boundaries", &state.chunkBoundaries);
  ImGui::Checkbox("biome map", &state.biomeMap);
  ImGui::Checkbox("territory control", &state.territoryControl);
  ImGui::Checkbox("supply routes", &state.supplyRoutes);
  ImGui::Checkbox("rail network", &state.railNetwork);
  ImGui::Checkbox("rail supply", &state.railSupply);
  ImGui::Checkbox("rail freight", &state.railFreight);
  ImGui::Checkbox("operation targets", &state.operationTargets);
  ImGui::Checkbox("AI state", &state.aiState);
  ImGui::Text("Units: %zu | Buildings: %zu | Operations: %zu", world.units.size(), world.buildings.size(), world.operations.size());
  ImGui::Text("Theaters: %zu | Objectives: %zu | ArmyGroups: %zu | NavalTF: %zu | AirWings: %zu",
              world.theaterCommands.size(), world.operationalObjectives.size(), world.armyGroups.size(), world.navalTaskForces.size(), world.airWings.size());
  ImGui::Text("Operational counters: created=%u executed=%u assigned=%u outcomes=%u",
              world.theatersCreatedCount, world.operationsExecutedCount, world.formationsAssignedCount, world.operationalOutcomesRecorded);
  ImGui::End();

  if (!ImGui::Begin("Perf Graphs")) { ImGui::End(); return; }
  dom::ui::theme::section_header("Simulation Performance");
  static float simHistory[120]{};
  static float navHistory[120]{};
  static float combatHistory[120]{};
  static float jobsHistory[120]{};
  static int idx = 0;
  auto profile = dom::sim::last_tick_profile();
  auto stats = dom::sim::last_simulation_stats();
  simHistory[idx] = static_cast<float>(profile.navMs + profile.combatMs);
  navHistory[idx] = static_cast<float>(profile.navMs);
  combatHistory[idx] = static_cast<float>(profile.combatMs);
  jobsHistory[idx] = static_cast<float>(stats.jobCount);
  idx = (idx + 1) % 120;
  ImGui::PlotLines("simulation tick time", simHistory, 120, idx, nullptr, 0.0f, 40.0f, ImVec2(0, 50));
  ImGui::PlotLines("navigation time", navHistory, 120, idx, nullptr, 0.0f, 40.0f, ImVec2(0, 50));
  ImGui::PlotLines("combat time", combatHistory, 120, idx, nullptr, 0.0f, 40.0f, ImVec2(0, 50));
  ImGui::PlotLines("job system stats", jobsHistory, 120, idx, nullptr, 0.0f, 1000.0f, ImVec2(0, 50));
  ImGui::End();


  if (ImGui::Begin("Debug · Rail Logistics")) {
    ImGui::Text("nodes=%zu edges=%zu networks=%zu trains=%zu", world.railNodes.size(), world.railEdges.size(), world.railNetworks.size(), world.trains.size());
    ImGui::Text("active trains supply=%u freight=%u throughput=%.2f disrupted=%u", world.activeSupplyTrains, world.activeFreightTrains, world.railThroughput, world.disruptedRailRoutes);
    if (!world.railNodes.empty()) {
      const auto& n = world.railNodes.front();
      ImGui::SeparatorText("selected rail node/segment/station (sample)");
      ImGui::Text("owner=%u network=%u tile=(%d,%d)", n.owner, n.networkId, n.tile.x, n.tile.y);
    }
    if (!world.trains.empty()) {
      const auto& t = world.trains.front();
      ImGui::SeparatorText("selected train (sample)");
      ImGui::Text("route_steps=%zu cargo=%s destination=%u status=%u", t.route.size(), t.cargoType.c_str(), t.destinationNode, (unsigned)t.state);
    }
  }
  ImGui::End();


  if (ImGui::Begin("Debug · Industrial Economy")) {
    ImGui::Text("factory_count=%u active=%u blocked=%u", world.factoryCount, world.activeFactories, world.blockedFactories);
    ImGui::Text("throughput=%.2f steel=%.2f fuel=%.2f munitions=%.2f machine_parts=%.2f electronics=%.2f",
                world.industrialThroughput,
                world.refinedOutputByTick[0], world.refinedOutputByTick[1], world.refinedOutputByTick[2], world.refinedOutputByTick[3], world.refinedOutputByTick[4]);
  }
  ImGui::End();

  if (ImGui::Begin("Debug · Civilization Identity")) {
    ImGui::Text("UNIQUE_UNITS_PRODUCED=%u UNIQUE_BUILDINGS_CONSTRUCTED=%u", world.uniqueUnitsProduced, world.uniqueBuildingsConstructed);
    ImGui::Text("CIV_CONTENT_RESOLUTION_FALLBACKS=%u", world.civContentResolutionFallbacks);
    ImGui::Text("ROME=%u CHINA=%u EUROPE=%u MIDDLEEAST=%u", world.romeContentUsage, world.chinaContentUsage, world.europeContentUsage, world.middleEastContentUsage);
    ImGui::Text("RUSSIA=%u USA=%u JAPAN=%u EU=%u UK=%u EGYPT=%u TARTARIA=%u", world.russiaContentUsage, world.usaContentUsage, world.japanContentUsage, world.euContentUsage, world.ukContentUsage, world.egyptContentUsage, world.tartariaContentUsage);
    ImGui::Text("CIV_DOCTRINE_SWITCHES=%u CIV_OPERATION_COUNT=%u", world.civDoctrineSwitches, world.civOperationCount);
    ImGui::Text("CIV_INDUSTRY_OUTPUT=%.2f CIV_LOGISTICS_BONUS_USAGE=%.2f", world.civIndustryOutput, world.civLogisticsBonusUsage);
    for (const auto& p : world.players) {
      ImGui::SeparatorText((std::string("P") + std::to_string(p.id) + " " + p.civilization.displayName).c_str());
      ImGui::Text("eco %.2f mil %.2f sci %.2f log %.2f", p.civilization.economyBias, p.civilization.militaryBias, p.civilization.scienceBias, p.civilization.logisticsBias);
      ImGui::Text("ops: secure %.2f encircle %.2f naval %.2f air %.2f",
                  p.civilization.operationPreference[(size_t)dom::sim::OperationType::SecureRoute],
                  p.civilization.operationPreference[(size_t)dom::sim::OperationType::Encirclement],
                  p.civilization.aiNavalPriority,
                  p.civilization.aiAirPriority);
    }
  }
  ImGui::End();


  if (ImGui::Begin("Debug · Strategic Deterrence")) {
    ImGui::Text("STRATEGIC_STOCKPILE_TOTAL=%u STRATEGIC_READY_TOTAL=%u STRATEGIC_PREPARING_TOTAL=%u", world.strategicStockpileTotal, world.strategicReadyTotal, world.strategicPreparingTotal);
    ImGui::Text("STRATEGIC_LAUNCHES=%u STRATEGIC_WARNINGS=%u STRATEGIC_INTERCEPTIONS=%u", world.strategicStrikeEvents, world.strategicWarningEvents, world.interceptionEvents);
    ImGui::Text("STRATEGIC_RETALIATIONS=%u SECOND_STRIKE_READY_COUNT=%u DETERRENCE_POSTURE_CHANGES=%u", world.strategicRetaliationEvents, world.secondStrikeReadyCount, world.deterrencePostureChangeCount);
    ImGui::Text("ARMAGEDDON_ACTIVE=%u NUCLEAR_USE_COUNT_TOTAL=%u ARMAGEDDON_TRIGGER_TICK=%u LAST_MAN_STANDING_MODE_ACTIVE=%u", world.armageddonActive ? 1u : 0u, world.nuclearUseCountTotal, world.armageddonTriggerTick, world.lastManStandingModeActive ? 1u : 0u);
    if (!world.nuclearUseCountByPlayer.empty()) {
      ImGui::TextUnformatted("NUCLEAR_USE_COUNT_BY_CIV/PLAYER");
      for (size_t i = 0; i < world.nuclearUseCountByPlayer.size(); ++i) ImGui::BulletText("P%zu (%s): %u", i, i < world.players.size() ? world.players[i].civilization.displayName.c_str() : "unknown", world.nuclearUseCountByPlayer[i]);
    }
    for (size_t i = 0; i < world.strategicDeterrence.size(); ++i) {
      const auto& d = world.strategicDeterrence[i];
      ImGui::BulletText("P%zu cap=%d stockpile=%u ready=%u prep=%u alert=%u warning=%d retaliation=%d secondStrike=%d", i, d.strategicCapabilityEnabled?1:0, d.strategicStockpile, d.strategicReadyCount, d.strategicPreparingCount, d.strategicAlertLevel, d.launchWarningActive?1:0, d.retaliationCapability?1:0, d.secondStrikeCapability?1:0);
    }
  }
  ImGui::End();

    if (ImGui::Begin("Debug · Mission")) {
    ImGui::Text("status=%u briefingShown=%s result=%s", (unsigned)world.missionRuntime.status, world.missionRuntime.briefingShown?"yes":"no", world.missionRuntime.resultTag.c_str());
    ImGui::Text("triggers fired=%u scripted actions=%u", world.missionRuntime.firedTriggerCount, world.missionRuntime.scriptedActionCount);
    ImGui::Separator();
    ImGui::TextUnformatted("Objectives");
    for (const auto& o : world.objectives) ImGui::BulletText("id=%u %s state=%u visible=%d progress=%.2f %s", o.id, o.title.c_str(), (unsigned)o.state, o.visible?1:0, o.progressValue, o.progressText.c_str());
    ImGui::Separator();
    ImGui::TextUnformatted("Triggers");
    for (const auto& t : world.triggers) ImGui::BulletText("id=%u fired=%d once=%d actions=%zu", t.id, t.fired?1:0, t.once?1:0, t.actions.size());
    if (!world.missionRuntime.luaHookLog.empty()) {
      ImGui::Separator();
      ImGui::TextUnformatted("Lua hooks");
      for (const auto& e : world.missionRuntime.luaHookLog) ImGui::BulletText("%s", e.c_str());
    }
  }
  ImGui::End();

  if (ImGui::Begin("Debug · Mythic Guardians")) {
    ImGui::Text("sites=%zu discovered=%u spawned=%u joined=%u killed=%u", world.guardianSites.size(), world.guardiansDiscovered, world.guardiansSpawned, world.guardiansJoined, world.guardiansKilled);
    for (const auto& s : world.guardianSites) {
      ImGui::BulletText("id=%u guardian=%s discovered=%d spawned=%d owner=%u alive=%d depleted=%d", s.instanceId, s.guardianId.c_str(), s.discovered?1:0, s.spawned?1:0, s.owner, s.alive?1:0, s.siteDepleted?1:0);
    }
  }
  ImGui::End();
#endif
}
} // namespace dom::debug
