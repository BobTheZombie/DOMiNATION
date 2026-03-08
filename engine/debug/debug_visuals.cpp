#include "engine/debug/debug_visuals.h"
#include "engine/render/renderer.h"

namespace dom::debug {
void sync_debug_visuals(const DebugVisualState& state) {
  static bool lastTerritory = true;
  static bool lastChunk = true;
  static bool lastFog = true;
  static bool lastTerrainMat = false;
  static bool lastWater = false;
  static bool lastEntityPresentation = false;
  if (state.territoryControl != lastTerritory) {
    dom::render::toggle_territory_overlay();
    lastTerritory = state.territoryControl;
  }
  if (state.chunkBoundaries != lastChunk) {
    dom::render::toggle_border_overlay();
    lastChunk = state.chunkBoundaries;
  }
  if (state.biomeMap != lastFog) {
    dom::render::toggle_fog_overlay();
    lastFog = state.biomeMap;
  }
  if (state.terrainMaterialOverlay != lastTerrainMat) {
    dom::render::toggle_terrain_material_overlay();
    lastTerrainMat = state.terrainMaterialOverlay;
  }
  if (state.waterOverlay != lastWater) {
    dom::render::toggle_water_overlay();
    lastWater = state.waterOverlay;
  }
  if (state.entityPresentationDebug != lastEntityPresentation) {
    dom::render::set_entity_presentation_debug(state.entityPresentationDebug);
    lastEntityPresentation = state.entityPresentationDebug;
  }
}
} // namespace dom::debug
