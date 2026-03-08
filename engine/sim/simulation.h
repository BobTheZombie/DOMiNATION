#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace dom::sim {

enum class Resource : uint8_t { Food, Wood, Metal, Wealth, Knowledge, Oil, Count };
enum class RefinedGood : uint8_t { Steel, Fuel, Munitions, MachineParts, Electronics, Count };
enum class Age : uint8_t { Ancient, Classical, Medieval, Gunpowder, Enlightenment, Industrial, Modern, Information };
enum class UnitType : uint8_t { Worker, Infantry, Archer, Cavalry, Siege, TransportShip, LightWarship, HeavyWarship, BombardShip, Fighter, Interceptor, Bomber, StrategicBomber, ReconDrone, StrikeDrone, TacticalMissile, StrategicMissile, Count };
enum class BuildingType : uint8_t { CityCenter, House, Farm, LumberCamp, Mine, Market, Library, Barracks, Wonder, Port, RadarTower, MobileRadar, Airbase, MissileSilo, AABattery, AntiMissileDefense, SteelMill, Refinery, MunitionsPlant, MachineWorks, ElectronicsLab, FactoryHub, Count };
enum class UnitRole : uint8_t { Infantry, Ranged, Cavalry, Siege, Worker, Building, Naval, Transport, Count };
enum class AttackType : uint8_t { Melee, Ranged };
enum class MatchPhase : uint8_t { Running, Ended, Postmatch };
enum class VictoryCondition : uint8_t { None, Conquest, Score, Wonder };
enum class ResourceNodeType : uint8_t { Forest, Ore, Farmable, Ruins };
enum class ObjectiveState : uint8_t { Inactive, Active, Completed, Failed };
enum class ObjectiveCategory : uint8_t { Primary, Secondary, HiddenOptional };
enum class TriggerType : uint8_t { TickReached, UnitDestroyed, BuildingDestroyed, BuildingCompleted, ObjectiveCompleted, ObjectiveFailed, PlayerEliminated, AreaEntered, DiplomacyChanged, WorldTensionReached, StrategicStrikeLaunched, WonderCompleted, CargoLanded };
enum class TriggerActionType : uint8_t { ActivateObjective, CompleteObjective, FailObjective, ShowMessage, SpawnUnits, SpawnBuildings, GrantResources, ChangeDiplomacy, SetWorldTension, RevealArea, LaunchOperation, EndMatchVictory, EndMatchDefeat, RunLuaHook };
enum class MissionStatus : uint8_t { InBriefing, Running, Victory, Defeat, PartialVictory };
enum class WorldEventCategory : uint8_t { Climate, Health, Economic, Political, Industrial, Strategic, Mythic };
enum class WorldEventScope : uint8_t { Global, Regional, Player, Theater, Biome };
enum class WorldEventState : uint8_t { Inactive, Active, Resolved };
enum class WorldEventTriggerType : uint8_t { None, WorldTensionHigh, LowFoodReserve, RailDisruption, PlagueRisk, StrategicEscalation, GuardianPressure, CampaignFlag, AuthoredTick, Scripted };

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
  GuardianDiscovered,
  GuardianJoined,
  GuardianKilled,
};

enum class GuardianSiteType : uint8_t { YetiLair, AbyssalTrench, DuneNest, SacredGrove, FrozenCavern };
enum class GuardianSpawnMode : uint8_t { OnDiscovery, ScenarioStart };
enum class GuardianDiscoveryMode : uint8_t { Proximity, FogReveal, UndergroundDiscovery, ScriptedReveal };
enum class GuardianBehaviorMode : uint8_t { DormantUntilDiscovery, NeutralEncounter, HostileEncounter };
enum class GuardianJoinMode : uint8_t { DiscovererControl, RemainNeutral, NeverJoin };

struct GuardianDefinition {
  std::string guardianId;
  std::string displayName;
  uint8_t biomeRequirement{0};
  GuardianSiteType siteType{GuardianSiteType::YetiLair};
  GuardianSpawnMode spawnMode{GuardianSpawnMode::OnDiscovery};
  uint32_t maxPerMap{1};
  bool unique{true};
  GuardianDiscoveryMode discoveryMode{GuardianDiscoveryMode::Proximity};
  GuardianBehaviorMode behaviorMode{GuardianBehaviorMode::DormantUntilDiscovery};
  GuardianJoinMode joinMode{GuardianJoinMode::DiscovererControl};
  std::string associatedUnitDefinitionId;
  float unitHp{3500.0f};
  float unitAttack{90.0f};
  float unitRange{1.6f};
  float unitSpeed{1.6f};
  std::string rewardHook;
  std::string effectHook;
  bool scenarioOnly{false};
  bool procedural{true};
  int rarityPermille{8};
  int minSpacingCells{20};
  float discoveryRadius{5.0f};
};

struct GuardianSiteInstance {
  uint32_t instanceId{0};
  std::string guardianId;
  GuardianSiteType siteType{GuardianSiteType::YetiLair};
  glm::vec2 pos{};
  int32_t regionId{-1};
  int32_t nodeId{-1};
  bool discovered{false};
  bool alive{true};
  uint16_t owner{UINT16_MAX};
  bool siteActive{true};
  bool siteDepleted{false};
  bool spawned{false};
  uint8_t behaviorState{0};
  uint32_t cooldownTicks{0};
  bool oneShotUsed{false};
  bool scenarioPlaced{false};
};

