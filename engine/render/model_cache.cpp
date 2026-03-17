#include "engine/render/model_cache.h"

#include <array>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace dom::render {

namespace {
constexpr const char* kFallbackModel = "assets_final/fallback/missing_mesh.glb";
}

void ModelCache::lazy_load_manifests() const {
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
      const std::string assetId = asset["asset_id"].get<std::string>();
      manifests_.assetToMeshPath[assetId] = asset["mesh"].get<std::string>();
      if (auto hookIt = asset.find("attachment_hooks"); hookIt != asset.end() && hookIt->is_object()) {
        auto& hookMap = manifests_.assetAttachmentHooks[assetId];
        for (const auto& [semantic, offsetNode] : hookIt->items()) {
          if (!offsetNode.is_array() || offsetNode.size() != 3) continue;
          hookMap[semantic] = glm::vec3{offsetNode[0].get<float>(), offsetNode[1].get<float>(), offsetNode[2].get<float>()};
        }
      }
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

AttachmentHookResolveResult ModelCache::resolve_attachment_hook(std::string_view resolvedAssetId,
                                                               std::string_view semanticId,
                                                               std::string_view hookId) const {
  lazy_load_manifests();
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
  const std::array<std::string, 2> lookupKeys = {std::string(hookId), std::string(semanticId)};

  if (const auto assetIt = manifests_.assetAttachmentHooks.find(std::string(resolvedAssetId));
      assetIt != manifests_.assetAttachmentHooks.end()) {
    for (const auto& key : lookupKeys) {
      if (key.empty()) continue;
      if (const auto offsetIt = assetIt->second.find(key); offsetIt != assetIt->second.end()) {
        result.normalizedOffset = offsetIt->second;
        result.valid = true;
        result.assetSpecific = true;
        return result;
      }
    }
  }

  for (const auto& key : lookupKeys) {
    if (key.empty()) continue;
    const auto it = kKnownHookOffsets.find(key);
    if (it == kKnownHookOffsets.end()) continue;
    result.normalizedOffset = it->second;
    result.valid = true;
    return result;
  }

  result.fallback = true;
  return result;
}

} // namespace dom::render
