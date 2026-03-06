#pragma once

#include "engine/assets/asset_registry.h"
#include "engine/assets/texture_atlas.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace dom::assets {

struct AssetHandle {
  uint32_t id{0};
  bool fallback{false};
};

struct ManifestStatus {
  bool valid{true};
  std::vector<std::string> warnings;
  std::vector<std::string> errors;
};

class AssetManager {
public:
  bool load_all(const std::filesystem::path& contentRoot = "content");

  const SpriteEntry* get_sprite(const std::string& spriteId) const;
  const MeshEntry* get_mesh(const std::string& meshId) const;
  const AssetRecord* get_icon(const std::string& assetId) const;

  AssetHandle resolve_sprite_handle(const std::string& spriteId);
  AssetHandle resolve_mesh_handle(const std::string& meshId);

  const ManifestStatus& status() const { return status_; }
  const AssetRegistry& registry() const { return registry_; }
  const TextureAtlas& atlas() const { return atlas_; }

private:
  bool load_asset_manifest(const std::filesystem::path& path);
  bool load_atlas_manifest(const std::filesystem::path& path);
  bool load_lod_manifest(const std::filesystem::path& path);
  bool validate_required_manifest(const std::filesystem::path& path, const std::string& name);

  uint32_t nextHandle_{1};
  std::unordered_map<std::string, AssetHandle> spriteHandles_;
  std::unordered_map<std::string, AssetHandle> meshHandles_;
  AssetRegistry registry_;
  TextureAtlas atlas_;
  ManifestStatus status_;
};

} // namespace dom::assets