enum class DiplomacyRelation : uint8_t { Allied, Neutral, War, Ceasefire };
enum class EspionageOpType : uint8_t { ReconCity, RevealRoute, SabotageEconomy, SabotageSupply, CounterIntel };
enum class EspionageOpState : uint8_t { Active, Completed, Failed };
enum class StrategicPosture : uint8_t { Expansionist, Defensive, TradeFocused, Escalating, TotalWar };
enum class AirUnitClass : uint8_t { Fighter, Interceptor, Bomber, StrategicBomber, ReconDrone, StrikeDrone };
enum class AirMissionState : uint8_t { Airborne, Attacking, Returning };
enum class DetectorType : uint8_t { RadarTower, MobileRadar, ReconDrone, NavalSensor, AirbaseRadar, SatelliteUplink, AABattery, AntiMissileDefense };
enum class StrikeType : uint8_t { TacticalMissile, StrategicMissile, StrategicBomberStrike, AbstractWMD };
enum class DeterrencePosture : uint8_t { Restrained, NoFirstUse, FlexibleResponse, MassiveRetaliation, LaunchOnWarning };
enum class StrategicStrikePhase : uint8_t { Unavailable, Preparing, Ready, Launched, Intercepted, Detonated, Resolved };
enum class StrategicInterceptionResult : uint8_t { Undetected, Detected, PartiallyIntercepted, FullyIntercepted, ReducedEffect };

enum class SupplyState : uint8_t { InSupply, LowSupply, OutOfSupply };
enum class OperationType : uint8_t { AssaultCity, DefendBorder, SecureRoute, RaidEconomy, RallyAndPush, AmphibiousAssault, NavalPatrol, CoastalBombard, Encirclement, NavalBlockade, StrategicBombing, MissileStrikeCampaign };
enum class TheaterPriority : uint8_t { Low, Medium, High, Critical };
enum class ArmyGroupStance : uint8_t { Offensive, Defensive };
enum class NavalMissionType : uint8_t { Patrol, Escort, Assault };
enum class AirMissionType : uint8_t { Bombing, Interception };
enum class OperationOutcome : uint8_t { InProgress, Success, Failure };
enum class TerrainClass : uint8_t { Land, ShallowWater, DeepWater };
enum class RailNodeType : uint8_t { Junction, Station, Depot };
enum class TrainType : uint8_t { Supply, Freight, Armored };
enum class TrainState : uint8_t { Active, Inactive, Delayed };
enum class BiomeType : uint8_t { TemperateGrassland, Steppe, Forest, Desert, Mediterranean, Jungle, Tundra, Arctic, Coast, Wetlands, Mountain, SnowMountain, Count };
enum class WorldPreset : uint8_t { Pangaea, Continents, Archipelago, InlandSea, MountainWorld };
enum class MineralType : uint8_t { Gold, Iron, Silver, Copper, Stone, Count };
enum class UndergroundNodeType : uint8_t { Shaft, Deposit, Junction, Depot, Exit };

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

struct IndustrialRecipe {
  RefinedGood output{RefinedGood::Steel};
  std::array<float, static_cast<size_t>(Resource::Count)> inputResources{};
  std::array<float, static_cast<size_t>(RefinedGood::Count)> inputGoods{};
  float outputAmount{0.0f};
  float cycleTime{10.0f};
};

struct FactoryState {
  uint8_t recipeIndex{0};
  float cycleProgress{0.0f};
  std::array<float, static_cast<size_t>(Resource::Count)> inputBuffer{};
  std::array<float, static_cast<size_t>(RefinedGood::Count)> outputBuffer{};
  bool paused{false};
  bool blocked{false};
  bool active{false};
  float throughputBonus{0.0f};
};

struct Unit { uint32_t id{}; uint16_t team{}; UnitType type{UnitType::Infantry}; float hp{100.0f}; float attack{8.0f}; float range{2.5f}; float speed{4.0f}; UnitRole role{UnitRole::Infantry}; AttackType attackType{AttackType::Melee}; UnitRole preferredTargetRole{UnitRole::Infantry}; std::array<uint16_t, static_cast<size_t>(UnitRole::Count)> vsRoleMultiplierPermille{1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000}; glm::vec2 pos{}; glm::vec2 renderPos{}; glm::vec2 target{}; glm::vec2 slotTarget{}; glm::vec2 moveDir{}; uint32_t targetUnit{}; uint32_t moveOrder{}; uint32_t attackMoveOrder{}; uint16_t targetLockTicks{}; uint16_t chaseTicks{}; uint16_t attackCooldownTicks{}; uint16_t lastTargetSwitchTick{}; uint16_t stuckTicks{}; uint16_t stealthRevealTicks{}; uint8_t orderPathLingerTicks{}; SupplyState supplyState{SupplyState::InSupply}; uint32_t transportId{0}; std::string definitionId; std::vector<uint32_t> cargo; bool hasMoveOrder{false}; bool attackMove{false}; bool embarked{false}; bool selected{false}; };

