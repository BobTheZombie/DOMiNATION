#include "engine/render/model_cache.h"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace dom::render {

namespace {
constexpr const char* kFallbackModel = "assets_final/fallback/missing_mesh.glb";
}

void ModelCache::lazy_load_manifests() {
  if (manifests_.loaded) return;
  manifests_.loaded = true;

  auto load_json = [](const char* path) {
    std::ifstream file(path);
    nlohmann::json doc;
    if (file.good()) file >> doc;
    return doc;
  };

  const auto lodDoc = load_json("content/lod_manifest.json");
  if (lodDoc.is_object() && lodDoc.contains("lod_entries") && lodDoc["lod_entries"].is_array()) {
    for (const auto& entry : lodDoc["lod_entries"]) {
      if (!entry.is_object()) continue;
      if (!entry.contains("lod_id") || !entry.contains("asset_id")) continue;
      manifests_.lodToAsset[entry["lod_id"].get<std::string>()] = entry["asset_id"].get<std::string>();
    }
  }

  const auto assetDoc = load_json("content/asset_manifest.json");
  if (assetDoc.is_object() && assetDoc.contains("assets") && assetDoc["assets"].is_array()) {
    for (const auto& asset : assetDoc["assets"]) {
      if (!asset.is_object() || !asset.contains("asset_id") || !asset.contains("mesh")) continue;
      manifests_.assetToMeshPath[asset["asset_id"].get<std::string>()] = asset["mesh"].get<std::string>();
    }
  }
}

ModelResolveResult ModelCache::resolve(std::string_view meshId,
                                       std::string_view lodGroup,
                                       ContentLodTier lodTier) {
  lazy_load_manifests();

  ModelResolveResult result;
  std::string lodSuffix = lodTier == ContentLodTier::Near ? "_lod0" : (lodTier == ContentLodTier::Mid ? "_lod1" : "_lod2");

  std::string lodKey = std::string(lodGroup.empty() ? meshId : lodGroup) + lodSuffix;
  auto lodAssetIt = manifests_.lodToAsset.find(lodKey);
  if (lodAssetIt == manifests_.lodToAsset.end()) {
    lodAssetIt = manifests_.lodToAsset.find(std::string(lodGroup));
  }

  std::string assetId;
  if (lodAssetIt != manifests_.lodToAsset.end()) assetId = lodAssetIt->second;
  if (assetId.empty() && !meshId.empty()) assetId = std::string(meshId);

  auto meshIt = manifests_.assetToMeshPath.find(assetId);
  std::string path = meshIt != manifests_.assetToMeshPath.end() ? meshIt->second : kFallbackModel;
  result.resolvedAssetId = assetId.empty() ? "missing_mesh" : assetId;

  result.model = loader_.load(path);
  result.fallback = !result.model.valid;
  if (result.fallback) {
    result.model = loader_.load(kFallbackModel);
    const std::string warnKey = std::string(meshId) + "|" + std::string(lodGroup);
    if (!warnedMissing_.contains(warnKey)) {
      warnedMissing_.insert(warnKey);
      std::cerr << "[render][model] missing/invalid model for mesh='" << meshId
                << "' lod_group='" << lodGroup << "', using fallback\n";
    }
  }
  return result;
}

AttachmentHookResolveResult ModelCache::resolve_attachment_hook(std::string_view hookId) const {
  static const std::unordered_map<std::string, glm::vec3> kKnownHookOffsets{
      {"banner_socket", {0.0f, 0.92f, 0.0f}},
      {"civ_emblem", {0.0f, 0.56f, 0.0f}},
      {"smoke_stack", {0.28f, 0.82f, 0.0f}},
      {"muzzle_flash", {0.62f, 0.16f, 0.0f}},
      {"selection_badge", {0.0f, -0.9f, 0.0f}},
      {"warning_badge", {0.0f, 0.98f, 0.0f}},
      {"guardian_aura", {0.0f, 0.0f, 0.0f}},
  };

  AttachmentHookResolveResult result{};
  auto it = kKnownHookOffsets.find(std::string(hookId));
  if (it == kKnownHookOffsets.end()) {
    result.fallback = true;
    return result;
  }

  result.normalizedOffset = it->second;
  result.valid = true;
  return result;
}

} // namespace dom::render
