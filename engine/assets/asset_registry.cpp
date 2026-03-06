#include "engine/assets/asset_registry.h"

namespace dom::assets {

bool AssetRegistry::add_asset(const AssetRecord& record) {
  return assetsById_.emplace(record.assetId, record).second;
}

bool AssetRegistry::add_mesh(const MeshEntry& mesh) {
  return meshesById_.emplace(mesh.meshId, mesh).second;
}

const AssetRecord* AssetRegistry::get_asset(const std::string& assetId) const {
  auto it = assetsById_.find(assetId);
  if (it == assetsById_.end()) {
    return nullptr;
  }
  return &it->second;
}

const MeshEntry* AssetRegistry::get_mesh(const std::string& meshId) const {
  auto it = meshesById_.find(meshId);
  if (it == meshesById_.end()) {
    return nullptr;
  }
  return &it->second;
}

} // namespace dom::assets
