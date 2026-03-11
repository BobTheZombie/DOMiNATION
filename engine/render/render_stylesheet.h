#pragma once

#include "engine/render/content_resolution.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dom::render {

enum class RenderStyleDomain : uint8_t { Terrain, Unit, Building, Object };

struct RenderStyleRequest {
  RenderStyleDomain domain{RenderStyleDomain::Object};
  std::string exactId;
  std::string civId;
  std::string themeId;
  std::string renderClass;
  std::string state;
  std::string biome;
  ContentLodTier lodTier{ContentLodTier::Near};
};

struct ResolvedRenderStyle {
  std::string styleId;
  std::string renderClass;
  std::string mesh;
  std::string material;
  std::string materialSet;
  std::string lodGroup;
  std::string icon;
  std::string badge;
  std::string decalSet;
  std::array<float, 3> tint{1.0f, 1.0f, 1.0f};
  std::array<float, 2> sizeScale{1.0f, 1.0f};
  std::unordered_map<std::string, std::string> attachments;
  bool fallback{false};
};

struct RenderStylesheetCounters {
  uint64_t styleResolveCount{0};
  uint64_t terrainResolveCount{0};
  uint64_t unitResolveCount{0};
  uint64_t buildingResolveCount{0};
  uint64_t objectResolveCount{0};
  uint64_t fallbackCount{0};
};

void load_render_stylesheets();
ResolvedRenderStyle resolve_render_style(const RenderStyleRequest& request);
const RenderStylesheetCounters& render_stylesheet_counters();
void reset_render_stylesheet_counters();

std::string lod_tier_id(ContentLodTier tier);

} // namespace dom::render
