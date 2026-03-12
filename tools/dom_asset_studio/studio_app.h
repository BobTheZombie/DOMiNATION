#pragma once

#include "engine/assets/asset_manager.h"
#include "engine/render/render_stylesheet.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace dom::tools {

class DomAssetStudioApp {
public:
  int run(int argc, char** argv);

private:
  struct PreviewSurfacePoint {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
  };

  struct PreviewSurface {
    std::string materialName;
    std::vector<PreviewSurfacePoint> points;
    std::vector<uint32_t> indices;
  };

  struct PreviewAsset {
    std::string sourcePath;
    std::string sourceKind;
    bool loaded{false};
    std::string error;
    std::vector<std::string> meshNames;
    std::vector<std::string> materialNames;
    std::vector<PreviewSurface> surfaces;
    size_t vertexCount{0};
    size_t indexCount{0};
    std::array<float, 3> minBounds{0.0f, 0.0f, 0.0f};
    std::array<float, 3> maxBounds{0.0f, 0.0f, 0.0f};
  };

  struct ScenePlacement {
    std::string label;
    std::string meshRef;
    std::string resolvedStyleId;
    PreviewAsset asset;
    bool visible{true};
    bool valid{false};
    std::string warning;
    float posX{0.0f};
    float posY{0.0f};
    float posZ{0.0f};
    float rotYDeg{0.0f};
    float scale{1.0f};
    std::unordered_map<std::string, std::string> attachments;
  };

  struct AttachmentAnchor {
    std::string socket;
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float radius{0.2f};
    bool selected{false};
    bool valid{true};
    std::string warning;
  };

  struct StylesheetDoc {
    std::string label;
    std::filesystem::path path;
    nlohmann::json json;
    bool dirty{false};
    std::string status;
  };

  struct ManifestDoc {
    std::string label;
    std::filesystem::path path;
    nlohmann::json json;
    bool dirty{false};
    std::string status;
  };

  struct CatalogEntry {
    std::string assetId;
    std::string type;
    std::string renderClass;
    std::string civTag;
    std::string themeTag;
    std::string mesh;
    std::string materialRef;
    std::string lodGroup;
    bool hasLod{false};
    bool hasThumbnail{false};
    bool missingMesh{false};
    bool missingMaterial{false};
    bool missingLod{false};
    bool invalidStylesheetRef{false};
    bool invalidAttachmentHook{false};
    bool missingReference{false};
    int warningCount{0};
    std::filesystem::path thumbnailPath;
  };

  bool init_sdl();
  void shutdown();
  void reload_content();
  void poll_events(bool& running);
  void render_frame();

  void draw_main_menu();
  void draw_project_browser();
  void draw_asset_inspector();
  void draw_stylesheet_editor();
  void draw_asset_catalog();
  void draw_viewport();
  void draw_log_panel();
  void draw_validation_panel();

  void append_log(const std::string& msg);
  void load_stylesheet(StylesheetDoc& doc);
  void save_stylesheet(StylesheetDoc& doc);
  void load_manifest(ManifestDoc& doc);
  void save_manifest(ManifestDoc& doc);
  void save_json_doc(const std::filesystem::path& path, const nlohmann::json& json, std::string& status, const char* kind);
  bool edit_style_layer(nlohmann::json& layer, const char* idPrefix);
  void run_content_validation();
  void run_internal_validation();
  void run_package_workflow();
  void apply_and_reload();
  void apply_manifest_to_stylesheet();
  void refresh_attachment_anchors_from_style();
  void write_attachment_anchors_to_style();
  void refresh_scene_attachment_previews();
  void update_preview_resolution();
  void refresh_preview_asset_from_resolution();
  void rebuild_asset_catalog();
  bool generate_thumbnail(CatalogEntry& entry, bool updateManifestPath);
  void generate_thumbnails_for_visible(bool updateManifestPaths);
  void clear_thumbnail_cache();
  void open_asset_for_preview(const std::filesystem::path& requestedPath, bool fromResolver);
  bool open_asset_for_scene_placement(const std::filesystem::path& requestedPath, ScenePlacement& placement);
  void add_current_asset_to_scene();
  void duplicate_selected_scene_placement();
  void remove_selected_scene_placement();
  void reset_selected_scene_placement();
  void clear_scene();
  void reset_scene_layout();
  void reload_scene_placements();
  void save_scene_layout();
  void load_scene_layout();
  int active_preview_lod() const;
  std::optional<PreviewAsset> load_preview_asset(const std::filesystem::path& path, std::string& error) const;

