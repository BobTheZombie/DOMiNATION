#include "tools/dom_asset_studio/studio_app.h"

#include "engine/render/content_resolution.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#ifdef DOM_HAS_IMGUI
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>

namespace dom::tools {

namespace {
const char* kDomainNames[] = {"Terrain", "Unit", "Building", "Object"};
const char* kLodNames[] = {"Near", "Mid", "Far"};
const char* kVariantSourceNames[] = {"Resolver (class/exact)", "Exact mapping only", "Render class only"};

struct BufferViewRef {
  int buffer{-1};
  size_t byteOffset{0};
  size_t byteLength{0};
  size_t byteStride{0};
};

size_t component_size(int componentType) {
  switch (componentType) {
    case 5120:
    case 5121: return 1;
    case 5122:
    case 5123: return 2;
    case 5125:
    case 5126: return 4;
    default: return 0;
  }
}

bool read_file_bytes(const std::filesystem::path& path, std::vector<uint8_t>& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in.good()) return false;
  in.seekg(0, std::ios::end);
  const auto len = static_cast<size_t>(in.tellg());
  in.seekg(0, std::ios::beg);
  out.resize(len);
  if (len > 0) in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(len));
  return in.good() || in.eof();
}

uint32_t read_u32_le(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8U) |
         (static_cast<uint32_t>(p[2]) << 16U) |
         (static_cast<uint32_t>(p[3]) << 24U);
}

} // namespace

int DomAssetStudioApp::run(int, char**) {
  if (!init_sdl()) return 1;

  reload_content();
  bool running = true;
  while (running) {
    poll_events(running);
    render_frame();
  }

  shutdown();
  return 0;
}

bool DomAssetStudioApp::init_sdl() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return false;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  window_ = SDL_CreateWindow("DOM Asset Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             width_, height_, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window_) return false;

  glContext_ = SDL_GL_CreateContext(window_);
  if (!glContext_) return false;
  SDL_GL_SetSwapInterval(1);

#ifdef DOM_HAS_IMGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(window_, glContext_);
  ImGui_ImplOpenGL3_Init("#version 130");
#endif

  stylesheets_ = {
      {"terrain_styles.json", "content/terrain_styles.json"},
      {"unit_styles.json", "content/unit_styles.json"},
      {"building_styles.json", "content/building_styles.json"},
      {"object_styles.json", "content/object_styles.json"},
  };
  for (auto& sheet : stylesheets_) load_stylesheet(sheet);
  load_manifest(assetManifest_);
  load_manifest(lodManifest_);

  return true;
}

void DomAssetStudioApp::shutdown() {
#ifdef DOM_HAS_IMGUI
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
#endif
  if (glContext_) SDL_GL_DeleteContext(glContext_);
  if (window_) SDL_DestroyWindow(window_);
  SDL_Quit();
}

void DomAssetStudioApp::reload_content() {
  assets_.load_all("content");
  dom::render::reload_render_stylesheets();
  dom::render::load_render_stylesheets();
  update_preview_resolution();

  treeEntries_.clear();
  if (std::filesystem::exists("content")) {
    for (const auto& e : std::filesystem::recursive_directory_iterator("content")) {
      if (e.is_regular_file() && treeEntries_.size() < 1024) treeEntries_.push_back(e.path());
    }
  }

  append_log("Content and stylesheet data loaded.");
}

void DomAssetStudioApp::poll_events(bool& running) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
#ifdef DOM_HAS_IMGUI
    ImGui_ImplSDL2_ProcessEvent(&event);
#endif
    if (event.type == SDL_QUIT) running = false;
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
      width_ = event.window.data1;
      height_ = event.window.data2;
    }
  }
}

void DomAssetStudioApp::render_frame() {
#ifdef DOM_HAS_IMGUI
  if (turntable_) orbitYaw_ = std::fmod(orbitYaw_ + 0.4f, 360.0f);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  draw_main_menu();
  draw_project_browser();
  draw_asset_inspector();
  draw_stylesheet_editor();
  draw_viewport();
  draw_log_panel();
  if (showValidationPanel_) draw_validation_panel();

  ImGui::Render();
#endif

  glViewport(0, 0, width_, height_);
  glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#ifdef DOM_HAS_IMGUI
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
  SDL_GL_SwapWindow(window_);
}

