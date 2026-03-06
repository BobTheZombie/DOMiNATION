#pragma once

#include "engine/assets/texture_atlas.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace dom::assets {

struct AssetRecord {
  std::string assetId;
  std::string type;
  std::string civilizationTheme;
  std::vector<std::string> biomeTags;
  std::string meshPath;
  std::string iconPath;
  std::vector<std::string> lodIds;
};

struct MeshEntry {
  std::string meshId;
  std::string sourcePath;
  std::string assetId;
};

class AssetRegistry {
public:
  bool add_asset(const AssetRecord& record);
  bool add_mesh(const MeshEntry& mesh);

  const AssetRecord* get_asset(const std::string& assetId) const;
  const MeshEntry* get_mesh(const std::string& meshId) const;
  const std::unordered_map<std::string, AssetRecord>& assets() const { return assetsById_; }

private:
  std::unordered_map<std::string, AssetRecord> assetsById_;
  std::unordered_map<std::string, MeshEntry> meshesById_;
};

} // namespace dom::assets