  SDL_Window* window_{nullptr};
  SDL_GLContext glContext_{nullptr};
  int width_{1600};
  int height_{900};

  dom::assets::AssetManager assets_;
  std::vector<StylesheetDoc> stylesheets_;
  ManifestDoc assetManifest_{"asset_manifest.json", "content/asset_manifest.json"};
  ManifestDoc lodManifest_{"lod_manifest.json", "content/lod_manifest.json"};
  int selectedStylesheet_{0};
  std::string selectedRenderClass_;
  std::string selectedExactId_;
  std::string previewCiv_;
  std::string previewTheme_;
  std::string previewState_;
  int previewDomain_{1};
  int previewLod_{0};
  bool autoPreviewLod_{false};

  std::vector<std::string> logs_;
  std::vector<std::filesystem::path> treeEntries_;
  std::string gltfOpenPath_;
  std::string loadedAssetPath_;

  // Viewport state
  float orbitYaw_{25.0f};
  float orbitPitch_{20.0f};
  float orbitDistance_{8.0f};
  float panX_{0.0f};
  float panY_{0.0f};
  bool showGrid_{true};
  bool wireframe_{false};
  bool showNormals_{false};
  bool turntable_{false};
  bool showAttachments_{true};
  int lightingPreset_{0};
  int backgroundMode_{0};
  int previewStyleVariant_{0};
  int previewZoomPreset_{1};
  int viewportMode_{0};
  int terrainContext_{0};
  int sceneSelectedIndex_{-1};
  std::filesystem::path sceneLayoutPath_{"tools/dom_asset_studio/scene_preview_layout.json"};
  std::vector<ScenePlacement> scenePlacements_;
  std::vector<AttachmentAnchor> previewAnchors_;
  int selectedAttachmentAnchor_{-1};
  bool showAttachmentDiagnostics_{true};
  float attachmentGizmoScale_{1.0f};
  std::unordered_set<std::string> supportedAttachmentTypes_;

  dom::render::ResolvedRenderStyle resolvedPreview_{};
  PreviewAsset previewAsset_{};
  bool showValidationPanel_{true};
  std::vector<std::string> validationMessages_;
  std::string selectedManifestAssetId_;
  std::string selectedManifestLodId_;
  std::string draftAssetId_;
  std::string draftLodId_;
  std::string packageStatus_;

  std::filesystem::path thumbnailCacheDir_{"tools/dom_asset_studio/cache/thumbnails"};
  std::vector<CatalogEntry> catalogEntries_;
  int selectedCatalogIndex_{-1};
  std::string catalogSearch_;
  std::string catalogFilterClass_;
  std::string catalogFilterType_;
  std::string catalogFilterCiv_;
  std::string catalogFilterTheme_;
  bool catalogHasLodOnly_{false};
  bool catalogNoLodOnly_{false};
  bool catalogHasThumbnailOnly_{false};
  bool catalogMissingThumbnailOnly_{false};
  bool catalogWarningsOnly_{false};
  bool catalogMissingRefsOnly_{false};
  bool catalogFavoritesOnly_{false};
  int catalogSortMode_{0};
  std::string compareAssetA_;
  std::string compareAssetB_;
  std::unordered_set<std::string> favoriteAssets_;
  std::map<std::string, unsigned int> thumbnailTextures_;
};

} // namespace dom::tools
