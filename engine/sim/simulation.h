#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/vec2.hpp>

namespace dom::sim {

enum class Resource : uint8_t { Food, Wood, Metal, Wealth, Knowledge, Oil, Count };
enum class Age : uint8_t { Ancient, Classical, Medieval, Gunpowder, Enlightenment, Industrial, Modern, Information };
enum class UnitType : uint8_t { Worker, Infantry, Archer, Cavalry, Siege };
enum class BuildingType : uint8_t { CityCenter, House, Farm, LumberCamp, Mine, Market, Library, Barracks, Wonder };
enum class UnitRole : uint8_t { Infantry, Ranged, Cavalry, Siege, Worker, Building };
enum class AttackType : uint8_t { Melee, Ranged };
enum class MatchPhase : uint8_t { Running, Ended, Postmatch };
enum class VictoryCondition : uint8_t { None, Conquest, Score, Wonder };
enum class ResourceNodeType : uint8_t { Forest, Ore, Farmable, Ruins };
enum class ObjectiveState : uint8_t { Inactive, Active, Completed, Failed };
enum class TriggerType : uint8_t { TickReached, EntityDestroyed, BuildingCompleted, AreaEntered, PlayerEliminated };
enum class TriggerActionType : uint8_t { ShowObjectiveText, SetObjectiveState, GrantResources, SpawnUnits, EndMatchWithVictory, EndMatchWithDefeat, RevealArea };

enum class ReplayCommandType : uint8_t { Move, Attack, AttackMove, PlaceBuilding, QueueTrain, QueueResearch, CancelQueue };

struct ReplayCommand {
  ReplayCommandType type{ReplayCommandType::Move};
  uint32_t tick{0};
  uint16_t team{0};
  std::vector<uint32_t> ids;
  glm::vec2 target{};
  uint32_t enemy{0};
  uint32_t buildingId{0};
  UnitType unitType{UnitType::Worker};
  BuildingType buildingType{BuildingType::House};
  size_t queueIndex{0};
};

enum class QueueKind : uint8_t { TrainUnit, AgeResearch };

struct ProductionItem {
  QueueKind kind{QueueKind::TrainUnit};
  UnitType unitType{UnitType::Worker};
  float remaining{0.0f};
  int targetAge{0};
};

struct Unit { uint32_t id{}; uint16_t team{}; UnitType type{UnitType::Infantry}; float hp{100.0f}; float attack{8.0f}; float range{2.5f}; float speed{4.0f}; UnitRole role{UnitRole::Infantry}; AttackType attackType{AttackType::Melee}; UnitRole preferredTargetRole{UnitRole::Infantry}; std::array<uint16_t, 6> vsRoleMultiplierPermille{1000, 1000, 1000, 1000, 1000, 1000}; glm::vec2 pos{}; glm::vec2 renderPos{}; glm::vec2 target{}; glm::vec2 slotTarget{}; glm::vec2 moveDir{}; uint32_t targetUnit{}; uint32_t moveOrder{}; uint32_t attackMoveOrder{}; uint16_t targetLockTicks{}; uint16_t chaseTicks{}; uint16_t attackCooldownTicks{}; uint16_t lastTargetSwitchTick{}; uint16_t stuckTicks{}; uint8_t orderPathLingerTicks{}; bool hasMoveOrder{false}; bool attackMove{false}; bool selected{false}; };

struct City { uint32_t id{}; uint16_t team{}; glm::vec2 pos{}; int level{1}; bool capital{false}; };

struct Building { uint32_t id{}; uint16_t team{}; BuildingType type{BuildingType::House}; glm::vec2 pos{}; glm::vec2 size{2.0f, 2.0f}; bool underConstruction{true}; float buildProgress{0.0f}; float buildTime{10.0f}; float hp{1000.0f}; float maxHp{1000.0f}; std::vector<ProductionItem> queue; };

struct ResourceNode { uint32_t id{}; ResourceNodeType type{ResourceNodeType::Forest}; glm::vec2 pos{}; float amount{1000.0f}; uint16_t owner{UINT16_MAX}; };
struct TriggerArea { uint32_t id{}; glm::vec2 min{}; glm::vec2 max{}; };
struct Objective { uint32_t id{}; std::string title; std::string text; bool primary{true}; ObjectiveState state{ObjectiveState::Inactive}; uint16_t owner{UINT16_MAX}; };
struct TriggerCondition { TriggerType type{TriggerType::TickReached}; uint32_t tick{0}; uint32_t entityId{0}; BuildingType buildingType{BuildingType::House}; uint32_t areaId{0}; uint16_t player{UINT16_MAX}; };
struct TriggerAction { TriggerActionType type{TriggerActionType::ShowObjectiveText}; std::string text; uint32_t objectiveId{0}; ObjectiveState objectiveState{ObjectiveState::Active}; uint16_t player{UINT16_MAX}; std::array<float, static_cast<size_t>(Resource::Count)> resources{}; UnitType spawnUnitType{UnitType::Infantry}; uint32_t spawnCount{0}; glm::vec2 spawnPos{}; uint16_t winner{0}; uint32_t areaId{0}; };
struct Trigger { uint32_t id{}; bool once{true}; bool fired{false}; TriggerCondition condition{}; std::vector<TriggerAction> actions; };
struct ObjectiveLogEntry { uint32_t tick{0}; std::string text; };

struct CivilizationRuntime {
  std::string id{"default"};
  float economyBias{1.0f};
  float militaryBias{1.0f};
  float scienceBias{1.0f};
  float aggression{1.0f};
  float defense{1.0f};
};

struct PlayerState { uint16_t id{}; Age age{Age::Ancient}; std::array<float, static_cast<size_t>(Resource::Count)> resources{400, 350, 250, 250, 100, 0}; int popUsed{0}; int popCap{10}; int score{0}; bool alive{true}; uint32_t unitsLost{0}; uint32_t buildingsLost{0}; int finalScore{0}; bool isHuman{false}; bool isCPU{true}; uint16_t teamId{0}; std::array<float, 3> color{0.8f, 0.8f, 0.8f}; CivilizationRuntime civilization{}; };
struct MatchResult { MatchPhase phase{MatchPhase::Running}; VictoryCondition condition{VictoryCondition::None}; uint16_t winner{0}; uint32_t endTick{0}; bool scoreTieBreak{false}; };
struct WonderState { uint16_t owner{UINT16_MAX}; uint32_t heldTicks{0}; };

struct MatchConfig {
  uint32_t timeLimitTicks{20 * 600}; uint32_t wonderHoldTicks{20 * 90};
  int scoreResourceWeight{1}; int scoreUnitWeight{30}; int scoreBuildingWeight{70}; int scoreAgeWeight{120}; int scoreCapitalWeight{220};
  bool allowConquest{true}; bool allowScore{true}; bool allowWonder{true};
};

struct TickProfile {
  double navMs{0.0};
  double combatMs{0.0};
};

struct World {
  uint32_t seed{1337}; int width{128}; int height{128};
  std::vector<float> heightmap; std::vector<float> fertility; std::vector<uint16_t> territoryOwner; std::vector<uint8_t> fog;
  std::vector<Unit> units; std::vector<City> cities; std::vector<Building> buildings; std::vector<ResourceNode> resourceNodes;
  std::vector<TriggerArea> triggerAreas; std::vector<Objective> objectives; std::vector<Trigger> triggers; std::vector<ObjectiveLogEntry> objectiveLog;
  std::vector<PlayerState> players;
  bool godMode{false}; uint32_t tick{0}; bool gameOver{false}; uint16_t winner{0}; MatchConfig config{}; MatchResult match{}; WonderState wonder{};
  bool uiBuildMenu{false}; bool uiTrainMenu{false}; bool uiResearchMenu{false}; bool placementActive{false}; BuildingType placementType{BuildingType::House}; glm::vec2 placementPos{}; bool placementValid{false};
  uint32_t territoryRecomputeCount{0}; uint32_t aiDecisionCount{0}; uint32_t completedBuildingsCount{0}; uint32_t trainedUnitsViaQueue{0}; uint32_t researchStartedCount{0}; uint32_t navVersion{1}; uint32_t flowFieldGeneratedCount{0}; uint32_t flowFieldCacheHitCount{0}; uint32_t groupMoveCommandCount{0}; uint32_t unitsReachedSlotCount{0}; uint32_t stuckMoveAssertions{0}; uint32_t totalDamageDealtPermille{0}; uint32_t combatEngagementCount{0}; uint32_t targetSwitchCount{0}; uint32_t chaseLimitBreakCount{0}; uint32_t buildingDamageEvents{0}; uint32_t unitDeathEvents{0}; uint32_t aiRetreatCount{0}; uint32_t focusFireEvents{0}; uint32_t combatTicks{0}; uint32_t rejectedCommandCount{0};
  uint32_t triggerExecutionCount{0}; uint32_t objectiveStateChangeCount{0};
  bool territoryDirty{true}; bool fogDirty{true};
};

void initialize_world(World& world, uint32_t seed);
bool load_scenario_file(World& world, const std::string& path, uint32_t fallbackSeed, std::string& err);
bool save_scenario_file(const std::string& path, const World& world, std::string& err);
void on_authoritative_state_loaded(World& world);
void tick_world(World& world, float dt);
bool gameplay_orders_allowed(const World& world);
void set_match_phase(World& world, MatchPhase phase);
int compute_player_score(const World& world, uint16_t playerId);
void consume_replay_commands(std::vector<ReplayCommand>& out);
void issue_move(World& world, uint16_t team, const std::vector<uint32_t>& ids, glm::vec2 target);
void issue_attack(World& world, uint16_t team, const std::vector<uint32_t>& ids, uint32_t enemy);
void issue_attack_move(World& world, uint16_t team, const std::vector<uint32_t>& ids, glm::vec2 target);
void toggle_god_mode(World& world);
void set_nav_debug(bool enabled);
bool nav_debug_enabled();
void set_combat_debug(bool enabled);
bool combat_debug_enabled();

bool start_build_placement(World& world, uint16_t team, BuildingType type);
void update_build_placement(World& world, uint16_t team, glm::vec2 worldPos);
bool confirm_build_placement(World& world, uint16_t team);
void cancel_build_placement(World& world);

bool enqueue_train_unit(World& world, uint16_t team, uint32_t buildingId, UnitType type);
bool enqueue_age_research(World& world, uint16_t team, uint32_t buildingId);
bool cancel_queue_item(World& world, uint16_t team, uint32_t buildingId, size_t index);
bool players_allied(const World& world, uint16_t a, uint16_t b);

uint64_t map_setup_hash(const World& world);
uint64_t state_hash(const World& world);
TickProfile last_tick_profile();

} // namespace dom::sim
