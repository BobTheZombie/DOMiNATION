#include "engine/render/content_resolution.h"

#include <array>

namespace dom::render {
namespace {
ContentResolutionCounters gCounters{};

bool missing(std::string_view value) {
  if (value.empty()) return true;
  return value.find("fallback") != std::string_view::npos ||
         value.find("missing") != std::string_view::npos;
}

void note_domain(ContentResolutionDomain domain) {
  switch (domain) {
    case ContentResolutionDomain::Material: ++gCounters.materialResolveCount; break;
    case ContentResolutionDomain::Entity: ++gCounters.entityResolveCount; break;
    case ContentResolutionDomain::CityRegion: ++gCounters.cityRegionResolveCount; break;
    case ContentResolutionDomain::Icon: ++gCounters.iconResolveCount; break;
  }
}
} // namespace

void reset_content_resolution_counters() { gCounters = {}; }

const ContentResolutionCounters& content_resolution_counters() { return gCounters; }

void note_content_resolution(ContentResolutionDomain domain, bool fallback) {
  note_domain(domain);
  if (fallback) ++gCounters.fallbackCount;
}

ContentLodTier select_lod_tier(float zoom) {
  ContentLodTier tier = ContentLodTier::Near;
  if (zoom >= 90.0f) tier = ContentLodTier::Far;
  else if (zoom >= 34.0f) tier = ContentLodTier::Mid;

  if (tier == ContentLodTier::Near) ++gCounters.lodNearCount;
  else if (tier == ContentLodTier::Mid) ++gCounters.lodMidCount;
  else ++gCounters.lodFarCount;
  return tier;
}

std::string resolve_content_id(std::string_view exactMapping,
                               std::string_view civSpecificMapping,
                               std::string_view civThemeMapping,
                               std::string_view categoryMapping,
                               std::string_view defaultFallback,
                               ContentResolutionDomain domain) {
  note_domain(domain);
  const std::array<std::string_view, 5> ordered = {
      exactMapping, civSpecificMapping, civThemeMapping, categoryMapping, defaultFallback};
  for (size_t i = 0; i < ordered.size(); ++i) {
    if (missing(ordered[i])) continue;
    if (i == ordered.size() - 1) ++gCounters.fallbackCount;
    return std::string(ordered[i]);
  }
  ++gCounters.fallbackCount;
  return std::string(defaultFallback);
}

} // namespace dom::render
