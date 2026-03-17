#pragma once

#include "engine/render/content_resolution.h"
#include "engine/render/gltf_runtime_loader.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <glm/vec3.hpp>

namespace dom::render {

struct ModelResolveResult {
  RuntimeModelData model;
  bool fallback{false};
  std::string resolvedAssetId;
};

struct AttachmentHookResolveResult {
  glm::vec3 normalizedOffset{0.0f, 0.0f, 0.0f};
  bool valid{false};
  bool fallback{false};
  bool assetSpecific{false};
};

class ModelCache {
 public:
  ModelResolveResult resolve(std::string_view meshId,
                             std::string_view lodGroup,
                             ContentLodTier lodTier);
  AttachmentHookResolveResult resolve_attachment_hook(std::string_view resolvedAssetId,
                                                      std::string_view semanticId,
                                                      std::string_view hookId) const;

 private:
  void lazy_load_manifests() const;

  struct ManifestState {
    bool loaded{false};
    std::unordered_map<std::string, std::string> lodToAsset;
    std::unordered_map<std::string, std::string> assetToMeshPath;
    std::unordered_map<std::string, std::unordered_map<std::string, glm::vec3>> assetAttachmentHooks;
  };

  mutable ManifestState manifests_;

  GltfRuntimeLoader loader_;
  std::unordered_set<std::string> warnedMissing_;
};

} // namespace dom::render
