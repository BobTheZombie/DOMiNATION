#include "engine/assets/asset_manager.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace dom::assets {
namespace {

nlohmann::json load_json_file(const std::filesystem::path& path, ManifestStatus& status) {
  std::ifstream in(path);
  if (!in.good()) {
    status.valid = false;
    status.errors.push_back("failed to open manifest: " + path.string());
    return {};
  }
  nlohmann::json j;
  in >> j;
  return j;
}

} // namespace

bool AssetManager::validate_required_manifest(const std::filesystem::path& path, const std::string& name) {
  if (!std::filesystem::exists(path)) {
    status_.valid = false;
    status_.errors.push_back("missing " + name + " manifest: " + path.string());
    return false;
  }
  return true;
}

bool AssetManager::load_all(const std::filesystem::path& contentRoot) {
  status_ = {};
  atlas_ = TextureAtlas{};
  registry_ = AssetRegistry{};

  const auto assetManifest = contentRoot / "asset_manifest.json";
  const auto atlasManifest = contentRoot / "atlas_manifest.json";
  const auto biomeManifest = contentRoot / "biome_manifest.json";
  const auto civManifest = contentRoot / "civilization_theme_manifest.json";
  const auto lodManifest = contentRoot / "lod_manifest.json";

  bool ok = true;
  ok &= validate_required_manifest(assetManifest, "asset");
  ok &= validate_required_manifest(atlasManifest, "atlas");
  ok &= validate_required_manifest(biomeManifest, "biome");
  ok &= validate_required_manifest(civManifest, "civilization_theme");
  ok &= validate_required_manifest(lodManifest, "lod");
  if (!ok) {
    return false;
  }

  ok &= load_asset_manifest(assetManifest);
  ok &= load_atlas_manifest(atlasManifest);
  ok &= load_lod_manifest(lodManifest);

  return ok && status_.valid;
}

bool AssetManager::load_asset_manifest(const std::filesystem::path& path) {
  const auto j = load_json_file(path, status_);
  if (!j.contains("assets") || !j["assets"].is_array()) {
    status_.valid = false;
    status_.errors.push_back("asset_manifest missing assets array");
    return false;
  }

  for (const auto& item : j["assets"]) {
    AssetRecord record;
    record.assetId = item.value("asset_id", "");
    record.type = item.value("type", "unknown");
    record.civilizationTheme = item.value("civilization_theme", "neutral");
    record.meshPath = item.value("mesh", "");
    record.iconPath = item.value("icon", "");
    if (item.contains("biome_tags")) {
      record.biomeTags = item["biome_tags"].get<std::vector<std::string>>();
    }
    if (item.contains("lods")) {
      record.lodIds = item["lods"].get<std::vector<std::string>>();
    }

    if (record.assetId.empty()) {
      status_.warnings.push_back("asset entry missing asset_id; skipped");
      continue;
    }
    if (!registry_.add_asset(record)) {
      status_.warnings.push_back("duplicate asset id encountered: " + record.assetId);
    }
  }
  return true;
}

bool AssetManager::load_atlas_manifest(const std::filesystem::path& path) {
  const auto j = load_json_file(path, status_);
  if (!j.contains("sprites") || !j["sprites"].is_array()) {
    status_.warnings.push_back("atlas_manifest has no sprites array");
    return true;
  }

  for (const auto& item : j["sprites"]) {
    SpriteEntry entry;
    entry.spriteId = item.value("sprite_id", "");
    entry.atlasId = item.value("atlas", "");
    if (item.contains("rect") && item["rect"].is_array() && item["rect"].size() == 4) {
      for (size_t i = 0; i < 4; ++i) entry.rect[i] = item["rect"][i].get<int>();
    }
    if (item.contains("pivot") && item["pivot"].is_array() && item["pivot"].size() == 2) {
      entry.pivot[0] = item["pivot"][0].get<float>();
      entry.pivot[1] = item["pivot"][1].get<float>();
    }
    if (item.contains("tags")) entry.tags = item["tags"].get<std::vector<std::string>>();

    if (entry.spriteId.empty()) {
      status_.warnings.push_back("sprite entry missing sprite_id; skipped");
      continue;
    }
    if (!atlas_.add_sprite(entry)) {
      status_.warnings.push_back("duplicate sprite id encountered: " + entry.spriteId);
    }
  }
  return true;
}

bool AssetManager::load_lod_manifest(const std::filesystem::path& path) {
  const auto j = load_json_file(path, status_);
  if (!j.contains("lod_entries") || !j["lod_entries"].is_array()) {
    status_.warnings.push_back("lod_manifest has no lod_entries array");
    return true;
  }

  for (const auto& item : j["lod_entries"]) {
    MeshEntry mesh;
    mesh.meshId = item.value("lod_id", "");
    mesh.assetId = item.value("asset_id", "");
    mesh.sourcePath = item.value("mesh", "");
    if (mesh.meshId.empty()) {
      status_.warnings.push_back("lod entry missing lod_id; skipped");
      continue;
    }
    if (!registry_.add_mesh(mesh)) {
      status_.warnings.push_back("duplicate mesh id encountered: " + mesh.meshId);
    }
  }
  return true;
}

const SpriteEntry* AssetManager::get_sprite(const std::string& spriteId) const {
  const auto* sprite = atlas_.get_sprite(spriteId);
  if (sprite) {
    return sprite;
  }
  return atlas_.get_sprite("missing_texture");
}

const MeshEntry* AssetManager::get_mesh(const std::string& meshId) const {
  const auto* mesh = registry_.get_mesh(meshId);
  if (mesh) {
    return mesh;
  }
  return registry_.get_mesh("missing_mesh");
}

const AssetRecord* AssetManager::get_icon(const std::string& assetId) const {
  const auto* asset = registry_.get_asset(assetId);
  if (asset && !asset->iconPath.empty()) {
    return asset;
  }
  return registry_.get_asset("missing_icon");
}

AssetHandle AssetManager::resolve_sprite_handle(const std::string& spriteId) {
  auto it = spriteHandles_.find(spriteId);
  if (it != spriteHandles_.end()) {
    return it->second;
  }

  AssetHandle handle{nextHandle_++, false};
  if (!atlas_.get_sprite(spriteId)) {
    std::cerr << "ASSET_WARN missing sprite id '" << spriteId << "', using fallback\n";
    handle.fallback = true;
  }
  spriteHandles_[spriteId] = handle;
  return handle;
}

AssetHandle AssetManager::resolve_mesh_handle(const std::string& meshId) {
  auto it = meshHandles_.find(meshId);
  if (it != meshHandles_.end()) {
    return it->second;
  }

  AssetHandle handle{nextHandle_++, false};
  if (!registry_.get_mesh(meshId)) {
    std::cerr << "ASSET_WARN missing mesh id '" << meshId << "', using fallback\n";
    handle.fallback = true;
  }
  meshHandles_[meshId] = handle;
  return handle;
}

} // namespace dom::assets
