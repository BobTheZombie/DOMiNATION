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
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace dom::tools {

namespace {
const char* kDomainNames[] = {"Terrain", "Unit", "Building", "Object"};
const char* kLodNames[] = {"Near", "Mid", "Far"};
}

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
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Asset")) {
    ImGui::InputText("Open glTF/GLB", &gltfOpenPath_);
    if (ImGui::MenuItem("Inspect Path")) {
      append_log("Inspect asset path: " + gltfOpenPath_);
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Stylesheet")) {
    if (ImGui::MenuItem("Save Current Stylesheet")) save_stylesheet(stylesheets_[selectedStylesheet_]);
    if (ImGui::MenuItem("Save All Stylesheets")) for (auto& s : stylesheets_) save_stylesheet(s);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Export")) {
    if (ImGui::MenuItem("Export Engine-Compatible Content (save + validate)")) {
      for (auto& s : stylesheets_) save_stylesheet(s);
      run_content_validation();
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Validate")) {
    if (ImGui::MenuItem("Run Content Validation")) run_content_validation();
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
    if (ImGui::Selectable(rel.c_str())) append_log("Selected file: " + rel);
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

  if (ImGui::CollapsingHeader("Asset Manifest / LOD", ImGuiTreeNodeFlags_DefaultOpen)) {
    int shown = 0;
    for (const auto& [id, rec] : assets_.registry().assets()) {
      if (shown++ > 30) break;
      if (ImGui::TreeNode(id.c_str())) {
        ImGui::Text("mesh: %s", rec.meshPath.c_str());
        ImGui::Text("type: %s", rec.type.c_str());
        ImGui::Text("civ/theme: %s", rec.civilizationTheme.c_str());
        for (const auto& lod : rec.lodIds) ImGui::BulletText("lod: %s", lod.c_str());
        ImGui::TreePop();
      }
    }
  }

  ImGui::End();
#endif
}

void DomAssetStudioApp::edit_style_layer(nlohmann::json& layer, const char* idPrefix) {
#ifdef DOM_HAS_IMGUI
  std::string mesh = layer.value("mesh", "");
  std::string material = layer.value("material", "");
  std::string lodGroup = layer.value("lod_group", "");
  ImGui::InputText((std::string("Mesh##") + idPrefix).c_str(), &mesh);
  ImGui::InputText((std::string("Material##") + idPrefix).c_str(), &material);
  ImGui::InputText((std::string("LOD Group##") + idPrefix).c_str(), &lodGroup);
  layer["mesh"] = mesh;
  layer["material"] = material;
  layer["lod_group"] = lodGroup;

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
  ImGui::Text("Status: %s", sheet.status.c_str());

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

  ImGui::Combo("Preview Domain", &previewDomain_, kDomainNames, 4);
  ImGui::InputText("Preview Civ", &previewCiv_);
  ImGui::InputText("Preview Theme", &previewTheme_);
  ImGui::InputText("Preview State", &previewState_);
  ImGui::Combo("Preview LOD", &previewLod_, kLodNames, 3);

  if (sheet.json.contains("render_classes") && sheet.json["render_classes"].contains(selectedRenderClass_)) {
    auto& rc = sheet.json["render_classes"][selectedRenderClass_];
    if (!rc.contains("default") || !rc["default"].is_object()) rc["default"] = nlohmann::json::object();
    ImGui::SeparatorText("Default Mapping");
    edit_style_layer(rc["default"], "rc_default");

    if (!rc.contains("civ_overrides") || !rc["civ_overrides"].is_object()) rc["civ_overrides"] = nlohmann::json::object();
    if (ImGui::TreeNode("Civ Overrides")) {
      for (auto& [civ, layer] : rc["civ_overrides"].items()) {
        if (!layer.is_object()) continue;
        if (ImGui::TreeNode(civ.c_str())) {
          edit_style_layer(layer, civ.c_str());
          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }

    if (!rc.contains("theme_overrides") || !rc["theme_overrides"].is_object()) rc["theme_overrides"] = nlohmann::json::object();
    if (ImGui::TreeNode("Theme Overrides")) {
      for (auto& [theme, layer] : rc["theme_overrides"].items()) {
        if (!layer.is_object()) continue;
        if (ImGui::TreeNode(theme.c_str())) {
          edit_style_layer(layer, theme.c_str());
          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }

  }

  if (ImGui::Button("Save Sheet")) save_stylesheet(sheet);
  ImGui::SameLine();
  if (ImGui::Button("Re-resolve Preview")) update_preview_resolution();

  if (ImGui::TreeNode("Raw JSON")) {
    std::string raw = sheet.json.dump(2);
    ImGui::InputTextMultiline("##raw", &raw, ImVec2(-1, 240));
    ImGui::TreePop();
  }

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

  ImGui::Text("Real-time model preview shell");
  ImGui::SliderFloat("Orbit Yaw", &orbitYaw_, -180.0f, 180.0f);
  ImGui::SliderFloat("Orbit Pitch", &orbitPitch_, -89.0f, 89.0f);
  ImGui::SliderFloat("Zoom", &orbitDistance_, 1.0f, 20.0f);
  ImGui::SliderFloat("Pan X", &panX_, -10.0f, 10.0f);
  ImGui::SliderFloat("Pan Y", &panY_, -10.0f, 10.0f);
  ImGui::Checkbox("Grid", &showGrid_);
  ImGui::Checkbox("Wireframe", &wireframe_);
  ImGui::Checkbox("Normals", &showNormals_);
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

  float cx = p.x + size.x * (0.5f + panX_ * 0.03f);
  float cy = p.y + size.y * (0.55f + panY_ * 0.03f);
  float radius = std::max(20.0f, 160.0f / orbitDistance_);
  ImU32 color = wireframe_ ? IM_COL32(255, 255, 120, 255) : IM_COL32(120, 210, 255, 220);
  draw->AddCircle(ImVec2(cx, cy), radius, color, 24, wireframe_ ? 2.5f : 0.0f);
  if (showNormals_) {
    for (int i = 0; i < 12; ++i) {
      float a = (i / 12.0f) * 6.28318f;
      draw->AddLine(ImVec2(cx + std::cos(a) * radius, cy + std::sin(a) * radius),
                    ImVec2(cx + std::cos(a) * (radius + 14.0f), cy + std::sin(a) * (radius + 14.0f)), IM_COL32(255, 100, 100, 255), 1.0f);
    }
  }

  draw->AddText(ImVec2(p.x + 8, p.y + 8), IM_COL32_WHITE, ("mesh: " + resolvedPreview_.mesh).c_str());
  draw->AddText(ImVec2(p.x + 8, p.y + 26), IM_COL32_WHITE, ("material: " + resolvedPreview_.material).c_str());

  if (ImGui::Button("Reset View")) {
    orbitYaw_ = 25.0f; orbitPitch_ = 20.0f; orbitDistance_ = 8.0f; panX_ = panY_ = 0.0f;
  }

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
  if (ImGui::Button("Run Validation")) run_content_validation();
  for (const auto& m : validationMessages_) ImGui::TextUnformatted(m.c_str());
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

void DomAssetStudioApp::save_stylesheet(StylesheetDoc& doc) {
  if (!doc.json.is_object()) {
    append_log("Skip save for invalid sheet: " + doc.path.string());
    return;
  }
  std::ofstream out(doc.path);
  out << doc.json.dump(2) << "\n";
  doc.dirty = false;
  doc.status = "saved";
  append_log("Saved stylesheet: " + doc.path.string());
  dom::render::reload_render_stylesheets();
  dom::render::load_render_stylesheets();
  update_preview_resolution();
}

void DomAssetStudioApp::run_content_validation() {
  validationMessages_.clear();
  int rc = std::system("python3 tools/validate_content_pipeline.py > /tmp/dom_asset_studio_validate.log 2>&1");
  std::ifstream in("/tmp/dom_asset_studio_validate.log");
  std::string line;
  while (std::getline(in, line)) {
    validationMessages_.push_back(line);
    if (validationMessages_.size() > 120) break;
  }
  append_log(rc == 0 ? "Validation passed." : "Validation reported errors.");
}

void DomAssetStudioApp::update_preview_resolution() {
  dom::render::RenderStyleRequest req{};
  req.domain = static_cast<dom::render::RenderStyleDomain>(std::clamp(previewDomain_, 0, 3));
  req.exactId = selectedExactId_;
  req.civId = previewCiv_;
  req.themeId = previewTheme_;
  req.renderClass = selectedRenderClass_;
  req.state = previewState_;
  req.lodTier = static_cast<dom::render::ContentLodTier>(std::clamp(previewLod_, 0, 2));
  resolvedPreview_ = dom::render::resolve_render_style(req);
}

} // namespace dom::tools