struct RoadSegment { uint32_t id{}; uint16_t owner{UINT16_MAX}; glm::ivec2 a{}; glm::ivec2 b{}; uint8_t quality{1}; };
struct TradeRoute { uint32_t id{}; uint16_t team{}; uint32_t fromCity{0}; uint32_t toCity{0}; bool active{false}; float efficiency{0.0f}; float wealthPerTick{0.0f}; uint32_t lastEvalTick{0}; };
struct OperationOrder { uint32_t id{}; uint16_t team{}; OperationType type{OperationType::RallyAndPush}; glm::vec2 target{}; uint32_t assignedTick{0}; bool active{true}; };
struct ArmyGroup { uint32_t id{0}; uint16_t owner{0}; uint32_t theaterId{0}; std::vector<uint32_t> unitIds; ArmyGroupStance stance{ArmyGroupStance::Defensive}; uint32_t assignedObjective{0}; bool active{true}; };
struct NavalTaskForce { uint32_t id{0}; uint16_t owner{0}; uint32_t theaterId{0}; std::vector<uint32_t> unitIds; NavalMissionType mission{NavalMissionType::Patrol}; uint32_t assignedObjective{0}; bool active{true}; };
struct AirWing { uint32_t id{0}; uint16_t owner{0}; uint32_t theaterId{0}; std::vector<uint32_t> squadronIds; AirMissionType mission{AirMissionType::Interception}; uint32_t assignedObjective{0}; bool active{true}; };
struct TheaterCommand { uint32_t theaterId{0}; uint16_t owner{0}; glm::ivec4 bounds{}; TheaterPriority priority{TheaterPriority::Medium}; std::vector<uint32_t> activeOperations; std::vector<uint32_t> assignedArmyGroups; std::vector<uint32_t> assignedNavalTaskForces; std::vector<uint32_t> assignedAirWings; float supplyStatus{1.0f}; float threatLevel{0.0f}; };
struct OperationalObjective { uint32_t id{0}; uint16_t owner{0}; uint32_t theaterId{0}; OperationType objectiveType{OperationType::RallyAndPush}; glm::ivec4 targetRegion{}; uint32_t requiredForce{0}; uint32_t startTick{0}; uint32_t durationTicks{0}; OperationOutcome outcome{OperationOutcome::InProgress}; bool active{true}; std::vector<uint32_t> armyGroups; std::vector<uint32_t> navalTaskForces; std::vector<uint32_t> airWings; };
struct RailNode { uint32_t id{0}; uint16_t owner{UINT16_MAX}; RailNodeType type{RailNodeType::Junction}; glm::ivec2 tile{}; uint32_t networkId{0}; bool active{true}; };
struct RailEdge { uint32_t id{0}; uint16_t owner{UINT16_MAX}; uint32_t aNode{0}; uint32_t bNode{0}; uint8_t quality{1}; bool bridge{false}; bool tunnel{false}; bool disrupted{false}; };
struct RailNetwork { uint32_t id{0}; uint16_t owner{UINT16_MAX}; uint32_t nodeCount{0}; uint32_t edgeCount{0}; bool active{false}; };
struct TrainRouteStep { uint32_t edgeId{0}; uint32_t toNode{0}; };
struct Train { uint32_t id{0}; uint16_t owner{UINT16_MAX}; TrainType type{TrainType::Supply}; TrainState state{TrainState::Inactive}; uint32_t currentNode{0}; uint32_t destinationNode{0}; uint32_t currentEdge{0}; uint32_t routeCursor{0}; float segmentProgress{0.0f}; float speed{0.03f}; float cargo{0.0f}; float capacity{100.0f}; std::string cargoType{"Supply"}; uint32_t lastRouteTick{0}; std::vector<TrainRouteStep> route; };
struct DiplomacyTreaty { bool tradeAgreement{false}; bool openBorders{false}; bool alliance{false}; bool nonAggression{false}; uint32_t lastChangedTick{0}; };
struct EspionageOp { uint32_t id{0}; uint16_t actor{0}; uint16_t target{0}; EspionageOpType type{EspionageOpType::ReconCity}; uint32_t startTick{0}; uint32_t durationTicks{0}; EspionageOpState state{EspionageOpState::Active}; int effectStrength{0}; };
struct AirUnit { uint32_t id{0}; uint16_t team{0}; AirUnitClass cls{AirUnitClass::Fighter}; AirMissionState state{AirMissionState::Airborne}; glm::vec2 pos{}; glm::vec2 missionTarget{}; float hp{100.0f}; float speed{6.0f}; uint32_t cooldownTicks{0}; bool missionPerformed{false}; };
struct DetectorSite { uint32_t id{0}; uint16_t team{0}; DetectorType type{DetectorType::RadarTower}; glm::vec2 pos{}; float radius{12.0f}; bool revealContactOnly{false}; bool active{true}; };
struct StrategicStrike { uint32_t id{0}; uint16_t team{0}; StrikeType type{StrikeType::TacticalMissile}; glm::vec2 from{}; glm::vec2 target{}; uint32_t prepTicksRemaining{0}; uint32_t travelTicksRemaining{0}; uint32_t cooldownTicks{0}; uint8_t interceptionState{0}; bool launched{false}; bool resolved{false}; StrategicStrikePhase phase{StrategicStrikePhase::Unavailable}; StrategicInterceptionResult interceptionResult{StrategicInterceptionResult::Undetected}; uint16_t targetTeam{UINT16_MAX}; uint16_t launchSystemCount{1}; bool warningIssued{false}; bool retaliationLaunch{false}; bool secondStrikeLaunch{false}; };
struct DenialZone { uint32_t id{0}; uint16_t team{0}; glm::vec2 pos{}; float radius{6.0f}; uint32_t ticksRemaining{0}; };
struct StrategicDeterrenceState {
  bool strategicCapabilityEnabled{false};
  uint16_t strategicStockpile{0};
  uint16_t strategicReadyCount{0};
  uint16_t strategicPreparingCount{0};
  uint8_t strategicAlertLevel{0};
  DeterrencePosture deterrencePosture{DeterrencePosture::Restrained};
  bool launchWarningActive{false};
  uint32_t recentStrategicUseTick{0};
  bool retaliationCapability{false};
  bool secondStrikeCapability{false};
};

