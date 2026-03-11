#include "engine/render/render_stylesheet.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace dom::render {
namespace {
using json = nlohmann::json;

struct StyleLayer {
  std::string id;
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
  std::unordered_map<std::string, StyleLayer> stateVariants;
  std::unordered_map<std::string, StyleLayer> lodVariants;
};

struct ClassStyle {
  StyleLayer defaultStyle;
  std::unordered_map<std::string, StyleLayer> civOverrides;
  std::unordered_map<std::string, StyleLayer> themeOverrides;
};

struct DomainStyles {
  StyleLayer defaultStyle;
  std::unordered_map<std::string, StyleLayer> exactMappings;
  std::unordered_map<std::string, ClassStyle> renderClasses;
};

std::array<DomainStyles, 4> gStyles;
RenderStylesheetCounters gCounters{};
bool gLoaded = false;

std::string read_string(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_string()) return {};
  return it->get<std::string>();
}

std::array<float, 3> read_vec3(const json& j, const char* key, const std::array<float, 3>& fallback) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_array() || it->size() != 3) return fallback;
  return {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()};
}

std::array<float, 2> read_vec2(const json& j, const char* key, const std::array<float, 2>& fallback) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_array() || it->size() != 2) return fallback;
  return {(*it)[0].get<float>(), (*it)[1].get<float>()};
}

StyleLayer parse_style_layer(const json& j) {
  StyleLayer s{};
  s.id = read_string(j, "style_id");
  s.mesh = read_string(j, "mesh");
  s.material = read_string(j, "material");
  s.materialSet = read_string(j, "material_set");
  s.lodGroup = read_string(j, "lod_group");
  s.icon = read_string(j, "icon");
  s.badge = read_string(j, "badge");
  s.decalSet = read_string(j, "decal_set");
  s.tint = read_vec3(j, "tint", s.tint);
  s.sizeScale = read_vec2(j, "size_scale", s.sizeScale);
  if (auto it = j.find("attachments"); it != j.end() && it->is_object()) {
    for (auto& [k, v] : it->items()) if (v.is_string()) s.attachments[k] = v.get<std::string>();
  }
  if (auto it = j.find("state_variants"); it != j.end() && it->is_object()) {
    for (auto& [k, v] : it->items()) if (v.is_object()) s.stateVariants[k] = parse_style_layer(v);
  }
  if (auto it = j.find("lods"); it != j.end() && it->is_object()) {
    for (auto& [k, v] : it->items()) if (v.is_object()) s.lodVariants[k] = parse_style_layer(v);
  }
  return s;
}

void overlay(StyleLayer& out, const StyleLayer& over) {
  if (!over.id.empty()) out.id = over.id;
  if (!over.mesh.empty()) out.mesh = over.mesh;
  if (!over.material.empty()) out.material = over.material;
  if (!over.materialSet.empty()) out.materialSet = over.materialSet;
  if (!over.lodGroup.empty()) out.lodGroup = over.lodGroup;
  if (!over.icon.empty()) out.icon = over.icon;
  if (!over.badge.empty()) out.badge = over.badge;
  if (!over.decalSet.empty()) out.decalSet = over.decalSet;
  out.tint = over.tint;
  out.sizeScale = over.sizeScale;
  for (const auto& [k, v] : over.attachments) out.attachments[k] = v;
}

size_t domain_index(RenderStyleDomain d) {
  return static_cast<size_t>(d);
}

void parse_domain_file(const std::filesystem::path& path, DomainStyles& domain) {
  std::ifstream in(path);
  if (!in) return;
  json j; in >> j;
  if (auto it = j.find("default"); it != j.end() && it->is_object()) domain.defaultStyle = parse_style_layer(*it);
  if (auto it = j.find("exact_mappings"); it != j.end() && it->is_object()) {
    for (auto& [id, entry] : it->items()) if (entry.is_object()) domain.exactMappings[id] = parse_style_layer(entry);
  }
  if (auto it = j.find("render_classes"); it != j.end() && it->is_object()) {
    for (auto& [id, entry] : it->items()) {
      if (!entry.is_object()) continue;
      ClassStyle cs{};
      if (auto dit = entry.find("default"); dit != entry.end() && dit->is_object()) cs.defaultStyle = parse_style_layer(*dit);
      if (auto cit = entry.find("civ_overrides"); cit != entry.end() && cit->is_object()) {
        for (auto& [cid, centry] : cit->items()) if (centry.is_object()) cs.civOverrides[cid] = parse_style_layer(centry);
      }
      if (auto tit = entry.find("theme_overrides"); tit != entry.end() && tit->is_object()) {
        for (auto& [tid, tentry] : tit->items()) if (tentry.is_object()) cs.themeOverrides[tid] = parse_style_layer(tentry);
      }
      domain.renderClasses[id] = std::move(cs);
    }
  }
}

