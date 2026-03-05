#include "engine/sim/simulation.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <glm/geometric.hpp>
#include <glm/common.hpp>

namespace dom::sim {
namespace {
float dist(glm::vec2 a, glm::vec2 b) { return glm::length(a - b); }

constexpr uint64_t kFNVOffset = 1469598103934665603ull;
constexpr uint64_t kFNVPrime = 1099511628211ull;

void hash_u32(uint64_t& h, uint32_t v) {
  h ^= static_cast<uint64_t>(v);
  h *= kFNVPrime;
}

void hash_float(uint64_t& h, float v) {
  hash_u32(h, static_cast<uint32_t>(v * 1000.0f));
}

void recompute_territory(World& w) {
  std::fill(w.territoryOwner.begin(), w.territoryOwner.end(), 0);
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      float best = 1e9f;
      uint16_t owner = 0;
      glm::vec2 p{x + 0.5f, y + 0.5f};
      for (const auto& c : w.cities) {
        float d = dist(c.pos, p) / (c.capital ? 1.4f : 1.0f);
        if (d < best && d < 20.0f) {
          best = d;
          owner = c.team;
        }
      }
      w.territoryOwner[y * w.width + x] = owner;
    }
  }
  ++w.territoryRecomputeCount;
  w.territoryDirty = true;
}

void recompute_fog(World& w) {
  std::fill(w.fog.begin(), w.fog.end(), 0);
  for (const auto& c : w.cities) {
    if (c.team != 0) continue;
    for (int y = std::max(0, static_cast<int>(c.pos.y) - 10); y <= std::min(w.height - 1, static_cast<int>(c.pos.y) + 10); ++y) {
      for (int x = std::max(0, static_cast<int>(c.pos.x) - 10); x <= std::min(w.width - 1, static_cast<int>(c.pos.x) + 10); ++x) {
        if (dist({x + 0.5f, y + 0.5f}, c.pos) < 10.5f) w.fog[y * w.width + x] = 255;
      }
    }
  }
  for (const auto& u : w.units) {
    if (u.team != 0 || u.hp <= 0) continue;
    for (int y = std::max(0, static_cast<int>(u.pos.y) - 7); y <= std::min(w.height - 1, static_cast<int>(u.pos.y) + 7); ++y) {
      for (int x = std::max(0, static_cast<int>(u.pos.x) - 7); x <= std::min(w.width - 1, static_cast<int>(u.pos.x) + 7); ++x) {
        if (dist({x + 0.5f, y + 0.5f}, u.pos) < 7.5f) w.fog[y * w.width + x] = 255;
      }
    }
  }
  w.fogDirty = true;
}

} // namespace

void initialize_world(World& w, uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> n(0.f, 1.f);
  w.heightmap.resize(w.width * w.height);
  w.fertility.resize(w.width * w.height);
  w.territoryOwner.resize(w.width * w.height);
  w.fog.assign(w.width * w.height, 0);
  for (int y = 0; y < w.height; ++y) {
    for (int x = 0; x < w.width; ++x) {
      float h = 0.4f * std::sin(x * 0.08f) + 0.4f * std::cos(y * 0.09f) + 0.2f * n(rng);
      w.heightmap[y * w.width + x] = h;
      w.fertility[y * w.width + x] = std::clamp(1.0f - std::abs(h), 0.1f, 1.0f);
    }
  }

  w.players = {{0, Age::Ancient}, {1, Age::Ancient}};
  w.cities = {{1, 0, {20, 20}, 1, true}, {2, 1, {95, 95}, 1, true}};
  uint32_t id = 1;
  for (int i = 0; i < 20; ++i) {
    w.units.push_back({id++, 0, 100, 9, 3.0f, 5.0f, {18.0f + i * 0.6f, 24.0f}, {18.0f + i * 0.6f, 24.0f}, {18.0f + i * 0.6f, 24.0f}, 0, false});
    w.units.push_back({id++, 1, 100, 9, 3.0f, 5.0f, {92.0f + i * 0.6f, 89.0f}, {92.0f + i * 0.6f, 89.0f}, {92.0f + i * 0.6f, 89.0f}, 0, false});
  }
  recompute_territory(w);
  recompute_fog(w);
}