struct City { uint32_t id{}; uint16_t team{}; glm::vec2 pos{}; int level{1}; bool capital{false}; };

struct Building { uint32_t id{}; uint16_t team{}; BuildingType type{BuildingType::House}; glm::vec2 pos{}; glm::vec2 size{2.0f, 2.0f}; bool underConstruction{true}; float buildProgress{0.0f}; float buildTime{10.0f}; float hp{1000.0f}; float maxHp{1000.0f}; std::string definitionId; std::vector<ProductionItem> queue; FactoryState factory{}; };

struct ResourceNode { uint32_t id{}; ResourceNodeType type{ResourceNodeType::Forest}; glm::vec2 pos{}; float amount{1000.0f}; uint16_t owner{UINT16_MAX}; };
struct MountainRegion { uint32_t id{0}; int minX{0}; int minY{0}; int maxX{0}; int maxY{0}; int peakCell{-1}; int centerCell{-1}; uint32_t cellCount{0}; };
struct SurfaceDeposit { uint32_t id{0}; uint32_t regionId{0}; MineralType mineral{MineralType::Iron}; int cell{-1}; float remaining{0.0f}; uint16_t owner{UINT16_MAX}; };
struct DeepDeposit { uint32_t id{0}; uint32_t regionId{0}; uint32_t nodeId{0}; MineralType mineral{MineralType::Gold}; int cell{-1}; float richness{0.0f}; float remaining{0.0f}; uint16_t owner{UINT16_MAX}; bool active{true}; };
struct StartCandidate { int cell{-1}; float score{0.0f}; uint8_t civBiasMask{0}; };
struct MythicCandidate { GuardianSiteType siteType{GuardianSiteType::YetiLair}; int cell{-1}; float score{0.0f}; };
struct UndergroundNode { uint32_t id{0}; uint32_t regionId{0}; UndergroundNodeType type{UndergroundNodeType::Junction}; int cell{-1}; uint32_t linkedBuildingId{0}; uint16_t owner{UINT16_MAX}; bool active{true}; };
struct UndergroundEdge { uint32_t id{0}; uint32_t regionId{0}; uint32_t a{0}; uint32_t b{0}; uint16_t owner{UINT16_MAX}; bool active{true}; };
struct TriggerArea { uint32_t id{}; glm::vec2 min{}; glm::vec2 max{}; };
struct Objective { uint32_t id{}; std::string objectiveId; std::string title; std::string description; std::string text; bool primary{true}; ObjectiveCategory category{ObjectiveCategory::Primary}; uint16_t owner{UINT16_MAX}; ObjectiveState state{ObjectiveState::Inactive}; bool visible{true}; float progressValue{0.0f}; std::string progressText; };
struct TriggerCondition { TriggerType type{TriggerType::TickReached}; uint32_t tick{0}; uint32_t entityId{0}; BuildingType buildingType{BuildingType::House}; uint32_t areaId{0}; uint16_t player{UINT16_MAX}; uint32_t objectiveId{0}; float worldTension{0.0f}; DiplomacyRelation diplomacy{DiplomacyRelation::Neutral}; uint16_t playerB{UINT16_MAX}; };
struct TriggerAction { TriggerActionType type{TriggerActionType::ShowMessage}; std::string text; uint32_t objectiveId{0}; ObjectiveState objectiveState{ObjectiveState::Active}; uint16_t player{UINT16_MAX}; std::array<float, static_cast<size_t>(Resource::Count)> resources{}; UnitType spawnUnitType{UnitType::Infantry}; BuildingType spawnBuildingType{BuildingType::House}; uint32_t spawnCount{0}; glm::vec2 spawnPos{}; uint16_t winner{0}; uint32_t areaId{0}; DiplomacyRelation diplomacy{DiplomacyRelation::Neutral}; uint16_t playerB{UINT16_MAX}; float worldTension{0.0f}; OperationType operationType{OperationType::RallyAndPush}; glm::vec2 operationTarget{}; std::string luaHook; };
struct Trigger { uint32_t id{}; bool once{true}; bool fired{false}; TriggerCondition condition{}; std::vector<TriggerAction> actions; };
struct ObjectiveLogEntry { uint32_t tick{0}; std::string text; };
struct MissionMessageDefinition { std::string messageId; std::string title; std::string body; std::string category{"intelligence"}; std::string speaker; std::string faction; std::string portraitId; std::string iconId; std::string imageId; std::string styleTag; int priority{0}; uint32_t durationTicks{600}; bool sticky{false}; };
struct MissionMessageRuntime { uint64_t sequence{0}; uint32_t tick{0}; std::string messageId; std::string title; std::string body; std::string category{"intelligence"}; std::string speaker; std::string faction; std::string portraitId; std::string iconId; std::string imageId; std::string styleTag; int priority{0}; uint32_t durationTicks{600}; bool sticky{false}; };
struct WorldEventDefinition {
  std::string eventId;
  std::string displayName;
  WorldEventCategory category{WorldEventCategory::Climate};
  WorldEventTriggerType triggerType{WorldEventTriggerType::None};
  WorldEventScope scope{WorldEventScope::Global};
  uint16_t targetPlayer{UINT16_MAX};
  int32_t targetRegion{-1};
  int32_t targetTheater{-1};
  int32_t targetBiome{-1};
  uint32_t minTick{0};
  uint32_t cooldownTicks{1200};
  uint32_t defaultDuration{1200};
  float triggerThreshold{0.0f};
  float baseSeverity{1.0f};
  bool authored{false};
  std::vector<std::string> campaignTags;
  std::string scriptedHook;
};
struct WorldEventInstance {
  std::string eventId;
  std::string displayName;
  WorldEventCategory category{WorldEventCategory::Climate};
  WorldEventScope scope{WorldEventScope::Global};
  uint16_t targetPlayer{UINT16_MAX};
  int32_t targetRegion{-1};
  int32_t targetTheater{-1};
  int32_t targetBiome{-1};
  uint32_t startTick{0};
  uint32_t durationTicks{0};
  float severity{1.0f};
  WorldEventState state{WorldEventState::Inactive};
  std::string effectPayload;
  std::vector<std::string> campaignTags;
  std::string scriptedHook;
};
struct ObjectiveTransitionDebugEntry { uint32_t tick{0}; uint32_t objectiveId{0}; ObjectiveState from{ObjectiveState::Inactive}; ObjectiveState to{ObjectiveState::Inactive}; uint32_t triggerId{0}; std::string actionType; std::string reason; };
struct MissionDefinition {
  std::string title;
  std::string subtitle;
  std::string locationLabel;
  std::string briefing;
  std::string debrief;
  std::string factionSummary;
  std::string carryoverSummary;
  std::string briefingPortraitId;
  std::string debriefPortraitId;
  std::string missionImageId;
  std::string factionIconId;
  std::vector<std::string> scenarioTags;
  std::vector<std::string> objectiveSummary;
  std::vector<std::string> introMessages;
  std::vector<MissionMessageDefinition> messageDefinitions;
  std::string victoryOutcomeTag{"victory"}; std::string defeatOutcomeTag{"defeat"}; std::string partialOutcomeTag{"partial_victory"}; std::string branchKey; std::string luaScriptFile; std::string luaScriptInline;
};
struct MissionRuntimeState { bool briefingShown{false}; MissionStatus status{MissionStatus::InBriefing}; std::string resultTag; std::vector<uint32_t> activeObjectives; std::vector<std::string> luaHookLog; uint32_t firedTriggerCount{0}; uint32_t scriptedActionCount{0}; };

