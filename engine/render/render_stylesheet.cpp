#include "engine/render/render_stylesheet.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

namespace dom::render {
namespace {
using json = nlohmann::json;

constexpr float kUnsetReadability = -1.0f;

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
  bool hasTint{false};
  bool hasSizeScale{false};
  MaterialReadabilityProfile readability{kUnsetReadability, kUnsetReadability, kUnsetReadability, kUnsetReadability,
                                         kUnsetReadability, kUnsetReadability, kUnsetReadability, kUnsetReadability,
                                         kUnsetReadability, kUnsetReadability, kUnsetReadability, kUnsetReadability};
  std::unordered_map<std::string, std::string> attachments;
  AnimationStyleBinding animation;
  std::unordered_map<std::string, std::shared_ptr<StyleLayer>> stateVariants;
  std::unordered_map<std::string, std::shared_ptr<StyleLayer>> lodVariants;
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

float read_float(const json& j, const char* key, float fallback) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_number()) return fallback;
  return it->get<float>();
}

MaterialReadabilityProfile default_readability(RenderStyleDomain domain) {
  switch (domain) {
    case RenderStyleDomain::Terrain:
      return {0.34f, 0.52f, 0.10f, 0.0f, 0.18f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.18f, 0.08f};
    case RenderStyleDomain::Unit:
      return {0.20f, 0.72f, 0.26f, 0.52f, 0.42f, 0.18f, 0.10f, 0.34f, 0.42f, 0.22f, 0.34f, 0.16f};
    case RenderStyleDomain::Building:
      return {0.24f, 0.68f, 0.22f, 0.38f, 0.40f, 0.16f, 0.48f, 0.42f, 0.28f, 0.26f, 0.28f, 0.22f};
    case RenderStyleDomain::Object:
      return {0.22f, 0.64f, 0.24f, 0.16f, 0.38f, 0.24f, 0.30f, 0.44f, 0.54f, 0.18f, 0.34f, 0.14f};
  }
  return {};
}