void tick_world(World& w, float dt) {
  ++w.tick;
  if (w.tick % 10 == 0) recompute_territory(w);

  for (auto& p : w.players) {
    p.resources[(size_t)Resource::Food] += 2.8f * dt * 20;
    p.resources[(size_t)Resource::Wealth] += 1.4f * dt * 20;
    p.resources[(size_t)Resource::Knowledge] += 1.0f * dt * 20;
    if (p.age < Age::Information && p.resources[(size_t)Resource::Knowledge] > 200 + 150 * (int)p.age
        && p.resources[(size_t)Resource::Wealth] > 220 + 180 * (int)p.age) {
      p.age = static_cast<Age>((int)p.age + 1);
      p.score += 300;
      if (p.age >= Age::Industrial) p.resources[(size_t)Resource::Oil] += 20;
    }
  }

  for (auto& u : w.units) {
    if (u.hp <= 0) continue;
    if (u.targetUnit != 0) {
      auto it = std::find_if(w.units.begin(), w.units.end(), [&](const Unit& e) { return e.id == u.targetUnit && e.hp > 0; });
      if (it != w.units.end()) u.target = it->pos;
    }

    glm::vec2 d = u.target - u.pos;
    float len = glm::length(d);
    if (len > 0.1f) {
      u.pos += (d / len) * u.speed * dt;
    }

    for (auto& e : w.units) {
      if (e.team == u.team || e.hp <= 0) continue;
      float dd = dist(u.pos, e.pos);
      if (dd <= u.range) {
        float mult = 1.0f;
        int tx = std::clamp((int)u.pos.x, 0, w.width - 1);
        int ty = std::clamp((int)u.pos.y, 0, w.height - 1);
        if (!w.godMode && w.territoryOwner[ty * w.width + tx] == e.team) mult = 0.85f;
        e.hp -= u.attack * mult * dt;
      }
    }

    u.renderPos = glm::mix(u.renderPos, u.pos, 0.35f);
  }

  w.units.erase(std::remove_if(w.units.begin(), w.units.end(), [](const Unit& u) { return u.hp <= 0; }), w.units.end());
  recompute_fog(w);

  bool team0Capital = false, team1Capital = false;
  for (const auto& c : w.cities) {
    if (c.capital && c.team == 0) team0Capital = true;
    if (c.capital && c.team == 1) team1Capital = true;
  }
  if (!team0Capital || !team1Capital) {
    w.gameOver = true;
    w.winner = team0Capital ? 0 : 1;
  }
  if (w.tick > 20 * 600) {
    w.gameOver = true;
    w.winner = w.players[0].score >= w.players[1].score ? 0 : 1;
  }
}

void issue_move(World& w, uint16_t team, const std::vector<uint32_t>& ids, glm::vec2 target) {
  for (auto& u : w.units) {
    if (u.team != team) continue;
    if (std::find(ids.begin(), ids.end(), u.id) != ids.end()) {
      u.target = target;
      u.targetUnit = 0;
    }
  }
}

void issue_attack(World& w, uint16_t team, const std::vector<uint32_t>& ids, uint32_t enemy) {
  for (auto& u : w.units) {
    if (u.team != team) continue;
    if (std::find(ids.begin(), ids.end(), u.id) != ids.end()) u.targetUnit = enemy;
  }
}

void toggle_god_mode(World& w) {
  w.godMode = !w.godMode;
  w.fogDirty = true;
}

uint64_t map_setup_hash(const World& w) {
  uint64_t h = kFNVOffset;
  for (float v : w.heightmap) hash_float(h, v);
  for (float v : w.fertility) hash_float(h, v);
  for (const auto& c : w.cities) {
    hash_u32(h, c.id);
    hash_u32(h, c.team);
    hash_float(h, c.pos.x);
    hash_float(h, c.pos.y);
  }
  for (const auto& u : w.units) {
    hash_u32(h, u.id);
    hash_u32(h, u.team);
    hash_float(h, u.pos.x);
    hash_float(h, u.pos.y);
  }
  return h;
}

uint64_t state_hash(const World& w) {
  uint64_t h = kFNVOffset;
  hash_u32(h, w.tick);
  hash_u32(h, static_cast<uint32_t>(w.units.size()));
  for (const auto& u : w.units) {
    hash_u32(h, u.id);
    hash_float(h, u.hp);
    hash_float(h, u.pos.x);
    hash_float(h, u.pos.y);
  }
  hash_u32(h, w.territoryRecomputeCount);
  hash_u32(h, w.aiDecisionCount);
  for (uint16_t owner : w.territoryOwner) hash_u32(h, owner);
  return h;
}

} // namespace dom::sim