struct CampaignCarryoverState {
  std::string campaignId;
  std::string playerCivilizationId;
  uint8_t unlockedAge{0};
  std::array<float, static_cast<size_t>(Resource::Count)> resources{};
  std::vector<uint32_t> veteranUnitIds;
  std::vector<std::string> discoveredGuardians;
  float worldTension{0.0f};
  std::vector<std::string> unlockedRewards;
  std::vector<std::pair<std::string, bool>> flags;
  std::vector<std::pair<std::string, int64_t>> variables;
  std::string previousMissionResult;
  std::string pendingBranchKey;
};

struct CivilizationRuntime {
  std::string id{"default"};
  std::string displayName{"Default"};
  std::string shortDescription;
  std::string themeId{"default"};
  std::vector<std::string> eraFlavorTags;
  std::vector<std::string> campaignTags;
  float economyBias{1.0f};
  float militaryBias{1.0f};
  float scienceBias{1.0f};
  float aggression{1.0f};
  float defense{1.0f};
  float diplomacyBias{1.0f};
  float logisticsBias{1.0f};
  float strategicBias{1.0f};
  float aiWorkerTargetMult{1.0f};
  float aiExpansionTiming{1.0f};
  float aiResearchPriority{1.0f};
  float aiReconPriority{1.0f};
  float aiNavalPriority{1.0f};
  float aiAirPriority{1.0f};
  float aiStrategicPriority{1.0f};
  std::array<float, static_cast<size_t>(Resource::Count)> resourceGatherMult = [](){ std::array<float, static_cast<size_t>(Resource::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(RefinedGood::Count)> refinedGoodOutputMult = [](){ std::array<float, static_cast<size_t>(RefinedGood::Count)> a{}; a.fill(1.0f); return a; }();
  float roadBonus{1.0f};
  float railBonus{1.0f};
  float supplyBonus{1.0f};
  float tradeRouteBonus{1.0f};
  float tunnelExtractionBonus{1.0f};
  float factoryThroughputBonus{1.0f};
  float aggressionBias{1.0f};
  float allianceBias{1.0f};
  float tradeBias{1.0f};
  float worldTensionResponseBias{1.0f};
  std::array<float, static_cast<size_t>(OperationType::MissileStrikeCampaign) + 1> operationPreference = [](){ std::array<float, static_cast<size_t>(OperationType::MissileStrikeCampaign) + 1> a{}; a.fill(1.0f); return a; }();
  std::vector<std::string> doctrineTags;
  std::array<float, static_cast<size_t>(UnitType::Count)> unitAttackMult = [](){ std::array<float, static_cast<size_t>(UnitType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(UnitType::Count)> unitHpMult = [](){ std::array<float, static_cast<size_t>(UnitType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(UnitType::Count)> unitCostMult = [](){ std::array<float, static_cast<size_t>(UnitType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(UnitType::Count)> unitTrainTimeMult = [](){ std::array<float, static_cast<size_t>(UnitType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingCostMult = [](){ std::array<float, static_cast<size_t>(BuildingType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingBuildTimeMult = [](){ std::array<float, static_cast<size_t>(BuildingType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingHpMult = [](){ std::array<float, static_cast<size_t>(BuildingType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<float, static_cast<size_t>(BuildingType::Count)> buildingTrickleMult = [](){ std::array<float, static_cast<size_t>(BuildingType::Count)> a{}; a.fill(1.0f); return a; }();
  std::array<std::string, static_cast<size_t>(UnitType::Count)> uniqueUnitDefs{};
  std::array<std::string, static_cast<size_t>(BuildingType::Count)> uniqueBuildingDefs{};
  std::vector<std::string> missionTags;
};

struct ContentPresentationInfo {
  std::string displayName;
  std::string iconId;
  std::string portraitId;
  bool unique{false};
};

struct BiomeRuntime {
  std::string id;
  std::string displayName;
  std::array<float, 3> palette{0.25f, 0.55f, 0.25f};
};

struct PlayerState { uint16_t id{}; Age age{Age::Ancient}; std::array<float, static_cast<size_t>(Resource::Count)> resources{400, 350, 250, 250, 100, 0}; std::array<float, static_cast<size_t>(RefinedGood::Count)> refinedGoods{}; int popUsed{0}; int popCap{10}; int score{0}; bool alive{true}; uint32_t unitsLost{0}; uint32_t buildingsLost{0}; int finalScore{0}; bool isHuman{false}; bool isCPU{true}; uint16_t teamId{0}; std::array<float, 3> color{0.8f, 0.8f, 0.8f}; CivilizationRuntime civilization{}; };
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
  uint32_t airUnitCount{0};
  uint32_t detectorCount{0};
  uint32_t radarReveals{0};
  uint32_t strategicStrikes{0};
  uint32_t interceptions{0};
  uint32_t strategicStockpileTotal{0};
  uint32_t strategicReadyTotal{0};
  uint32_t strategicPreparingTotal{0};
  uint32_t strategicWarnings{0};
  uint32_t strategicRetaliations{0};
  uint32_t secondStrikeReadyCount{0};
  uint32_t deterrencePostureChanges{0};
  uint32_t activeDenialZones{0};
  uint32_t mountainRegionCount{0};
  uint32_t mountainChainCount{0};
  uint32_t riverCount{0};
  uint32_t lakeCount{0};
  uint32_t startCandidateCount{0};
  uint32_t mythicCandidateCount{0};
  uint32_t surfaceDepositCount{0};
  uint32_t deepDepositCount{0};
  uint32_t activeMineShafts{0};
  uint32_t activeTunnels{0};
  uint32_t undergroundDepots{0};
  float undergroundYield{0.0f};
  uint32_t guardianSiteCount{0};
  uint32_t guardiansDiscovered{0};
  uint32_t guardiansSpawned{0};
  uint32_t guardiansJoined{0};
  uint32_t guardiansKilled{0};
  uint32_t hostileGuardianEvents{0};
  uint32_t alliedGuardianEvents{0};
  uint32_t railNodeCount{0};
  uint32_t railEdgeCount{0};
  uint32_t activeRailNetworks{0};
  uint32_t activeTrains{0};
  uint32_t activeSupplyTrains{0};
  uint32_t activeFreightTrains{0};
  float railThroughput{0.0f};
  uint32_t disruptedRailRoutes{0};
  uint32_t campaignMissionCount{0};
  uint32_t campaignFlagsSet{0};
  uint32_t campaignResourcesCount{0};
  uint32_t campaignBranchesTaken{0};
  uint32_t factoryCount{0};
  uint32_t activeFactories{0};
  uint32_t blockedFactories{0};
  uint32_t activeWorldEvents{0};
  uint32_t resolvedWorldEvents{0};
  uint32_t totalWorldEventsTriggered{0};
  float steelOutput{0.0f};
  float fuelOutput{0.0f};
  float munitionsOutput{0.0f};
  float machinePartsOutput{0.0f};
  float electronicsOutput{0.0f};
  float industrialThroughput{0.0f};
  uint32_t uniqueUnitsProduced{0};
  uint32_t uniqueBuildingsConstructed{0};
  uint32_t civContentResolutionFallbacks{0};
  uint32_t romeContentUsage{0};
  uint32_t chinaContentUsage{0};
  uint32_t europeContentUsage{0};
  uint32_t middleEastContentUsage{0};
  uint32_t civDoctrineSwitches{0};
  float civIndustryOutput{0.0f};
  float civLogisticsBonusUsage{0.0f};
  uint32_t civOperationCount{0};
  uint32_t contentFallbackCount{0};
  uint32_t civPresentationResolves{0};
  uint32_t guardianPresentationResolves{0};
  uint32_t campaignPresentationResolves{0};
  uint32_t eventPresentationResolves{0};
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
  WorldPreset worldPreset{WorldPreset::Pangaea};
  std::vector<float> heightmap; std::vector<float> fertility; std::vector<uint8_t> terrainClass; std::vector<uint8_t> biomeMap; std::vector<uint16_t> territoryOwner; std::vector<uint8_t> fog;
  std::vector<float> temperatureMap;
  std::vector<float> moistureMap;
  std::vector<uint8_t> coastClassMap;
  std::vector<int32_t> landmassIdByCell;
  std::vector<uint8_t> riverMap;
  std::vector<uint8_t> lakeMap;
  std::vector<float> resourceWeightMap;
  std::vector<StartCandidate> startCandidates;
  std::vector<MythicCandidate> mythicCandidates;
  std::vector<uint8_t> fogVisibilityByPlayer;
  std::vector<uint8_t> fogExploredByPlayer;
  std::vector<uint8_t> fogMaskByPlayer;
  std::vector<Unit> units; std::vector<City> cities; std::vector<Building> buildings; std::vector<ResourceNode> resourceNodes;
  std::vector<RoadSegment> roads; std::vector<TradeRoute> tradeRoutes; std::vector<OperationOrder> operations;
  std::vector<TheaterCommand> theaterCommands;
  std::vector<ArmyGroup> armyGroups;
  std::vector<NavalTaskForce> navalTaskForces;
  std::vector<AirWing> airWings;
  std::vector<OperationalObjective> operationalObjectives;
  std::vector<RailNode> railNodes; std::vector<RailEdge> railEdges; std::vector<RailNetwork> railNetworks; std::vector<Train> trains;
  std::vector<IndustrialRecipe> industrialRecipes;
  std::vector<DiplomacyRelation> diplomacy; std::vector<DiplomacyTreaty> treaties; float worldTension{0.0f};
  std::vector<EspionageOp> espionageOps; std::vector<StrategicPosture> strategicPosture;
  std::vector<AirUnit> airUnits;
  std::vector<DetectorSite> detectors;
  std::vector<StrategicStrike> strategicStrikes;
  std::vector<StrategicDeterrenceState> strategicDeterrence;
  std::vector<DenialZone> denialZones;
  std::vector<int32_t> mountainRegionByCell;
  std::vector<MountainRegion> mountainRegions;
  std::vector<SurfaceDeposit> surfaceDeposits;
  std::vector<DeepDeposit> deepDeposits;
  std::vector<UndergroundNode> undergroundNodes;
  std::vector<UndergroundEdge> undergroundEdges;
  std::vector<GuardianDefinition> guardianDefinitions;
  std::vector<GuardianSiteInstance> guardianSites;
  std::vector<uint8_t> radarContactByPlayer;
  std::vector<TriggerArea> triggerAreas; std::vector<Objective> objectives; std::vector<Trigger> triggers; std::vector<ObjectiveLogEntry> objectiveLog;
  std::vector<WorldEventDefinition> worldEventDefinitions;
  std::vector<WorldEventInstance> worldEvents;
  std::vector<MissionMessageRuntime> missionMessages;
  std::vector<ObjectiveTransitionDebugEntry> objectiveDebugLog;
  uint64_t nextMissionMessageSequence{1};
  MissionDefinition mission{};
  MissionRuntimeState missionRuntime{};
  CampaignCarryoverState campaign{};
  std::vector<PlayerState> players;
  bool godMode{false}; uint32_t tick{0}; bool gameOver{false}; uint16_t winner{0}; MatchConfig config{}; MatchResult match{}; WonderState wonder{};
  bool uiBuildMenu{false}; bool uiTrainMenu{false}; bool uiResearchMenu{false}; bool placementActive{false}; BuildingType placementType{BuildingType::House}; glm::vec2 placementPos{}; bool placementValid{false};
  uint32_t territoryRecomputeCount{0}; uint32_t aiDecisionCount{0}; uint32_t completedBuildingsCount{0}; uint32_t trainedUnitsViaQueue{0}; uint32_t researchStartedCount{0}; uint32_t navVersion{1}; uint32_t flowFieldGeneratedCount{0}; uint32_t flowFieldCacheHitCount{0}; uint32_t groupMoveCommandCount{0}; uint32_t unitsReachedSlotCount{0}; uint32_t stuckMoveAssertions{0}; uint32_t totalDamageDealtPermille{0}; uint32_t combatEngagementCount{0}; uint32_t targetSwitchCount{0}; uint32_t chaseLimitBreakCount{0}; uint32_t buildingDamageEvents{0}; uint32_t unitDeathEvents{0}; uint32_t aiRetreatCount{0}; uint32_t focusFireEvents{0}; uint32_t combatTicks{0}; uint32_t rejectedCommandCount{0};
  uint32_t triggerExecutionCount{0}; uint32_t objectiveStateChangeCount{0};
  uint32_t logisticsRoadCount{0}; uint32_t logisticsTradeActiveCount{0}; uint32_t logisticsOperationIssuedCount{0};
  uint32_t theatersCreatedCount{0}; uint32_t operationsExecutedCount{0}; uint32_t formationsAssignedCount{0}; uint32_t operationalOutcomesRecorded{0};
  uint32_t diplomacyEventCount{0}; uint32_t postureChangeCount{0};
  uint32_t suppliedUnits{0}; uint32_t lowSupplyUnits{0}; uint32_t outOfSupplyUnits{0};
  uint32_t embarkEvents{0}; uint32_t disembarkEvents{0}; uint32_t navalCombatEvents{0};
  uint32_t radarRevealEvents{0}; uint32_t strategicStrikeEvents{0}; uint32_t interceptionEvents{0}; uint32_t airMissionEvents{0};
  uint32_t strategicWarningEvents{0}; uint32_t strategicRetaliationEvents{0};
  uint32_t strategicStockpileTotal{0}; uint32_t strategicReadyTotal{0}; uint32_t strategicPreparingTotal{0}; uint32_t secondStrikeReadyCount{0};
  uint32_t deterrencePostureChangeCount{0};
  uint32_t mountainRegionCount{0}; uint32_t surfaceDepositCount{0}; uint32_t deepDepositCount{0};
  uint32_t mountainChainCount{0}; uint32_t riverCount{0}; uint32_t lakeCount{0}; uint32_t startCandidateCount{0}; uint32_t mythicCandidateCount{0};
  uint32_t activeMineShafts{0}; uint32_t activeTunnels{0}; uint32_t undergroundDepots{0};
  float undergroundYield{0.0f};
  uint32_t guardiansDiscovered{0};
  uint32_t guardiansSpawned{0};
  uint32_t guardiansJoined{0};
  uint32_t guardiansKilled{0};
  uint32_t hostileGuardianEvents{0};
  uint32_t alliedGuardianEvents{0};
  uint32_t railNodeCount{0};
  uint32_t railEdgeCount{0};
  uint32_t activeRailNetworks{0};
  uint32_t activeTrains{0};
  uint32_t activeSupplyTrains{0};
  uint32_t activeFreightTrains{0};
  float railThroughput{0.0f};
  uint32_t disruptedRailRoutes{0};
  uint32_t factoryCount{0};
  uint32_t activeFactories{0};
  uint32_t blockedFactories{0};
  float industrialThroughput{0.0f};
  std::array<float, static_cast<size_t>(RefinedGood::Count)> refinedOutputByTick{};
  uint32_t uniqueUnitsProduced{0};
  uint32_t uniqueBuildingsConstructed{0};
  uint32_t civContentResolutionFallbacks{0};
  uint32_t romeContentUsage{0};
  uint32_t chinaContentUsage{0};
  uint32_t europeContentUsage{0};
  uint32_t middleEastContentUsage{0};
  uint32_t civDoctrineSwitches{0};
  float civIndustryOutput{0.0f};
  float civLogisticsBonusUsage{0.0f};
  uint32_t civOperationCount{0};
  uint32_t activeWorldEventCount{0};
  uint32_t resolvedWorldEventCount{0};
  uint32_t triggeredWorldEventCount{0};
  bool territoryDirty{true}; bool fogDirty{true};
};


void initialize_world(World& world, uint32_t seed);
void set_world_preset(World& world, WorldPreset preset);
WorldPreset parse_world_preset(const std::string& value);
const char* world_preset_name(WorldPreset preset);
void set_worker_threads(int threads);
int worker_threads();
void run_task_graph(TaskGraph& graph);
bool load_scenario_file(World& world, const std::string& path, uint32_t fallbackSeed, std::string& err);
bool save_scenario_file(const std::string& path, const World& world, std::string& err);
CivilizationRuntime civilization_runtime_for(const std::string& id);
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
ContentPresentationInfo unit_content_presentation(const World& world, uint16_t team, UnitType type, const std::string& definitionId);
ContentPresentationInfo building_content_presentation(const World& world, uint16_t team, BuildingType type, const std::string& definitionId);
ContentPresentationInfo guardian_content_presentation(const std::string& guardianId, GuardianSiteType siteType);
ContentPresentationInfo event_content_presentation(const std::string& eventId, WorldEventCategory category);
bool valid_mine_shaft_placement(const World& world, glm::ivec2 tile);
bool deep_deposit_available(const World& world, uint32_t depositId, uint16_t team);

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
