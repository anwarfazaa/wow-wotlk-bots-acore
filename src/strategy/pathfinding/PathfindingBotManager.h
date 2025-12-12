/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHFINDINGBOTMANAGER_H
#define _PLAYERBOT_PATHFINDINGBOTMANAGER_H

#include "PathfindingBotContext.h"
#include <shared_mutex>
#include <unordered_map>

class Player;
class PlayerbotAI;
class ExplorationEngine;
class StuckRecoverySystem;
class PathLearner;
class WaypointGenerator;

/**
 * PathfindingBotManager - Singleton orchestrator for autonomous dungeon exploration
 *
 * Manages the lifecycle of pathfinding bots:
 * - Starts/stops pathfinding operations
 * - Tracks state per bot
 * - Coordinates exploration, learning, and waypoint generation
 * - Provides GM command interface
 */
class PathfindingBotManager
{
public:
    static PathfindingBotManager* instance();

    // Initialization
    void Initialize();
    void LoadFromDatabase();

    // Lifecycle management
    bool StartPathfinding(Player* bot, uint32 mapId);
    void StopPathfinding(Player* bot);
    void PausePathfinding(Player* bot);
    void ResumePathfinding(Player* bot);

    // State queries
    bool IsActive(Player* bot) const;
    PathfindingState GetState(Player* bot) const;
    uint32 GetCurrentIteration(Player* bot) const;
    bool IsConverged(Player* bot) const;
    float GetExplorationPercent(Player* bot) const;
    std::string GetStatusString(Player* bot) const;

    // Context access
    PathfindingContext* GetContext(Player* bot);
    const PathfindingContext* GetContext(Player* bot) const;

    // Update (called from PlayerbotAI update loop)
    void Update(Player* bot, uint32 diff);

    // State transitions
    void SetState(Player* bot, PathfindingState newState);
    void OnEnterState(Player* bot, PathfindingState state);
    void OnExitState(Player* bot, PathfindingState state);

    // Event handlers
    void OnBotDeath(Player* bot);
    void OnBotRespawn(Player* bot);
    void OnBossKilled(Player* bot, uint32 bossEntry);
    void OnCombatStart(Player* bot);
    void OnCombatEnd(Player* bot);
    void OnPositionChanged(Player* bot, const Position& newPos);

    // Iteration management
    void CompleteIteration(Player* bot);
    void ResetForNewIteration(Player* bot);
    bool CheckConvergence(Player* bot);

    // Database operations
    void SaveIterationToDatabase(Player* bot, const IterationResult& result);
    void SaveBestRouteToDatabase(uint32 mapId, const IterationResult& bestResult);
    void LoadBestRouteFromDatabase(uint32 mapId);

    // Waypoint promotion
    void PromoteWaypointCandidates(uint32 mapId);
    void ClearLearnedData(uint32 mapId);

    // Configuration
    const PathfindingConfig& GetConfig() const { return m_config; }
    void SetConfig(const PathfindingConfig& config) { m_config = config; }

    // Dungeon info
    std::vector<uint32> GetDungeonBosses(uint32 mapId) const;
    Position GetDungeonEntrance(uint32 mapId) const;
    std::string GetDungeonName(uint32 mapId) const;

private:
    PathfindingBotManager();
    ~PathfindingBotManager();

    // Internal update methods
    void UpdateIdle(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateEntering(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateExploring(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateStuckRecovery(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateCombat(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateBossEncounter(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateExiting(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateResetting(Player* bot, PathfindingContext& ctx, uint32 diff);
    void UpdateAnalyzing(Player* bot, PathfindingContext& ctx, uint32 diff);

    // Helper methods
    void RecordPosition(Player* bot, PathfindingContext& ctx);
    void UpdateBreadcrumbTrail(PathfindingContext& ctx, const Position& pos);
    float CalculateDistance(const Position& a, const Position& b) const;
    bool IsBossTarget(Player* bot) const;
    void EnterDungeon(Player* bot, uint32 mapId);
    void ExitDungeon(Player* bot);
    void ResetInstance(Player* bot);
    std::string SerializePathToJson(const std::vector<Position>& path) const;
    std::vector<Position> DeserializePathFromJson(const std::string& json) const;

    // Components
    std::unique_ptr<ExplorationEngine> m_explorationEngine;
    std::unique_ptr<StuckRecoverySystem> m_stuckRecovery;
    std::unique_ptr<PathLearner> m_pathLearner;
    std::unique_ptr<WaypointGenerator> m_waypointGenerator;

    // Context storage
    std::unordered_map<uint64, PathfindingContext> m_contexts;
    mutable std::shared_mutex m_contextMutex;

    // Configuration
    PathfindingConfig m_config;

    // Dungeon data cache
    struct DungeonInfo
    {
        std::string name;
        Position entrance;
        std::vector<uint32> bossEntries;
    };
    std::unordered_map<uint32, DungeonInfo> m_dungeonInfo;
};

#define sPathfindingBot PathfindingBotManager::instance()

#endif  // _PLAYERBOT_PATHFINDINGBOTMANAGER_H
