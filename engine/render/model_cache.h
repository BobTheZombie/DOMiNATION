#pragma once

#include "engine/render/content_resolution.h"
#include "engine/render/gltf_runtime_loader.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace dom::render {

struct ModelResolveResult {
  RuntimeModelData model;
  bool fallback{false};
  std::string resolvedAssetId;
};

class ModelCache {
 public:
  ModelResolveResult resolve(std::string_view meshId,
                             std::string_view lodGroup,
                             ContentLodTier lodTier);

 private:
  void lazy_load_manifests();

  struct ManifestState {
    bool loaded{false};
    std::unordered_map<std::string, std::string> lodToAsset;
    std::unordered_map<std::string, std::string> assetToMeshPath;
  } manifests_;

  GltfRuntimeLoader loader_;
  std::unordered_set<std::string> warnedMissing_;
};

} // namespace dom::render