void DomAssetStudioApp::draw_main_menu() {
#ifdef DOM_HAS_IMGUI
  if (!ImGui::BeginMainMenuBar()) return;

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Reload Content")) reload_content();
    if (ImGui::MenuItem("Apply and Reload")) apply_and_reload();
    if (ImGui::MenuItem("Save Manifests")) {
      save_manifest(assetManifest_);
      save_manifest(lodManifest_);
      apply_and_reload();
    }
    if (ImGui::MenuItem("Apply Asset->Stylesheet Mapping")) apply_manifest_to_stylesheet();
    if (ImGui::MenuItem("Quit")) {
      SDL_Event quit{};
      quit.type = SDL_QUIT;
      SDL_PushEvent(&quit);
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem("Reset View")) {
      orbitYaw_ = 25.0f; orbitPitch_ = 20.0f; orbitDistance_ = 8.0f; panX_ = panY_ = 0.0f;
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("Validation", nullptr, &showValidationPanel_);
    ImGui::MenuItem("Grid", nullptr, &showGrid_);
    ImGui::MenuItem("Wireframe", nullptr, &wireframe_);
    ImGui::MenuItem("Normals", nullptr, &showNormals_);
    ImGui::MenuItem("Turntable", nullptr, &turntable_);
    ImGui::MenuItem("Attachments", nullptr, &showAttachments_);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Asset")) {
    ImGui::InputText("Open glTF/GLB", &gltfOpenPath_);
    if (ImGui::MenuItem("Open For Preview")) open_asset_for_preview(gltfOpenPath_, false);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Stylesheet")) {
    if (ImGui::MenuItem("Save Current Stylesheet")) save_stylesheet(stylesheets_[selectedStylesheet_]);
    if (ImGui::MenuItem("Save All Stylesheets")) for (auto& s : stylesheets_) save_stylesheet(s);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Export")) {
    if (ImGui::MenuItem("Export Engine-Compatible Content (save + validate)")) {
      apply_and_reload();
      run_internal_validation();
      run_content_validation();
      run_package_workflow();
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Validate")) {
    if (ImGui::MenuItem("Run Content Validation")) {
      run_internal_validation();
      run_content_validation();
    }
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();
#endif
}

void DomAssetStudioApp::draw_project_browser() {
#ifdef DOM_HAS_IMGUI
  if (!ImGui::Begin("Project Browser")) { ImGui::End(); return; }
  ImGui::Text("content/");
  ImGui::Separator();
  for (const auto& p : treeEntries_) {
    std::string rel = p.string();
    bool isGltf = p.extension() == ".gltf" || p.extension() == ".glb";
    if (ImGui::Selectable(rel.c_str())) {
      append_log("Selected file: " + rel);
      if (isGltf) {
        gltfOpenPath_ = rel;
        open_asset_for_preview(rel, false);
      }
    }
  }
  ImGui::End();
#endif
}

void DomAssetStudioApp::draw_asset_inspector() {
#ifdef DOM_HAS_IMGUI
  if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }

  const auto& status = assets_.status();
  ImGui::Text("Manifest Status: %s", status.valid ? "OK" : "INVALID");
  ImGui::Text("Asset count: %d", static_cast<int>(assets_.registry().assets().size()));

  if (ImGui::CollapsingHeader("Manifest Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const auto& e : status.errors) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", e.c_str());
    for (const auto& w : status.warnings) ImGui::TextColored(ImVec4(1, 0.85f, 0.4f, 1), "%s", w.c_str());
  }

  if (ImGui::CollapsingHeader("Resolved Preview Metadata", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("style_id: %s", resolvedPreview_.styleId.c_str());
    ImGui::Text("mesh ref: %s", resolvedPreview_.mesh.c_str());
    ImGui::Text("material: %s", resolvedPreview_.material.c_str());
    ImGui::Text("lod_group: %s", resolvedPreview_.lodGroup.c_str());
    ImGui::Text("loaded asset: %s", loadedAssetPath_.empty() ? "<none>" : loadedAssetPath_.c_str());

    if (previewAsset_.loaded) {
      ImGui::Text("meshes: %d", static_cast<int>(previewAsset_.meshNames.size()));
      ImGui::Text("materials: %d", static_cast<int>(previewAsset_.materialNames.size()));
      ImGui::Text("vertices/indices: %d / %d",
                  static_cast<int>(previewAsset_.vertexCount),
                  static_cast<int>(previewAsset_.indexCount));
      ImGui::Text("bounds min: %.2f %.2f %.2f", previewAsset_.minBounds[0], previewAsset_.minBounds[1], previewAsset_.minBounds[2]);
      ImGui::Text("bounds max: %.2f %.2f %.2f", previewAsset_.maxBounds[0], previewAsset_.maxBounds[1], previewAsset_.maxBounds[2]);

      if (ImGui::TreeNode("Mesh Names")) {
        for (const auto& name : previewAsset_.meshNames) ImGui::BulletText("%s", name.c_str());
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Material Names")) {
        for (const auto& name : previewAsset_.materialNames) ImGui::BulletText("%s", name.c_str());
        ImGui::TreePop();
      }
    } else if (!previewAsset_.error.empty()) {
      ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Asset preview unavailable: %s", previewAsset_.error.c_str());
    }

    if (ImGui::TreeNode("Attachments / Sockets")) {
      if (resolvedPreview_.attachments.empty()) {
        ImGui::TextDisabled("No attachment mappings in style");
      } else {
        for (const auto& [socket, target] : resolvedPreview_.attachments) {
          ImGui::BulletText("%s -> %s", socket.c_str(), target.c_str());
        }
      }
      ImGui::TreePop();
    }
  }

  if (ImGui::CollapsingHeader("Asset Manifest / LOD", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (!assetManifest_.json.is_object() || !lodManifest_.json.is_object()) {
      ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Manifests unavailable for editing");
    } else {
      if (!assetManifest_.json.contains("assets") || !assetManifest_.json["assets"].is_array()) assetManifest_.json["assets"] = nlohmann::json::array();
      if (!lodManifest_.json.contains("lod_entries") || !lodManifest_.json["lod_entries"].is_array()) lodManifest_.json["lod_entries"] = nlohmann::json::array();

      if (ImGui::BeginCombo("Asset Entry", selectedManifestAssetId_.empty() ? "<new / none>" : selectedManifestAssetId_.c_str())) {
        for (auto& asset : assetManifest_.json["assets"]) {
          const std::string aid = asset.value("asset_id", "");
          if (aid.empty()) continue;
          bool sel = selectedManifestAssetId_ == aid;
          if (ImGui::Selectable(aid.c_str(), sel)) {
            selectedManifestAssetId_ = aid;
            draftAssetId_ = aid;
          }
        }
        ImGui::EndCombo();
      }
      ImGui::InputText("Asset ID", &draftAssetId_);
      if (ImGui::Button("Create/Select Asset")) {
        if (!draftAssetId_.empty()) {
          bool found = false;
          for (auto& asset : assetManifest_.json["assets"]) {
            if (asset.value("asset_id", "") == draftAssetId_) {
              found = true;
              break;
            }
          }
          if (!found) {
            nlohmann::json entry = {
              {"asset_id", draftAssetId_}, {"type", "object"}, {"civilization_theme", "neutral"},
              {"biome_tags", nlohmann::json::array({"any"})}, {"mesh", ""}, {"icon", ""}, {"lods", nlohmann::json::array()}
            };
            assetManifest_.json["assets"].push_back(entry);
            assetManifest_.dirty = true;
          }
          selectedManifestAssetId_ = draftAssetId_;
        }
      }

      nlohmann::json* selectedAsset = nullptr;
      for (auto& asset : assetManifest_.json["assets"]) {
        if (asset.value("asset_id", "") == selectedManifestAssetId_) {
          selectedAsset = &asset;
          break;
        }
      }
      if (selectedAsset) {
        const nlohmann::json before = *selectedAsset;
        std::string type = selectedAsset->value("type", "object");
        std::string civTheme = selectedAsset->value("civilization_theme", "neutral");
        std::string mesh = selectedAsset->value("mesh", "");
        std::string icon = selectedAsset->value("icon", "");
        std::string renderClass = selectedAsset->value("render_class", "");
        std::string category = selectedAsset->value("category", "");
        std::string attachmentProfile = selectedAsset->value("attachment_profile", "");
        std::string notes = selectedAsset->value("notes", "");
        std::string assetStatus = selectedAsset->value("status", "draft");
        std::string materialRef = selectedAsset->value("material_ref", "");
        std::string previewThumbnail = selectedAsset->value("preview_thumbnail", "");
        std::string civTag = selectedAsset->value("civ_tag", "");
        std::string themeTag = selectedAsset->value("theme_tag", "");
        ImGui::InputText("Type", &type);
        ImGui::InputText("Civilization Theme", &civTheme);
        ImGui::InputText("Mesh", &mesh);
        ImGui::InputText("Icon", &icon);
        ImGui::InputText("Render Class", &renderClass);
        ImGui::InputText("Category Metadata", &category);
        ImGui::InputText("Attachment Profile", &attachmentProfile);
        ImGui::InputText("Material Ref", &materialRef);
        ImGui::InputText("Civ Tag", &civTag);
        ImGui::InputText("Theme Tag", &themeTag);
        ImGui::InputText("Preview/Thumbnail", &previewThumbnail);
        ImGui::InputText("Status", &assetStatus);
        ImGui::InputText("Notes", &notes);
        (*selectedAsset)["type"] = type;
        (*selectedAsset)["civilization_theme"] = civTheme;
        (*selectedAsset)["mesh"] = mesh;
        (*selectedAsset)["icon"] = icon;
        (*selectedAsset)["render_class"] = renderClass;
        (*selectedAsset)["category"] = category;
        (*selectedAsset)["attachment_profile"] = attachmentProfile;
        (*selectedAsset)["material_ref"] = materialRef;
        (*selectedAsset)["civ_tag"] = civTag;
        (*selectedAsset)["theme_tag"] = themeTag;
        (*selectedAsset)["preview_thumbnail"] = previewThumbnail;
        (*selectedAsset)["status"] = assetStatus;
        (*selectedAsset)["notes"] = notes;
        if (before != *selectedAsset) assetManifest_.dirty = true;
      }

      ImGui::SeparatorText("LOD Entries");
      if (ImGui::BeginCombo("LOD Entry", selectedManifestLodId_.empty() ? "<new / none>" : selectedManifestLodId_.c_str())) {
        for (auto& lod : lodManifest_.json["lod_entries"]) {
          const std::string lid = lod.value("lod_id", "");
          if (lid.empty()) continue;
          bool sel = selectedManifestLodId_ == lid;
          if (ImGui::Selectable(lid.c_str(), sel)) {
            selectedManifestLodId_ = lid;
            draftLodId_ = lid;
          }
        }
        ImGui::EndCombo();
      }
      ImGui::InputText("LOD ID", &draftLodId_);
      if (ImGui::Button("Create/Select LOD")) {
        if (!draftLodId_.empty()) {
          bool found = false;
          for (auto& lod : lodManifest_.json["lod_entries"]) {
            if (lod.value("lod_id", "") == draftLodId_) { found = true; break; }
          }
          if (!found) {
            nlohmann::json entry = {{"lod_id", draftLodId_}, {"asset_id", selectedManifestAssetId_}, {"mesh", ""}, {"screen_size", 1.0}, {"category", ""}, {"render_class", ""}, {"attachments", nlohmann::json::object()}};
            lodManifest_.json["lod_entries"].push_back(entry);
            lodManifest_.dirty = true;
          }
          selectedManifestLodId_ = draftLodId_;
        }
      }

      for (auto& lod : lodManifest_.json["lod_entries"]) {
        if (lod.value("lod_id", "") == selectedManifestLodId_) {
          const nlohmann::json before = lod;
          std::string aid = lod.value("asset_id", "");
          std::string mesh = lod.value("mesh", "");
          std::string category = lod.value("category", "");
          std::string renderClass = lod.value("render_class", "");
          std::string lodGroupId = lod.value("lod_group_id", lod.value("lod_id", ""));
          std::string nearRef = lod.value("near_asset", "");
          std::string midRef = lod.value("mid_asset", "");
          std::string farRef = lod.value("far_asset", "");
          std::string fallbackRef = lod.value("fallback_asset", "");
          float screenSize = lod.value("screen_size", 1.0f);
          ImGui::InputText("LOD Asset ID", &aid);
          ImGui::InputText("LOD Mesh", &mesh);
          ImGui::InputText("LOD Category", &category);
          ImGui::InputText("LOD Render Class", &renderClass);
          ImGui::InputText("LOD Group ID", &lodGroupId);
          ImGui::InputText("Near Asset Ref", &nearRef);
          ImGui::InputText("Mid Asset Ref", &midRef);
          ImGui::InputText("Far Asset Ref", &farRef);
          ImGui::InputText("Fallback Asset Ref", &fallbackRef);
          ImGui::SliderFloat("LOD Screen Size", &screenSize, 0.0f, 1.0f);
          lod["asset_id"] = aid;
          lod["mesh"] = mesh;
          lod["category"] = category;
          lod["render_class"] = renderClass;
          lod["lod_group_id"] = lodGroupId;
          lod["near_asset"] = nearRef;
          lod["mid_asset"] = midRef;
          lod["far_asset"] = farRef;
          lod["fallback_asset"] = fallbackRef;
          lod["screen_size"] = screenSize;
          if (before != lod) lodManifest_.dirty = true;
          break;
        }
      }

      if (ImGui::Button("Save Manifests")) {
        save_manifest(assetManifest_);
        save_manifest(lodManifest_);
        apply_and_reload();
      }
      ImGui::SameLine();
      if (ImGui::Button("Apply + Reload")) apply_and_reload();
    }
  }

  ImGui::End();
#endif
}

bool DomAssetStudioApp::edit_style_layer(nlohmann::json& layer, const char* idPrefix) {
#ifdef DOM_HAS_IMGUI
  std::string styleId = layer.value("style_id", "");
  std::string renderClass = layer.value("render_class", "");
  std::string mesh = layer.value("mesh", "");
  std::string material = layer.value("material", "");
  std::string lodGroup = layer.value("lod_group", "");
  std::string civTag = layer.value("civ_tag", "");
  std::string themeTag = layer.value("theme_tag", "");
  std::string category = layer.value("category", "");
  std::string assetId = layer.value("asset_id", "");
  ImGui::InputText((std::string("Style ID##") + idPrefix).c_str(), &styleId);
  ImGui::InputText((std::string("Render Class##") + idPrefix).c_str(), &renderClass);
  ImGui::InputText((std::string("Mesh##") + idPrefix).c_str(), &mesh);
  ImGui::InputText((std::string("Material##") + idPrefix).c_str(), &material);
  ImGui::InputText((std::string("LOD Group##") + idPrefix).c_str(), &lodGroup);
  ImGui::InputText((std::string("Asset ID##") + idPrefix).c_str(), &assetId);
  ImGui::InputText((std::string("Category##") + idPrefix).c_str(), &category);
  ImGui::InputText((std::string("Civ Tag##") + idPrefix).c_str(), &civTag);
  ImGui::InputText((std::string("Theme Tag##") + idPrefix).c_str(), &themeTag);
  const nlohmann::json before = layer;
  layer["style_id"] = styleId;
  layer["render_class"] = renderClass;
  layer["mesh"] = mesh;
  layer["material"] = material;
  layer["lod_group"] = lodGroup;
  layer["asset_id"] = assetId;
  layer["category"] = category;
  layer["civ_tag"] = civTag;
  layer["theme_tag"] = themeTag;

  if (!layer.contains("attachments") || !layer["attachments"].is_object()) layer["attachments"] = nlohmann::json::object();
  if (ImGui::TreeNode((std::string("Attachments##") + idPrefix).c_str())) {
    for (auto& [k, v] : layer["attachments"].items()) {
      std::string socket = k;
      std::string ref = v.is_string() ? v.get<std::string>() : "";
      ImGui::Text("%s", socket.c_str());
      ImGui::SameLine();
      if (ImGui::InputText((std::string("##att") + idPrefix + socket).c_str(), &ref)) v = ref;
    }
    ImGui::TreePop();
  }
  return before != layer;
#else
  return false;
#endif
}

void DomAssetStudioApp::draw_stylesheet_editor() {
#ifdef DOM_HAS_IMGUI
  if (!ImGui::Begin("Stylesheet Editor")) { ImGui::End(); return; }

  std::vector<const char*> labels;
  for (auto& s : stylesheets_) labels.push_back(s.label.c_str());
  ImGui::Combo("Domain Sheet", &selectedStylesheet_, labels.data(), static_cast<int>(labels.size()));

  auto& sheet = stylesheets_[selectedStylesheet_];
  ImGui::Text("Path: %s", sheet.path.string().c_str());
  ImGui::Text("Status: %s%s", sheet.status.c_str(), sheet.dirty ? " (dirty)" : "");

  if (!sheet.json.is_object()) {
    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Invalid JSON object");
    ImGui::End();
    return;
  }

  if (sheet.json.contains("render_classes") && sheet.json["render_classes"].is_object()) {
    if (ImGui::BeginCombo("Render Class", selectedRenderClass_.empty() ? "<none>" : selectedRenderClass_.c_str())) {
      for (auto& [k, _] : sheet.json["render_classes"].items()) {
        bool sel = selectedRenderClass_ == k;
        if (ImGui::Selectable(k.c_str(), sel)) { selectedRenderClass_ = k; update_preview_resolution(); }
      }
      ImGui::EndCombo();
    }
  }
  if (sheet.json.contains("exact_mappings") && sheet.json["exact_mappings"].is_object()) {
    if (ImGui::BeginCombo("Exact Mapping", selectedExactId_.empty() ? "<none>" : selectedExactId_.c_str())) {
      for (auto& [k, _] : sheet.json["exact_mappings"].items()) {
        bool sel = selectedExactId_ == k;
        if (ImGui::Selectable(k.c_str(), sel)) { selectedExactId_ = k; update_preview_resolution(); }
      }
      ImGui::EndCombo();
    }
  }

  bool changed = false;
  changed |= ImGui::Combo("Preview Domain", &previewDomain_, kDomainNames, 4);
  changed |= ImGui::Combo("Variant Source", &previewStyleVariant_, kVariantSourceNames, 3);
  changed |= ImGui::InputText("Preview Civ", &previewCiv_);
  changed |= ImGui::InputText("Preview Theme", &previewTheme_);
  changed |= ImGui::InputText("Preview State", &previewState_);
  changed |= ImGui::Combo("Preview LOD", &previewLod_, kLodNames, 3);
  if (changed) update_preview_resolution();

  if (sheet.json.contains("render_classes") && sheet.json["render_classes"].contains(selectedRenderClass_)) {
    auto& rc = sheet.json["render_classes"][selectedRenderClass_];
    if (!rc.contains("default") || !rc["default"].is_object()) rc["default"] = nlohmann::json::object();
    ImGui::SeparatorText("Default Mapping");
    if (edit_style_layer(rc["default"], "rc_default")) sheet.dirty = true;
  }

  if (ImGui::Button("Save Sheet")) save_stylesheet(sheet);
  ImGui::SameLine();
  if (ImGui::Button("Re-resolve Preview")) update_preview_resolution();

  ImGui::SeparatorText("Resolved Preview Chain");
  ImGui::Text("style_id: %s", resolvedPreview_.styleId.c_str());
  ImGui::Text("mesh: %s", resolvedPreview_.mesh.c_str());
  ImGui::Text("material: %s", resolvedPreview_.material.c_str());
  ImGui::Text("lod_group: %s", resolvedPreview_.lodGroup.c_str());
  ImGui::Text("fallback: %s", resolvedPreview_.fallback ? "true" : "false");

  ImGui::End();
#endif
}

void DomAssetStudioApp::draw_viewport() {
#ifdef DOM_HAS_IMGUI
  if (!ImGui::Begin("Viewport")) { ImGui::End(); return; }

  ImGui::Text("3D preview (manifest + stylesheet resolved)");
  ImGui::SliderFloat("Orbit Yaw", &orbitYaw_, -180.0f, 180.0f);
  ImGui::SliderFloat("Orbit Pitch", &orbitPitch_, -89.0f, 89.0f);
  ImGui::SliderFloat("Zoom", &orbitDistance_, 1.0f, 20.0f);
  ImGui::SliderFloat("Pan X", &panX_, -10.0f, 10.0f);
  ImGui::SliderFloat("Pan Y", &panY_, -10.0f, 10.0f);
  ImGui::Checkbox("Turntable", &turntable_);
  ImGui::Checkbox("Grid", &showGrid_);
  ImGui::Checkbox("Wireframe", &wireframe_);
  ImGui::Checkbox("Normals", &showNormals_);
  ImGui::Checkbox("Sockets/Attachments", &showAttachments_);
  ImGui::Combo("Lighting Preset", &lightingPreset_, "Studio\0Sunset\0Flat\0");
  ImGui::Combo("Background", &backgroundMode_, "Dark\0Sky\0Neutral\0");

  ImVec2 p = ImGui::GetCursorScreenPos();
  ImVec2 size = ImGui::GetContentRegionAvail();
  if (size.x < 10 || size.y < 10) { ImGui::End(); return; }

  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImU32 bg = backgroundMode_ == 1 ? IM_COL32(80, 120, 170, 255) : (backgroundMode_ == 2 ? IM_COL32(120, 120, 120, 255) : IM_COL32(32, 32, 40, 255));
  draw->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg);

  if (showGrid_) {
    for (int i = 0; i <= 20; ++i) {
      float t = static_cast<float>(i) / 20.0f;
      draw->AddLine(ImVec2(p.x + t * size.x, p.y), ImVec2(p.x + t * size.x, p.y + size.y), IM_COL32(70, 70, 75, 120));
      draw->AddLine(ImVec2(p.x, p.y + t * size.y), ImVec2(p.x + size.x, p.y + t * size.y), IM_COL32(70, 70, 75, 120));
    }
  }

  const float yaw = orbitYaw_ * 0.0174533f;
  const float pitch = orbitPitch_ * 0.0174533f;
  const float cy = std::cos(yaw);
  const float sy = std::sin(yaw);
  const float cp = std::cos(pitch);
  const float sp = std::sin(pitch);

  auto project = [&](const PreviewSurfacePoint& v, float& ox, float& oy, float& depth) {
    float x = v.x;
    float yv = v.y;
    float z = v.z;
    float rx = x * cy + z * sy;
    float rz = -x * sy + z * cy;
    float ry = yv * cp - rz * sp;
    float rz2 = yv * sp + rz * cp;
    const float d = orbitDistance_ + 4.0f + rz2;
    const float scale = 170.0f / std::max(1.0f, d);
    ox = p.x + size.x * (0.5f + panX_ * 0.03f) + rx * scale;
    oy = p.y + size.y * (0.55f + panY_ * 0.03f) - ry * scale;
    depth = d;
  };

  if (previewAsset_.loaded && !previewAsset_.surfaces.empty()) {
    for (const auto& surface : previewAsset_.surfaces) {
      for (size_t i = 0; i + 2 < surface.indices.size(); i += 3) {
        const auto i0 = surface.indices[i + 0];
        const auto i1 = surface.indices[i + 1];
        const auto i2 = surface.indices[i + 2];
        if (i0 >= surface.points.size() || i1 >= surface.points.size() || i2 >= surface.points.size()) continue;
        float x0, y0, z0, x1, y1, z1, x2, y2, z2;
        project(surface.points[i0], x0, y0, z0);
        project(surface.points[i1], x1, y1, z1);
        project(surface.points[i2], x2, y2, z2);
        if (wireframe_) {
          draw->AddTriangle(ImVec2(x0, y0), ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 230, 120, 210), 1.0f);
        } else {
          draw->AddTriangleFilled(ImVec2(x0, y0), ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(120, 210, 255, 140));
          draw->AddTriangle(ImVec2(x0, y0), ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(20, 40, 65, 200), 1.0f);
        }
        if (showNormals_) {
          const float cx = (x0 + x1 + x2) / 3.0f;
          const float cy2 = (y0 + y1 + y2) / 3.0f;
          draw->AddLine(ImVec2(cx, cy2), ImVec2(cx, cy2 - 8.0f), IM_COL32(255, 100, 100, 220), 1.0f);
        }
      }
    }
  }

  if (showAttachments_) {
    float sx = p.x + size.x * 0.12f;
    float sy2 = p.y + size.y * 0.15f;
    for (const auto& [socket, target] : resolvedPreview_.attachments) {
      draw->AddCircleFilled(ImVec2(sx, sy2), 3.0f, IM_COL32(255, 190, 80, 220));
      draw->AddText(ImVec2(sx + 8.0f, sy2 - 8.0f), IM_COL32(255, 230, 190, 255), (socket + " -> " + target).c_str());
      sy2 += 18.0f;
    }
  }

  draw->AddText(ImVec2(p.x + 8, p.y + 8), IM_COL32_WHITE, ("mesh: " + resolvedPreview_.mesh).c_str());
  draw->AddText(ImVec2(p.x + 8, p.y + 26), IM_COL32_WHITE, ("material: " + resolvedPreview_.material).c_str());

  ImGui::End();
#endif
}

void DomAssetStudioApp::draw_log_panel() {
#ifdef DOM_HAS_IMGUI
  if (!ImGui::Begin("Log / Output")) { ImGui::End(); return; }
  for (const auto& line : logs_) ImGui::TextUnformatted(line.c_str());
  ImGui::End();
#endif
}

void DomAssetStudioApp::draw_validation_panel() {
#ifdef DOM_HAS_IMGUI
  if (!ImGui::Begin("Validation")) { ImGui::End(); return; }
  if (ImGui::Button("Run Validation")) {
    run_internal_validation();
    run_content_validation();
  }
  ImGui::SameLine();
  if (ImGui::Button("Revalidate Current Project")) {
    run_internal_validation();
    run_content_validation();
  }
  ImGui::Text("Dirty: manifest=%s lod=%s stylesheet=%s",
              assetManifest_.dirty ? "yes" : "no",
              lodManifest_.dirty ? "yes" : "no",
              stylesheets_[selectedStylesheet_].dirty ? "yes" : "no");
  for (const auto& m : validationMessages_) ImGui::TextUnformatted(m.c_str());
  if (!packageStatus_.empty()) {
    ImGui::SeparatorText("Packaging");
    ImGui::TextUnformatted(packageStatus_.c_str());
  }
  ImGui::End();
#endif
}

void DomAssetStudioApp::append_log(const std::string& msg) {
  logs_.push_back(msg);
  if (logs_.size() > 256) logs_.erase(logs_.begin());
}

void DomAssetStudioApp::load_stylesheet(StylesheetDoc& doc) {
  std::ifstream in(doc.path);
  if (!in.good()) {
    doc.status = "missing file";
    append_log("Missing stylesheet: " + doc.path.string());
    return;
  }
  try {
    in >> doc.json;
    doc.status = "loaded";
  } catch (const std::exception& e) {
    doc.status = std::string("parse error: ") + e.what();
    append_log("JSON parse failed for " + doc.path.string() + ": " + e.what());
  }
}

void DomAssetStudioApp::save_json_doc(const std::filesystem::path& path, const nlohmann::json& json, std::string& status, const char* kind) {
  try {
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const std::filesystem::path tempPath = path.string() + ".tmp";
    {
      std::ofstream out(tempPath);
      if (!out.good()) {
        status = "open failed";
        append_log(std::string("Failed to save ") + kind + ": " + path.string());
        return;
      }
      out << json.dump(2) << "\n";
      if (!out.good()) {
        status = "write failed";
        append_log(std::string("Failed to write ") + kind + ": " + tempPath.string());
        return;
      }
    }
    std::filesystem::rename(tempPath, path);
    status = "saved";
    append_log(std::string("Saved ") + kind + ": " + path.string());
  } catch (const std::exception& e) {
    status = std::string("save error: ") + e.what();
    append_log(std::string("Save failure for ") + kind + " " + path.string() + ": " + e.what());
  }
}

void DomAssetStudioApp::load_manifest(ManifestDoc& doc) {
  std::ifstream in(doc.path);
  if (!in.good()) {
    doc.status = "missing file";
    append_log("Missing manifest: " + doc.path.string());
    return;
  }
  try {
    in >> doc.json;
    doc.status = "loaded";
    doc.dirty = false;
  } catch (const std::exception& e) {
    doc.status = std::string("parse error: ") + e.what();
    append_log("Manifest parse failed for " + doc.path.string() + ": " + e.what());
  }
}

void DomAssetStudioApp::save_manifest(ManifestDoc& doc) {
  if (!doc.json.is_object()) {
    append_log("Skip save for invalid manifest: " + doc.path.string());
    return;
  }
  save_json_doc(doc.path, doc.json, doc.status, "manifest");
  doc.dirty = false;
}

void DomAssetStudioApp::save_stylesheet(StylesheetDoc& doc) {
  if (!doc.json.is_object()) {
    append_log("Skip save for invalid sheet: " + doc.path.string());
    return;
  }
  save_json_doc(doc.path, doc.json, doc.status, "stylesheet");
  doc.dirty = false;
  dom::render::reload_render_stylesheets();
  dom::render::load_render_stylesheets();
  update_preview_resolution();
}

void DomAssetStudioApp::apply_manifest_to_stylesheet() {
  if (selectedManifestAssetId_.empty()) {
    append_log("Apply mapping skipped: no selected manifest asset.");
    return;
  }
  auto& sheet = stylesheets_[selectedStylesheet_];
  if (!sheet.json.is_object()) return;
  if (!sheet.json.contains("exact_mappings") || !sheet.json["exact_mappings"].is_object()) sheet.json["exact_mappings"] = nlohmann::json::object();

  for (const auto& asset : assetManifest_.json.value("assets", nlohmann::json::array())) {
    if (!asset.is_object() || asset.value("asset_id", "") != selectedManifestAssetId_) continue;
    nlohmann::json& target = sheet.json["exact_mappings"][selectedManifestAssetId_];
    target["style_id"] = selectedManifestAssetId_;
    target["mesh"] = asset.value("asset_id", "");
    std::string lodGroup;
    if (asset.contains("lods") && asset["lods"].is_array() && !asset["lods"].empty() && asset["lods"][0].is_string()) {
      lodGroup = asset["lods"][0].get<std::string>();
    }
    target["lod_group"] = lodGroup;
    target["render_class"] = asset.value("render_class", "");
    target["category"] = asset.value("category", "");
    target["civ_tag"] = asset.value("civilization_theme", "neutral");
    target["theme_tag"] = asset.value("civilization_theme", "neutral");
    if (!target.contains("attachments") || !target["attachments"].is_object()) target["attachments"] = nlohmann::json::object();
    sheet.dirty = true;
    append_log("Applied manifest metadata into exact mapping: " + selectedManifestAssetId_);
    break;
  }
  update_preview_resolution();
}

void DomAssetStudioApp::run_content_validation() {
  int rc = std::system("python3 tools/validate_content_pipeline.py > /tmp/dom_asset_studio_validate.log 2>&1");
  std::ifstream in("/tmp/dom_asset_studio_validate.log");
  std::string line;
  validationMessages_.push_back("-- validate_content_pipeline.py --");
  while (std::getline(in, line)) {
    validationMessages_.push_back(line);
    if (validationMessages_.size() > 120) break;
  }
  append_log(rc == 0 ? "Validation passed." : "Validation reported errors.");
}

void DomAssetStudioApp::run_internal_validation() {
  validationMessages_.clear();
  std::set<std::string> assetIds;
  std::set<std::string> lodIds;
  std::set<std::string> duplicateAssets;
  std::set<std::string> duplicateLods;
  const std::set<std::string> validRenderClasses = {"terrain", "unit", "building", "object", "site", "city_cluster", "icon", "marker"};
  const std::set<std::string> validAttachmentKeys = {"banner_socket", "civ_emblem", "smoke_stack", "muzzle_flash", "selection_badge", "warning_badge", "guardian_aura"};

  if (assetManifest_.json.contains("assets") && assetManifest_.json["assets"].is_array()) {
    for (const auto& asset : assetManifest_.json["assets"]) {
      const std::string aid = asset.value("asset_id", "");
      if (aid.empty()) {
        validationMessages_.push_back("error: asset entry missing asset_id");
        continue;
      }
      if (!assetIds.insert(aid).second) duplicateAssets.insert(aid);
      const std::string mesh = asset.value("mesh", "");
      if (mesh.empty()) validationMessages_.push_back("warning: asset " + aid + " has empty mesh path");
      const std::string renderClass = asset.value("render_class", "");
      if (!renderClass.empty() && !validRenderClasses.contains(renderClass)) validationMessages_.push_back("error: asset " + aid + " has invalid render_class " + renderClass);
      const std::string civTag = asset.value("civ_tag", "");
      const std::string themeTag = asset.value("theme_tag", "");
      if (!civTag.empty() && (civTag.find(' ') != std::string::npos || civTag.find('/') != std::string::npos)) validationMessages_.push_back("error: asset " + aid + " has invalid civ_tag " + civTag);
      if (!themeTag.empty() && (themeTag.find(' ') != std::string::npos || themeTag.find('/') != std::string::npos)) validationMessages_.push_back("error: asset " + aid + " has invalid theme_tag " + themeTag);
    }
  }
  if (lodManifest_.json.contains("lod_entries") && lodManifest_.json["lod_entries"].is_array()) {
    for (const auto& lod : lodManifest_.json["lod_entries"]) {
      const std::string lid = lod.value("lod_id", "");
      const std::string aid = lod.value("asset_id", "");
      if (lid.empty()) {
        validationMessages_.push_back("error: lod entry missing lod_id");
        continue;
      }
      if (!lodIds.insert(lid).second) duplicateLods.insert(lid);
      if (!aid.empty() && !assetIds.contains(aid) && aid != "missing_mesh") {
        validationMessages_.push_back("error: lod " + lid + " references unknown asset_id " + aid);
      }
      for (const char* key : {"near_asset", "mid_asset", "far_asset", "fallback_asset"}) {
        const std::string ref = lod.value(key, "");
        if (!ref.empty() && !assetIds.contains(ref) && ref != "missing_mesh") validationMessages_.push_back(std::string("error: lod ") + lid + " has invalid " + key + " " + ref);
      }
      if (lod.contains("attachments") && lod["attachments"].is_object()) {
        for (const auto& [socket, _] : lod["attachments"].items()) {
          if (!validAttachmentKeys.contains(socket)) validationMessages_.push_back("error: lod " + lid + " has invalid attachment key " + socket);
        }
      }
    }
  }
  for (const auto& d : duplicateAssets) validationMessages_.push_back("error: duplicate asset id " + d);
  for (const auto& d : duplicateLods) validationMessages_.push_back("error: duplicate lod id " + d);

  auto check_sheet = [&](const StylesheetDoc& sheet) {
    if (!sheet.json.is_object()) return;
    std::function<void(const nlohmann::json&)> walk = [&](const nlohmann::json& node) {
      if (node.is_object()) {
        const std::string mesh = node.value("mesh", "");
        if (!mesh.empty() && !assetIds.contains(mesh) && mesh != "missing_mesh") {
          validationMessages_.push_back("warning: stylesheet mesh reference missing in asset_manifest: " + mesh);
        }
        const std::string lod = node.value("lod_group", "");
        if (!lod.empty() && !lodIds.contains(lod) && lod != "missing_mesh") {
          validationMessages_.push_back("warning: stylesheet lod_group reference missing in lod_manifest: " + lod);
        }
        for (const auto& [_, v] : node.items()) walk(v);
      } else if (node.is_array()) {
        for (const auto& v : node) walk(v);
      }
    };
    walk(sheet.json);
  };
  for (const auto& sheet : stylesheets_) check_sheet(sheet);
}

void DomAssetStudioApp::run_package_workflow() {
  int rc = std::system("python3 tools/asset_pipeline/package_assets.py > /tmp/dom_asset_studio_package.log 2>&1");
  std::ifstream in("/tmp/dom_asset_studio_package.log");
  std::string line;
  packageStatus_.clear();
  while (std::getline(in, line)) {
    packageStatus_ += line;
    packageStatus_ += "\n";
  }
  append_log(rc == 0 ? "Packaging complete." : "Packaging failed (see Validation panel). ");
}

void DomAssetStudioApp::apply_and_reload() {
  save_manifest(assetManifest_);
  save_manifest(lodManifest_);
  for (auto& sheet : stylesheets_) {
    if (sheet.dirty) save_stylesheet(sheet);
  }
  reload_content();
  append_log("Apply + reload completed.");
}

void DomAssetStudioApp::update_preview_resolution() {
  dom::render::RenderStyleRequest req{};
  req.domain = static_cast<dom::render::RenderStyleDomain>(std::clamp(previewDomain_, 0, 3));
  req.exactId = previewStyleVariant_ == 2 ? "" : selectedExactId_;
  req.civId = previewCiv_;
  req.themeId = previewTheme_;
  req.renderClass = previewStyleVariant_ == 1 ? "" : selectedRenderClass_;
  req.state = previewState_;
  req.lodTier = static_cast<dom::render::ContentLodTier>(std::clamp(previewLod_, 0, 2));
  resolvedPreview_ = dom::render::resolve_render_style(req);
  refresh_preview_asset_from_resolution();
}

void DomAssetStudioApp::refresh_preview_asset_from_resolution() {
  if (resolvedPreview_.mesh.empty()) {
    previewAsset_ = {};
    previewAsset_.error = "resolved style has no mesh";
    loadedAssetPath_.clear();
    return;
  }

  if (const auto* mesh = assets_.get_mesh(resolvedPreview_.mesh)) {
    if (!mesh->sourcePath.empty()) {
      open_asset_for_preview(mesh->sourcePath, true);
      return;
    }
  }

  open_asset_for_preview(resolvedPreview_.mesh, true);
}

void DomAssetStudioApp::open_asset_for_preview(const std::filesystem::path& requestedPath, bool fromResolver) {
  if (requestedPath.empty()) return;
  std::filesystem::path path = requestedPath;
  if (!path.is_absolute()) path = std::filesystem::path("content") / path;
  if (!std::filesystem::exists(path)) {
    previewAsset_ = {};
    previewAsset_.error = "file not found: " + path.string();
    if (!fromResolver) append_log("Cannot open asset: " + previewAsset_.error);
    return;
  }

  std::string error;
  auto loaded = load_preview_asset(path, error);
  if (!loaded) {
    previewAsset_ = {};
    previewAsset_.error = error;
    loadedAssetPath_ = path.string();
    append_log("Preview load failed: " + error);
    return;
  }

  previewAsset_ = *loaded;
  loadedAssetPath_ = path.string();
  if (!fromResolver) append_log("Loaded preview asset: " + loadedAssetPath_);
}

std::optional<DomAssetStudioApp::PreviewAsset> DomAssetStudioApp::load_preview_asset(const std::filesystem::path& path, std::string& error) const {
  PreviewAsset out{};
  out.sourcePath = path.string();

  std::vector<uint8_t> bytes;
  if (!read_file_bytes(path, bytes)) {
    error = "failed to read file bytes";
    return std::nullopt;
  }

  nlohmann::json j;
  std::vector<uint8_t> glbBinChunk;
  const auto ext = path.extension().string();
  if (ext == ".gltf") {
    out.sourceKind = "gltf";
    try {
      j = nlohmann::json::parse(bytes.begin(), bytes.end());
    } catch (const std::exception& e) {
      error = std::string("gltf parse error: ") + e.what();
      return std::nullopt;
    }
  } else if (ext == ".glb") {
    out.sourceKind = "glb";
    if (bytes.size() < 20) {
      error = "glb too small";
      return std::nullopt;
    }
    if (read_u32_le(bytes.data()) != 0x46546C67U) {
      error = "invalid glb magic";
      return std::nullopt;
    }
    size_t off = 12;
    bool gotJson = false;
    while (off + 8 <= bytes.size()) {
      const uint32_t chunkLen = read_u32_le(bytes.data() + off);
      const uint32_t chunkType = read_u32_le(bytes.data() + off + 4);
      off += 8;
      if (off + chunkLen > bytes.size()) break;
      if (chunkType == 0x4E4F534AU) {
        try {
          j = nlohmann::json::parse(bytes.begin() + static_cast<long>(off), bytes.begin() + static_cast<long>(off + chunkLen));
          gotJson = true;
        } catch (const std::exception& e) {
          error = std::string("glb json parse error: ") + e.what();
          return std::nullopt;
        }
      } else if (chunkType == 0x004E4942U) {
        glbBinChunk.assign(bytes.begin() + static_cast<long>(off), bytes.begin() + static_cast<long>(off + chunkLen));
      }
      off += chunkLen;
    }
    if (!gotJson) {
      error = "glb missing JSON chunk";
      return std::nullopt;
    }
  } else {
    error = "unsupported asset extension: " + ext;
    return std::nullopt;
  }

  std::vector<std::vector<uint8_t>> buffers;
  if (j.contains("buffers") && j["buffers"].is_array()) {
    buffers.resize(j["buffers"].size());
    for (size_t i = 0; i < j["buffers"].size(); ++i) {
      const auto& bj = j["buffers"][i];
      if (ext == ".glb" && i == 0 && !glbBinChunk.empty()) {
        buffers[i] = glbBinChunk;
        continue;
      }
      const std::string uri = bj.value("uri", "");
      if (uri.empty() || uri.rfind("data:", 0) == 0) {
        error = "embedded/data URI buffer unsupported for now";
        return std::nullopt;
      }
      std::filesystem::path bpath = path.parent_path() / uri;
      if (!read_file_bytes(bpath, buffers[i])) {
        error = "failed reading external buffer: " + bpath.string();
        return std::nullopt;
      }
    }
  }

  std::vector<BufferViewRef> views;
  if (j.contains("bufferViews") && j["bufferViews"].is_array()) {
    for (const auto& v : j["bufferViews"]) {
      BufferViewRef ref{};
      ref.buffer = v.value("buffer", -1);
      ref.byteOffset = v.value("byteOffset", 0);
      ref.byteLength = v.value("byteLength", 0);
      ref.byteStride = v.value("byteStride", 0);
      views.push_back(ref);
    }
  }

  auto load_accessor_positions = [&](int accessorIndex, std::vector<PreviewSurfacePoint>& points) -> bool {
    if (!j.contains("accessors") || !j["accessors"].is_array()) return false;
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(j["accessors"].size())) return false;
    const auto& acc = j["accessors"][accessorIndex];
    if (acc.value("type", "") != "VEC3") return false;
    if (acc.value("componentType", 0) != 5126) return false;
    const int viewIdx = acc.value("bufferView", -1);
    if (viewIdx < 0 || viewIdx >= static_cast<int>(views.size())) return false;
    const auto& view = views[viewIdx];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(buffers.size())) return false;
    const auto& b = buffers[view.buffer];
    const size_t count = acc.value("count", 0);
    const size_t accOffset = acc.value("byteOffset", 0);
    const size_t stride = view.byteStride > 0 ? view.byteStride : 12;
    size_t base = view.byteOffset + accOffset;
    if (base >= b.size()) return false;
    points.reserve(points.size() + count);
    for (size_t i = 0; i < count; ++i) {
      size_t p0 = base + i * stride;
      if (p0 + 12 > b.size()) break;
      const float* fp = reinterpret_cast<const float*>(b.data() + p0);
      points.push_back({fp[0], fp[1], fp[2]});
    }
    return !points.empty();
  };

  auto load_accessor_indices = [&](int accessorIndex, std::vector<uint32_t>& indices) -> bool {
    if (!j.contains("accessors") || !j["accessors"].is_array()) return false;
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(j["accessors"].size())) return false;
    const auto& acc = j["accessors"][accessorIndex];
    if (acc.value("type", "") != "SCALAR") return false;
    const int compType = acc.value("componentType", 0);
    const size_t csize = component_size(compType);
    if (csize == 0) return false;
    const int viewIdx = acc.value("bufferView", -1);
    if (viewIdx < 0 || viewIdx >= static_cast<int>(views.size())) return false;
    const auto& view = views[viewIdx];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(buffers.size())) return false;
    const auto& b = buffers[view.buffer];
    const size_t count = acc.value("count", 0);
    const size_t accOffset = acc.value("byteOffset", 0);
    const size_t stride = view.byteStride > 0 ? view.byteStride : csize;
    size_t base = view.byteOffset + accOffset;
    if (base >= b.size()) return false;
    indices.reserve(indices.size() + count);
    for (size_t i = 0; i < count; ++i) {
      size_t p0 = base + i * stride;
      if (p0 + csize > b.size()) break;
      uint32_t idx = 0;
      if (compType == 5121) idx = b[p0];
      else if (compType == 5123) idx = static_cast<uint16_t>(b[p0] | (static_cast<uint16_t>(b[p0 + 1]) << 8U));
      else if (compType == 5125) idx = read_u32_le(b.data() + p0);
      else return false;
      indices.push_back(idx);
    }
    return !indices.empty();
  };

  if (j.contains("materials") && j["materials"].is_array()) {
    for (const auto& m : j["materials"]) out.materialNames.push_back(m.value("name", "<unnamed_material>"));
  }

  if (!j.contains("meshes") || !j["meshes"].is_array()) {
    error = "asset has no meshes";
    return std::nullopt;
  }

  bool boundsInit = false;
  for (const auto& m : j["meshes"]) {
    out.meshNames.push_back(m.value("name", "<unnamed_mesh>"));
    if (!m.contains("primitives") || !m["primitives"].is_array()) continue;
    for (const auto& prim : m["primitives"]) {
      if (prim.value("mode", 4) != 4) continue;
      if (!prim.contains("attributes") || !prim["attributes"].is_object()) continue;
      int posAccessor = prim["attributes"].value("POSITION", -1);
      if (posAccessor < 0) continue;
      PreviewSurface surface{};
      const int materialIdx = prim.value("material", -1);
      if (materialIdx >= 0 && materialIdx < static_cast<int>(out.materialNames.size())) surface.materialName = out.materialNames[materialIdx];
      if (!load_accessor_positions(posAccessor, surface.points)) continue;
      if (prim.contains("indices")) {
        if (!load_accessor_indices(prim["indices"].get<int>(), surface.indices)) {
          surface.indices.clear();
        }
      }
      if (surface.indices.empty()) {
        for (uint32_t i = 0; i + 2 < static_cast<uint32_t>(surface.points.size()); i += 3) {
          surface.indices.push_back(i);
          surface.indices.push_back(i + 1);
          surface.indices.push_back(i + 2);
        }
      }

      for (const auto& p : surface.points) {
        if (!boundsInit) {
          out.minBounds = {p.x, p.y, p.z};
          out.maxBounds = {p.x, p.y, p.z};
          boundsInit = true;
        } else {
          out.minBounds[0] = std::min(out.minBounds[0], p.x);
          out.minBounds[1] = std::min(out.minBounds[1], p.y);
          out.minBounds[2] = std::min(out.minBounds[2], p.z);
          out.maxBounds[0] = std::max(out.maxBounds[0], p.x);
          out.maxBounds[1] = std::max(out.maxBounds[1], p.y);
          out.maxBounds[2] = std::max(out.maxBounds[2], p.z);
        }
      }

      out.vertexCount += surface.points.size();
      out.indexCount += surface.indices.size();
      out.surfaces.push_back(std::move(surface));
    }
  }

  if (out.surfaces.empty()) {
    error = "mesh primitives unavailable (expects TRIANGLES + POSITION accessor)";
    return std::nullopt;
  }

  out.loaded = true;
  return out;
}

} // namespace dom::tools
