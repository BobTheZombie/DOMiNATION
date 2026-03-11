#pragma once

#include "engine/assets/asset_manager.h"
#include "engine/render/render_stylesheet.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
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

  struct StylesheetDoc {
    std::string label;
    std::filesystem::path path;
    nlohmann::json json;
    bool dirty{false};
    std::string status;
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
  void draw_viewport();
  void draw_log_panel();
  void draw_validation_panel();

  void append_log(const std::string& msg);
  void load_stylesheet(StylesheetDoc& doc);
  void save_stylesheet(StylesheetDoc& doc);
  void edit_style_layer(nlohmann::json& layer, const char* idPrefix);
  void run_content_validation();
  void update_preview_resolution();
  void refresh_preview_asset_from_resolution();
  void open_asset_for_preview(const std::filesystem::path& requestedPath, bool fromResolver);
  std::optional<PreviewAsset> load_preview_asset(const std::filesystem::path& path, std::string& error) const;

  SDL_Window* window_{nullptr};
  SDL_GLContext glContext_{nullptr};
  int width_{1600};
  int height_{900};

  dom::assets::AssetManager assets_;
  std::vector<StylesheetDoc> stylesheets_;
  int selectedStylesheet_{0};
  std::string selectedRenderClass_;
  std::string selectedExactId_;
  std::string previewCiv_;
  std::string previewTheme_;
  std::string previewState_;
  int previewDomain_{1};
  int previewLod_{0};

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

  dom::render::ResolvedRenderStyle resolvedPreview_{};
  PreviewAsset previewAsset_{};
  bool showValidationPanel_{true};
  std::vector<std::string> validationMessages_;
};

} // namespace dom::tools
