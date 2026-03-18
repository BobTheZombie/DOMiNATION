#include "engine/debug/debug_panels.h"
#include "engine/ui/ui_theme.h"
#include "engine/render/terrain_materials.h"
#include "engine/render/renderer.h"
#include "engine/render/content_resolution.h"
#include "engine/ui/ui_icons.h"
#include "engine/ui/ui_alerts.h"
#include "engine/audio/audio_system.h"

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
  ImGui::Checkbox("terrain material overlay", &state.terrainMaterialOverlay);
  ImGui::Checkbox("water feature overlay", &state.waterOverlay);
  ImGui::Checkbox("entity presentation debug", &state.entityPresentationDebug);
  ImGui::Checkbox("deterministic visual feedback", &state.visualFeedbackEnabled);
  ImGui::Checkbox("visual feedback source overlay", &state.visualFeedbackOverlayDebug);
  ImGui::Checkbox("strategic visualization layer", &state.strategicVisualization);
  ImGui::Text("Units: %zu | Buildings: %zu | Operations: %zu", world.units.size(), world.buildings.size(), world.operations.size());
  ImGui::Text("Theaters: %zu | Objectives: %zu | ArmyGroups: %zu | NavalTF: %zu | AirWings: %zu",
              world.theaterCommands.size(), world.operationalObjectives.size(), world.armyGroups.size(), world.navalTaskForces.size(), world.airWings.size());
  ImGui::Text("Operational counters: created=%u executed=%u assigned=%u outcomes=%u",
              world.theatersCreatedCount, world.operationsExecutedCount, world.formationsAssignedCount, world.operationalOutcomesRecorded);
  const auto& terrainCounters = dom::render::terrain_presentation_counters();
  ImGui::Text("TERRAIN_MATERIAL_RESOLVES=%llu WATER_FEATURE_RESOLVES=%llu", (unsigned long long)terrainCounters.terrainMaterialResolves, (unsigned long long)terrainCounters.waterFeatureResolves);
  ImGui::Text("FOREST_CLUSTER_COUNT=%llu MOUNTAIN_FEATURE_COUNT=%llu FALLBACKS=%llu", (unsigned long long)terrainCounters.forestClusterCount, (unsigned long long)terrainCounters.mountainFeatureCount, (unsigned long long)terrainCounters.presentationFallbackCount);
  const auto& entityCounters = dom::render::entity_presentation_counters();
  ImGui::Text("UNIT_PRESENTATION_RESOLVES=%llu BUILDING_PRESENTATION_RESOLVES=%llu CITY_PRESENTATION_RESOLVES=%llu CAPITAL_PRESENTATION_RESOLVES=%llu",
              (unsigned long long)entityCounters.unitPresentationResolves,
              (unsigned long long)entityCounters.buildingPresentationResolves,
              (unsigned long long)entityCounters.cityPresentationResolves,
              (unsigned long long)entityCounters.capitalPresentationResolves);
  ImGui::Text("REGION_PRESENTATION_RESOLVES=%llu INDUSTRIAL_REGION_MARKERS=%llu PORT_REGION_MARKERS=%llu",
              (unsigned long long)entityCounters.regionPresentationResolves,
              (unsigned long long)entityCounters.industrialRegionMarkers,
              (unsigned long long)entityCounters.portRegionMarkers);
  ImGui::Text("RAIL_REGION_MARKERS=%llu MINING_REGION_MARKERS=%llu CITY_PRESENTATION_FALLBACKS=%llu",
              (unsigned long long)entityCounters.railRegionMarkers,
              (unsigned long long)entityCounters.miningRegionMarkers,
              (unsigned long long)entityCounters.cityPresentationFallbacks);
  ImGui::Text("GUARDIAN_PRESENTATION_RESOLVES=%llu ENTITY_PRESENTATION_FALLBACKS=%llu FAR_LOD_CLUSTER_COUNT=%llu",
              (unsigned long long)entityCounters.guardianPresentationResolves,
              (unsigned long long)entityCounters.entityPresentationFallbacks,
              (unsigned long long)entityCounters.farLodClusterCount);
  ImGui::Text("MODEL_RESOLVE_COUNT=%llu MODEL_FALLBACK_COUNT=%llu ACTIVE_MODEL_INSTANCES=%llu",
              (unsigned long long)entityCounters.modelResolveCount,
              (unsigned long long)entityCounters.modelFallbackCount,
              (unsigned long long)entityCounters.activeModelInstances);
  ImGui::Text("LOD_MODEL_TIER_COUNTS near=%llu mid=%llu far=%llu",
              (unsigned long long)entityCounters.lodModelNearCount,
              (unsigned long long)entityCounters.lodModelMidCount,
              (unsigned long long)entityCounters.lodModelFarCount);
  ImGui::Text("ANIMATION_RESOLVE_COUNT=%llu ANIMATION_FALLBACK_COUNT=%llu ACTIVE_ANIMATED_INSTANCES=%llu",
              (unsigned long long)entityCounters.animationResolveCount,
              (unsigned long long)entityCounters.animationFallbackCount,
              (unsigned long long)entityCounters.activeAnimatedInstances);
  ImGui::Text("CLIP_PLAY_EVENTS=%llu LOOPING_CLIP_INSTANCES=%llu",
              (unsigned long long)entityCounters.clipPlayEvents,
              (unsigned long long)entityCounters.loopingClipInstances);
  const auto& resolveCounters = dom::render::content_resolution_counters();
  ImGui::Text("MATERIAL_RESOLVE_COUNT=%llu ENTITY_RESOLVE_COUNT=%llu CITY_REGION_RESOLVE_COUNT=%llu ICON_RESOLVE_COUNT=%llu FALLBACK_COUNT=%llu",
              (unsigned long long)resolveCounters.materialResolveCount,
              (unsigned long long)resolveCounters.entityResolveCount,
              (unsigned long long)resolveCounters.cityRegionResolveCount,
              (unsigned long long)resolveCounters.iconResolveCount,
              (unsigned long long)resolveCounters.fallbackCount);
  ImGui::Text("LOD_NEAR_COUNT=%llu LOD_MID_COUNT=%llu LOD_FAR_COUNT=%llu",
              (unsigned long long)resolveCounters.lodNearCount,
              (unsigned long long)resolveCounters.lodMidCount,
              (unsigned long long)resolveCounters.lodFarCount);
  const auto audioCounters = dom::audio::debug_counters();
  ImGui::Text("AUDIO_RESOLVE_COUNT=%llu AUDIO_FALLBACK_COUNT=%llu ACTIVE_AMBIENT_CHANNELS=%llu",
              (unsigned long long)audioCounters.audioResolveCount,
              (unsigned long long)audioCounters.audioFallbackCount,
              (unsigned long long)audioCounters.activeAmbientChannels);
  ImGui::Text("EVENT_SOUND_TRIGGERS=%llu UI_SOUND_TRIGGERS=%llu",
              (unsigned long long)audioCounters.eventSoundTriggers,
              (unsigned long long)audioCounters.uiSoundTriggers);
  ImGui::SeparatorText("Strategic Visualization");
  const auto& strategicCounters = dom::render::strategic_visualization_counters();
  ImGui::Text("MOVEMENT_PATH_RESOLVES=%llu SUPPLY_FLOW_RESOLVES=%llu RAIL_VISUAL_EVENTS=%llu",
              (unsigned long long)strategicCounters.movementPathResolves,
              (unsigned long long)strategicCounters.supplyFlowResolves,
              (unsigned long long)strategicCounters.railVisualEvents);
  ImGui::Text("FRONTLINE_ZONE_UPDATES=%llu THEATER_VISUAL_RESOLVES=%llu VISUAL_FALLBACK_COUNT=%llu",
              (unsigned long long)strategicCounters.frontlineZoneUpdates,
              (unsigned long long)strategicCounters.theaterVisualResolves,
              (unsigned long long)strategicCounters.visualFallbackCount);
  ImGui::Text("RAIL_FLOW_LINES=%llu TRAIN_MARKERS=%llu LOGISTICS_VISUAL_EVENTS=%llu",
              (unsigned long long)strategicCounters.railFlowLines,
              (unsigned long long)strategicCounters.trainMarkers,
              (unsigned long long)strategicCounters.logisticsVisualEvents);
  const auto& feedbackCounters = dom::render::visual_feedback_counters();
  ImGui::Text("COMBAT_EFFECT_SPAWNS=%llu STRATEGIC_EFFECT_SPAWNS=%llu CRISIS_EFFECT_SPAWNS=%llu GUARDIAN_EFFECT_SPAWNS=%llu",
              (unsigned long long)feedbackCounters.combatEffectSpawns,
              (unsigned long long)feedbackCounters.strategicEffectSpawns,
              (unsigned long long)feedbackCounters.crisisEffectSpawns,
              (unsigned long long)feedbackCounters.guardianEffectSpawns);
  ImGui::Text("INDUSTRY_ACTIVITY_EFFECTS=%llu SELECTION_FEEDBACK_EVENTS=%llu FEEDBACK_FALLBACK_COUNT=%llu",
              (unsigned long long)feedbackCounters.industryActivityEffects,
              (unsigned long long)feedbackCounters.selectionFeedbackEvents,
              (unsigned long long)feedbackCounters.feedbackFallbackCount);
  const auto& uiCounters = dom::ui::icons::presentation_counters();
  ImGui::Text("ICON_RESOLVE_COUNT=%llu MARKER_RESOLVE_COUNT=%llu ALERT_RESOLVE_COUNT=%llu PRESENTATION_FALLBACK_COUNT=%llu",
              (unsigned long long)uiCounters.iconResolveCount,
              (unsigned long long)uiCounters.markerResolveCount,
              (unsigned long long)uiCounters.alertResolveCount,
              (unsigned long long)uiCounters.presentationFallbackCount);
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
    auto alertQueue = dom::ui::alerts::build_alert_queue(world, dom::ui::UiState{});
    ImGui::Text("alert queue ordering (deterministic)=%zu", alertQueue.size());
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
