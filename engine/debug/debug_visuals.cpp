#include "engine/debug/debug_visuals.h"
#include "engine/render/renderer.h"

namespace dom::debug {
void sync_debug_visuals(const DebugVisualState& state) {
  static bool lastTerritory = true;
  static bool lastChunk = true;
  static bool lastFog = true;
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
}
} // namespace dom::debug
