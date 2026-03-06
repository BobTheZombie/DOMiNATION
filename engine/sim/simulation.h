#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <glm/vec2.hpp>

namespace dom::sim {

enum class Resource : uint8_t { Food, Wood, Metal, Wealth, Knowledge, Oil, Count };
enum class Age : uint8_t { Ancient, Classical, Medieval, Gunpowder, Enlightenment, Industrial, Modern, Information };
enum class UnitType : uint8_t { Worker, Infantry, Archer, Cavalry, Siege, TransportShip, LightWarship, HeavyWarship, BombardShip, Count };
enum class BuildingType : uint8_t { CityCenter, House, Farm, LumberCamp, Mine, Market, Library, Barracks, Wonder, Port, Count };
enum class UnitRole : uint8_t { Infantry, Ranged, Cavalry, Siege, Worker, Building, Naval, Transport, Count };
enum class AttackType : uint8_t { Melee, Ranged };
enum class MatchPhase : uint8_t { Running, Ended, Postmatch };
enum class VictoryCondition : uint8_t { None, Conquest, Score, Wonder };
enum class ResourceNodeType : uint8_t { Forest, Ore, Farmable, Ruins };
enum class ObjectiveState : uint8_t { Inactive, Active, Completed, Failed };
enum class TriggerType : uint8_t { TickReached, EntityDestroyed, BuildingCompleted, AreaEntered, PlayerEliminated };
enum class TriggerActionType : uint8_t { ShowObjectiveText, SetObjectiveState, GrantResources, SpawnUnits, EndMatchWithVictory, EndMatchWithDefeat, RevealArea };

enum class ReplayCommandType : uint8_t { Move, Attack, AttackMove, PlaceBuilding, QueueTrain, QueueResearch, CancelQueue };
enum class GameplayEventType : uint8_t {
  UnitDied,
  BuildingCompleted,
  CityCaptured,
  WonderStarted,
  WonderCompleted,
  PlayerEliminated,
  ObjectiveCompleted,
  WarDeclared,
  AllianceFormed,
  AllianceBroken,
  TradeAgreementCreated,
  TradeAgreementBroken,
  EspionageSuccess,
  EspionageFailure,
  PostureChanged,
};

enum class DiplomacyRelation : uint8_t { Allied, Neutral, War, Ceasefire };
enum class EspionageOpType : uint8_t { ReconCity, RevealRoute, SabotageEconomy, SabotageSupply, CounterIntel };
enum class EspionageOpState : uint8_t { Active, Completed, Failed };
enum class StrategicPosture : uint8_t { Expansionist, Defensive, TradeFocused, Escalating, TotalWar };

enum class SupplyState : uint8_t { InSupply, LowSupply, OutOfSupply };
enum class OperationType : uint8_t { AssaultCity, DefendBorder, SecureRoute, RaidEconomy, RallyAndPush, AmphibiousAssault, NavalPatrol, CoastalBombard };
enum class TerrainClass : uint8_t { Land, ShallowWater, DeepWater };
enum class BiomeType : uint8_t { TemperateGrassland, Steppe, Forest, Desert, Mediterranean, Jungle, Tundra, Arctic, Coast, Wetlands, Mountain, Count };

struct GameplayEvent {
  GameplayEventType type{GameplayEventType::UnitDied};
  uint32_t tick{0};
  uint16_t actor{UINT16_MAX};
  uint16_t subject{UINT16_MAX};
  uint32_t entityId{0};
  std::string text;
};

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

struct Unit { uint32_t id{}; uint16_t team{}; UnitType type{UnitType::Infantry}; float hp{100.0f}; float attack{8.0f}; float range{2.5f}; float speed{4.0f}; UnitRole role{UnitRole::Infantry}; AttackType attackType{AttackType::Melee}; UnitRole preferredTargetRole{UnitRole::Infantry}; std::array<uint16_t, static_cast<size_t>(UnitRole::Count)> vsRoleMultiplierPermille{1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000}; glm::vec2 pos{}; glm::vec2 renderPos{}; glm::vec2 target{}; glm::vec2 slotTarget{}; glm::vec2 moveDir{}; uint32_t targetUnit{}; uint32_t moveOrder{}; uint32_t attackMoveOrder{}; uint16_t targetLockTicks{}; uint16_t chaseTicks{}; uint16_t attackCooldownTicks{}; uint16_t lastTargetSwitchTick{}; uint16_t stuckTicks{}; uint16_t stealthRevealTicks{}; uint8_t orderPathLingerTicks{}; SupplyState supplyState{SupplyState::InSupply}; uint32_t transportId{0}; std::vector<uint32_t> cargo; bool hasMoveOrder{false}; bool attackMove{false}; bool embarked{false}; bool selected{false}; };

struct RoadSegment { uint32_t id{}; uint16_t owner{UINT16_MAX}; glm::ivec2 a{}; glm::ivec2 b{}; uint8_t quality{1}; };
struct TradeRoute { uint32_t id{}; uint16_t team{}; uint32_t fromCity{0}; uint32_t toCity{0}; bool active{false}; float efficiency{0.0f}; float wealthPerTick{0.0f}; uint32_t lastEvalTick{0}; };
struct OperationOrder { uint32_t id{}; uint16_t team{}; OperationType type{OperationType::RallyAndPush}; glm::vec2 target{}; uint32_t assignedTick{0}; bool active{true}; };
struct DiplomacyTreaty { bool tradeAgreement{false}; bool openBorders{false}; bool alliance{false}; bool nonAggression{false}; uint32_t lastChangedTick{0}; };
struct EspionageOp { uint32_t id{0}; uint16_t actor{0}; uint16_t target{0}; EspionageOpType type{EspionageOpType::ReconCity}; uint32_t startTick{0}; uint32_t durationTicks{0}; EspionageOpState state{EspionageOpState::Active}; int effectStrength{0}; };

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

struct BiomeRuntime {
  std::string id;
  std::string displayName;
  std::array<float, 3> palette{0.25f, 0.55f, 0.25f};
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

struct SimulationStats {
  uint32_t threads{1};
  uint32_t jobCount{0};
  uint32_t chunkCount{0};
  uint32_t movementTasks{0};
  uint32_t fogTasks{0};
  uint32_t territoryTasks{0};
  uint32_t navRequests{0};
  uint32_t navCompletions{0};
  uint32_t navStaleDrops{0};
  uint32_t eventCount{0};
  uint32_t roadCount{0};
  uint32_t activeTradeRoutes{0};
  uint32_t suppliedUnits{0};
  uint32_t lowSupplyUnits{0};
  uint32_t outOfSupplyUnits{0};
  uint32_t operationCount{0};
  float worldTension{0.0f};
  uint32_t allianceCount{0};
  uint32_t warCount{0};
  uint32_t activeEspionageOps{0};
  uint32_t postureChanges{0};
  uint32_t diplomacyEvents{0};
  uint32_t navalUnitCount{0};
  uint32_t transportCount{0};
  uint32_t embarkedUnitCount{0};
  uint32_t activeNavalOperations{0};
  uint32_t coastalTargets{0};
  uint32_t navalCombatEvents{0};
};

struct ChunkCoord {
  int x{0};
  int y{0};
};

struct ChunkRange {
  int start{0};
  int end{0};
};

struct Job {
  std::function<void()> execute;
};

struct TaskGraph {
  std::vector<Job> jobs;
};

struct World {
  uint32_t seed{1337}; int width{128}; int height{128};
  std::vector<float> heightmap; std::vector<float> fertility; std::vector<uint8_t> terrainClass; std::vector<uint8_t> biomeMap; std::vector<uint16_t> territoryOwner; std::vector<uint8_t> fog;
  std::vector<uint8_t> fogVisibilityByPlayer;
  std::vector<uint8_t> fogExploredByPlayer;
  std::vector<uint8_t> fogMaskByPlayer;
  std::vector<Unit> units; std::vector<City> cities; std::vector<Building> buildings; std::vector<ResourceNode> resourceNodes;
  std::vector<RoadSegment> roads; std::vector<TradeRoute> tradeRoutes; std::vector<OperationOrder> operations;
  std::vector<DiplomacyRelation> diplomacy; std::vector<DiplomacyTreaty> treaties; float worldTension{0.0f};
  std::vector<EspionageOp> espionageOps; std::vector<StrategicPosture> strategicPosture;
  std::vector<TriggerArea> triggerAreas; std::vector<Objective> objectives; std::vector<Trigger> triggers; std::vector<ObjectiveLogEntry> objectiveLog;
  std::vector<PlayerState> players;
  bool godMode{false}; uint32_t tick{0}; bool gameOver{false}; uint16_t winner{0}; MatchConfig config{}; MatchResult match{}; WonderState wonder{};
  bool uiBuildMenu{false}; bool uiTrainMenu{false}; bool uiResearchMenu{false}; bool placementActive{false}; BuildingType placementType{BuildingType::House}; glm::vec2 placementPos{}; bool placementValid{false};
  uint32_t territoryRecomputeCount{0}; uint32_t aiDecisionCount{0}; uint32_t completedBuildingsCount{0}; uint32_t trainedUnitsViaQueue{0}; uint32_t researchStartedCount{0}; uint32_t navVersion{1}; uint32_t flowFieldGeneratedCount{0}; uint32_t flowFieldCacheHitCount{0}; uint32_t groupMoveCommandCount{0}; uint32_t unitsReachedSlotCount{0}; uint32_t stuckMoveAssertions{0}; uint32_t totalDamageDealtPermille{0}; uint32_t combatEngagementCount{0}; uint32_t targetSwitchCount{0}; uint32_t chaseLimitBreakCount{0}; uint32_t buildingDamageEvents{0}; uint32_t unitDeathEvents{0}; uint32_t aiRetreatCount{0}; uint32_t focusFireEvents{0}; uint32_t combatTicks{0}; uint32_t rejectedCommandCount{0};
  uint32_t triggerExecutionCount{0}; uint32_t objectiveStateChangeCount{0};
  uint32_t logisticsRoadCount{0}; uint32_t logisticsTradeActiveCount{0}; uint32_t logisticsOperationIssuedCount{0};
  uint32_t diplomacyEventCount{0}; uint32_t postureChangeCount{0};
  uint32_t suppliedUnits{0}; uint32_t lowSupplyUnits{0}; uint32_t outOfSupplyUnits{0};
  uint32_t embarkEvents{0}; uint32_t disembarkEvents{0}; uint32_t navalCombatEvents{0};
  bool territoryDirty{true}; bool fogDirty{true};
};

void initialize_world(World& world, uint32_t seed);
void set_worker_threads(int threads);
int worker_threads();
void run_task_graph(TaskGraph& graph);
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
const BiomeRuntime& biome_runtime(BiomeType biome);
BiomeType biome_at(const World& world, int cellIndex);
std::string building_visual_variant_id(const World& world, const Building& building);

bool enqueue_train_unit(World& world, uint16_t team, uint32_t buildingId, UnitType type);
bool enqueue_age_research(World& world, uint16_t team, uint32_t buildingId);
bool cancel_queue_item(World& world, uint16_t team, uint32_t buildingId, size_t index);
bool players_allied(const World& world, uint16_t a, uint16_t b);
bool players_at_war(const World& world, uint16_t a, uint16_t b);
bool is_unit_visible_to_player(const World& world, const Unit& unit, uint16_t playerId);
bool trade_access_allowed(const World& world, uint16_t a, uint16_t b);
bool declare_war(World& world, uint16_t actor, uint16_t target);
bool form_alliance(World& world, uint16_t a, uint16_t b);
bool establish_trade_agreement(World& world, uint16_t a, uint16_t b);
bool break_treaty(World& world, uint16_t a, uint16_t b);
const char* posture_name(StrategicPosture posture);

uint64_t map_setup_hash(const World& world);
uint64_t state_hash(const World& world);
TickProfile last_tick_profile();
SimulationStats last_simulation_stats();
void consume_gameplay_events(std::vector<GameplayEvent>& out);

void rebuild_chunk_membership(const World& world);
ChunkRange query_chunk_range(int beginChunk, int maxChunkCount);
void process_chunk_range(const World& world, ChunkRange range, const std::function<void(int chunkIndex)>& fn);

} // namespace dom::sim