void overlay_readability(MaterialReadabilityProfile& out, const MaterialReadabilityProfile& over) {
  auto overlayValue = [](float& dst, float src) {
    if (src >= 0.0f) dst = src;
  };
  overlayValue(out.ambientBoost, over.ambientBoost);
  overlayValue(out.directionalBoost, over.directionalBoost);
  overlayValue(out.rimLight, over.rimLight);
  overlayValue(out.civTintStrength, over.civTintStrength);
  overlayValue(out.stateContrast, over.stateContrast);
  overlayValue(out.emissiveStrength, over.emissiveStrength);
  overlayValue(out.industrialHighlight, over.industrialHighlight);
  overlayValue(out.warningHighlight, over.warningHighlight);
  overlayValue(out.guardianHighlight, over.guardianHighlight);
  overlayValue(out.damageDesaturate, over.damageDesaturate);
  overlayValue(out.farDistanceBoost, over.farDistanceBoost);
  overlayValue(out.terrainBlend, over.terrainBlend);
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
  if (j.contains("tint")) {
    s.tint = read_vec3(j, "tint", s.tint);
    s.hasTint = true;
  }
  if (j.contains("size_scale")) {
    s.sizeScale = read_vec2(j, "size_scale", s.sizeScale);
    s.hasSizeScale = true;
  }
  if (auto it = j.find("readability"); it != j.end() && it->is_object()) {
    s.readability.ambientBoost = read_float(*it, "ambient_boost", s.readability.ambientBoost);
    s.readability.directionalBoost = read_float(*it, "directional_boost", s.readability.directionalBoost);
    s.readability.rimLight = read_float(*it, "rim_light", s.readability.rimLight);
    s.readability.civTintStrength = read_float(*it, "civ_tint_strength", s.readability.civTintStrength);
    s.readability.stateContrast = read_float(*it, "state_contrast", s.readability.stateContrast);
    s.readability.emissiveStrength = read_float(*it, "emissive_strength", s.readability.emissiveStrength);
    s.readability.industrialHighlight = read_float(*it, "industrial_highlight", s.readability.industrialHighlight);
    s.readability.warningHighlight = read_float(*it, "warning_highlight", s.readability.warningHighlight);
    s.readability.guardianHighlight = read_float(*it, "guardian_highlight", s.readability.guardianHighlight);
    s.readability.damageDesaturate = read_float(*it, "damage_desaturate", s.readability.damageDesaturate);
    s.readability.farDistanceBoost = read_float(*it, "far_distance_boost", s.readability.farDistanceBoost);
    s.readability.terrainBlend = read_float(*it, "terrain_blend", s.readability.terrainBlend);
  }
  if (auto it = j.find("attachments"); it != j.end() && it->is_object()) {
    for (auto& [k, v] : it->items()) if (v.is_string()) s.attachments[k] = v.get<std::string>();
  }
  if (auto it = j.find("animation"); it != j.end() && it->is_object()) {
    if (auto st = it->find("default_state"); st != it->end() && st->is_string()) s.animation.defaultState = st->get<std::string>();
    if (auto cp = it->find("default_clip"); cp != it->end() && cp->is_string()) s.animation.defaultClip = cp->get<std::string>();
    if (auto stateClips = it->find("state_clips"); stateClips != it->end() && stateClips->is_object()) {
      for (auto& [stateId, clip] : stateClips->items()) if (clip.is_string()) s.animation.stateClips[stateId] = clip.get<std::string>();
    }
    if (auto hints = it->find("playback_hints"); hints != it->end() && hints->is_object()) {
      for (auto& [k, v] : hints->items()) {
        if (!v.is_string()) continue;
        s.animation.playbackHints[k] = (v.get<std::string>() == "oneshot") ? AnimationPlaybackHint::OneShot : AnimationPlaybackHint::Loop;
      }
    }
  }
  if (auto it = j.find("state_variants"); it != j.end() && it->is_object()) {
    for (auto& [k, v] : it->items()) {
      if (v.is_object()) s.stateVariants[k] = std::make_shared<StyleLayer>(parse_style_layer(v));
    }
  }
  if (auto it = j.find("lods"); it != j.end() && it->is_object()) {
    for (auto& [k, v] : it->items()) {
      if (v.is_object()) s.lodVariants[k] = std::make_shared<StyleLayer>(parse_style_layer(v));
    }
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
  if (over.hasTint) {
    out.tint = over.tint;
    out.hasTint = true;
  }
  if (over.hasSizeScale) {
    out.sizeScale = over.sizeScale;
    out.hasSizeScale = true;
  }
  overlay_readability(out.readability, over.readability);
  for (const auto& [k, v] : over.attachments) out.attachments[k] = v;
  if (!over.animation.defaultState.empty()) out.animation.defaultState = over.animation.defaultState;
  if (!over.animation.defaultClip.empty()) out.animation.defaultClip = over.animation.defaultClip;
  for (const auto& [k, v] : over.animation.stateClips) out.animation.stateClips[k] = v;
  for (const auto& [k, v] : over.animation.playbackHints) out.animation.playbackHints[k] = v;
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

void reload_render_stylesheets() {
  gLoaded = false;
  gStyles = {};
  maybe_load();
}

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
  overlay_readability(selected.readability, default_readability(request.domain));
  bool fallback = false;

  if (auto it = domain.exactMappings.find(request.exactId); it != domain.exactMappings.end()) {
    overlay(selected, it->second);
  } else if (auto ct = domain.renderClasses.find(request.renderClass); ct != domain.renderClasses.end()) {
    overlay(selected, ct->second.defaultStyle);
    if (!request.civId.empty()) {
      if (auto civIt = ct->second.civOverrides.find(request.civId); civIt != ct->second.civOverrides.end()) overlay(selected, civIt->second);
      else if (!request.themeId.empty()) {
        if (auto thIt = ct->second.themeOverrides.find(request.themeId); thIt != ct->second.themeOverrides.end()) overlay(selected, thIt->second);
      }
    } else if (!request.themeId.empty()) {
      if (auto thIt = ct->second.themeOverrides.find(request.themeId); thIt != ct->second.themeOverrides.end()) overlay(selected, thIt->second);
    }
  } else {
    fallback = true;
  }

  const std::string lodId = lod_tier_id(request.lodTier);
  if (auto it = selected.lodVariants.find(lodId); it != selected.lodVariants.end() && it->second) overlay(selected, *it->second);
  if (!request.state.empty()) {
    if (auto st = selected.stateVariants.find(request.state); st != selected.stateVariants.end() && st->second) {
      overlay(selected, *st->second);
    }
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
  out.readability = selected.readability;
  out.attachments = std::move(selected.attachments);
  out.animation = std::move(selected.animation);
  out.fallback = fallback;
  return out;
}

const RenderStylesheetCounters& render_stylesheet_counters() { return gCounters; }
void reset_render_stylesheet_counters() { gCounters = {}; }

} // namespace dom::render
