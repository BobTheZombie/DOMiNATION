#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace dom::render {

enum class ContentResolutionDomain : uint8_t {
  Material,
  Entity,
  CityRegion,
  Icon,
};

enum class ContentLodTier : uint8_t {
  Near,
  Mid,
  Far,
};

struct ContentResolutionCounters {
  uint64_t materialResolveCount{0};
  uint64_t entityResolveCount{0};
  uint64_t cityRegionResolveCount{0};
  uint64_t iconResolveCount{0};
  uint64_t fallbackCount{0};
  uint64_t lodNearCount{0};
  uint64_t lodMidCount{0};
  uint64_t lodFarCount{0};
};

void reset_content_resolution_counters();
const ContentResolutionCounters& content_resolution_counters();

void note_content_resolution(ContentResolutionDomain domain, bool fallback);
ContentLodTier select_lod_tier(float zoom);

std::string resolve_content_id(std::string_view exactMapping,
                               std::string_view civSpecificMapping,
                               std::string_view civThemeMapping,
                               std::string_view categoryMapping,
                               std::string_view defaultFallback,
                               ContentResolutionDomain domain);

} // namespace dom::render
