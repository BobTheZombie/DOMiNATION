#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/vec2.hpp>

namespace dom::sim {

enum class Resource : uint8_t { Food, Wood, Metal, Wealth, Knowledge, Oil, Count };
enum class Age : uint8_t { Ancient, Classical, Medieval, Gunpowder, Enlightenment, Industrial, Modern, Information };

struct Unit {
  uint32_t id{};
  uint16_t team{};
  float hp{100.0f};
  float attack{8.0f};
  float range{2.5f};
  float speed{4.0f};
  glm::vec2 pos{};
  glm::vec2 renderPos{};
  glm::vec2 target{};
  uint32_t targetUnit{0};
  bool selected{false};
};

struct City {
  uint32_t id{};
  uint16_t team{};
  glm::vec2 pos{};
  int level{1};
  bool capital{false};
};

struct PlayerState {
  uint16_t id{};
  Age age{Age::Ancient};
  std::array<float, static_cast<size_t>(Resource::Count)> resources{400, 350, 250, 250, 100, 0};
  int score{0};
  bool alive{true};
};

struct World {
  int width{128};
  int height{128};
  std::vector<float> heightmap;
  std::vector<float> fertility;
  std::vector<uint16_t> territoryOwner;
  std::vector<uint8_t> fog;
  std::vector<Unit> units;
  std::vector<City> cities;
  std::vector<PlayerState> players;
  bool godMode{false};
  uint32_t tick{0};
  bool gameOver{false};
  uint16_t winner{0};

  uint32_t territoryRecomputeCount{0};
  uint32_t aiDecisionCount{0};
  bool territoryDirty{true};
  bool fogDirty{true};
};

void initialize_world(World& world, uint32_t seed);
void tick_world(World& world, float dt);
void issue_move(World& world, uint16_t team, const std::vector<uint32_t>& ids, glm::vec2 target);
void issue_attack(World& world, uint16_t team, const std::vector<uint32_t>& ids, uint32_t enemy);
void toggle_god_mode(World& world);

uint64_t map_setup_hash(const World& world);
uint64_t state_hash(const World& world);

} // namespace dom::sim