void maybe_load() {
  if (gLoaded) return;
  gLoaded = true;
  parse_domain_file("content/terrain_styles.json", gStyles[domain_index(RenderStyleDomain::Terrain)]);
  parse_domain_file("content/unit_styles.json", gStyles[domain_index(RenderStyleDomain::Unit)]);
  parse_domain_file("content/building_styles.json", gStyles[domain_index(RenderStyleDomain::Building)]);
  parse_domain_file("content/object_styles.json", gStyles[domain_index(RenderStyleDomain::Object)]);
}

void note_domain(RenderStyleDomain domain) {
  ++gCounters.styleResolveCount;
  switch (domain) {
    case RenderStyleDomain::Terrain: ++gCounters.terrainResolveCount; break;
    case RenderStyleDomain::Unit: ++gCounters.unitResolveCount; break;
    case RenderStyleDomain::Building: ++gCounters.buildingResolveCount; break;
    case RenderStyleDomain::Object: ++gCounters.objectResolveCount; break;
  }
}

ContentResolutionDomain to_content_domain(RenderStyleDomain domain) {
  return domain == RenderStyleDomain::Terrain ? ContentResolutionDomain::Material : ContentResolutionDomain::Entity;
}

} // namespace

void load_render_stylesheets() { maybe_load(); }

std::string lod_tier_id(ContentLodTier tier) {
  if (tier == ContentLodTier::Near) return "near";
  if (tier == ContentLodTier::Mid) return "mid";
  return "far";
}

ResolvedRenderStyle resolve_render_style(const RenderStyleRequest& request) {
  maybe_load();
  note_domain(request.domain);

  const auto& domain = gStyles[domain_index(request.domain)];
  StyleLayer selected = domain.defaultStyle;
  bool fallback = false;

  if (auto it = domain.exactMappings.find(request.exactId); it != domain.exactMappings.end()) {
    selected = it->second;
  } else if (auto ct = domain.renderClasses.find(request.renderClass); ct != domain.renderClasses.end()) {
    selected = ct->second.defaultStyle;
    if (!request.civId.empty()) {
      if (auto civIt = ct->second.civOverrides.find(request.civId); civIt != ct->second.civOverrides.end()) selected = civIt->second;
      else if (!request.themeId.empty()) {
        if (auto thIt = ct->second.themeOverrides.find(request.themeId); thIt != ct->second.themeOverrides.end()) selected = thIt->second;
      }
    } else if (!request.themeId.empty()) {
      if (auto thIt = ct->second.themeOverrides.find(request.themeId); thIt != ct->second.themeOverrides.end()) selected = thIt->second;
    }
  } else {
    fallback = true;
  }

  const std::string lodId = lod_tier_id(request.lodTier);
  if (auto it = selected.lodVariants.find(lodId); it != selected.lodVariants.end()) overlay(selected, it->second);
  if (!request.state.empty()) {
    if (auto st = selected.stateVariants.find(request.state); st != selected.stateVariants.end()) overlay(selected, st->second);
  }

  note_content_resolution(to_content_domain(request.domain), fallback);
  if (fallback) ++gCounters.fallbackCount;

  ResolvedRenderStyle out{};
  out.styleId = selected.id;
  out.renderClass = request.renderClass;
  out.mesh = selected.mesh;
  out.material = selected.material;
  out.materialSet = selected.materialSet;
  out.lodGroup = selected.lodGroup;
  out.icon = selected.icon;
  out.badge = selected.badge;
  out.decalSet = selected.decalSet;
  out.tint = selected.tint;
  out.sizeScale = selected.sizeScale;
  out.attachments = std::move(selected.attachments);
  out.fallback = fallback;
  return out;
}

const RenderStylesheetCounters& render_stylesheet_counters() { return gCounters; }
void reset_render_stylesheet_counters() { gCounters = {}; }

} // namespace dom::render
