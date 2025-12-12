/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_DUNGEONNAVIGATOR_H
#define _PLAYERBOT_DUNGEONNAVIGATOR_H

#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <optional>

#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"

class Player;
class Unit;
class Group;

/**
 * WaypointType - Types of dungeon waypoints
 */
enum class WaypointType : uint8
{
    PATH = 0,           // Regular navigation point
    BOSS = 1,           // Boss encounter location
    TRASH_PACK = 2,     // Trash pack pull location
    SAFE_SPOT = 3,      // Safe resting/regrouping area
    EVENT = 4           // Scripted event trigger point
};

/**
 * DungeonWaypoint - Loaded from database
 */
struct DungeonWaypoint
{
    uint32 id = 0;
    uint32 mapId = 0;
    std::string dungeonName;
    uint16 waypointIndex = 0;
    Position position;
    WaypointType type = WaypointType::PATH;
    uint32 bossEntry = 0;
    uint16 trashPackId = 0;
    bool requiresClear = false;
    float safeRadius = 5.0f;
    bool waitForGroup = false;
    bool isOptional = false;
    std::string description;

    bool IsBoss() const { return type == WaypointType::BOSS; }
    bool IsTrash() const { return type == WaypointType::TRASH_PACK; }
    bool IsSafeSpot() const { return type == WaypointType::SAFE_SPOT; }
    bool IsEvent() const { return type == WaypointType::EVENT; }

    float GetDistanceTo(const Position& pos) const
    {
        float dx = position.GetPositionX() - pos.GetPositionX();
        float dy = position.GetPositionY() - pos.GetPositionY();
        float dz = position.GetPositionZ() - pos.GetPositionZ();
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
};

/**
 * DungeonPath - Complete path through a dungeon
 */
struct DungeonPath
{
    uint32 mapId = 0;
    std::string dungeonName;
    std::vector<DungeonWaypoint> waypoints;

    const DungeonWaypoint* GetWaypoint(uint16 index) const
    {
        if (index < waypoints.size())
            return &waypoints[index];
        return nullptr;
    }

    const DungeonWaypoint* GetNextBoss(uint16 fromIndex) const
    {
        for (size_t i = fromIndex; i < waypoints.size(); ++i)
        {
            if (waypoints[i].IsBoss())
                return &waypoints[i];
        }
        return nullptr;
    }

    const DungeonWaypoint* GetNextSafeSpot(uint16 fromIndex) const
    {
        for (size_t i = fromIndex; i < waypoints.size(); ++i)
        {
            if (waypoints[i].IsSafeSpot())
                return &waypoints[i];
        }
        return nullptr;
    }

    uint16 GetWaypointCount() const { return static_cast<uint16>(waypoints.size()); }

    bool IsValid() const { return !waypoints.empty() && mapId > 0; }
};

/**
 * GroupProgress - Track group's progress through dungeon
 */
struct GroupProgress
{
    uint32 groupId = 0;
    uint32 mapId = 0;
    uint16 currentWaypointIndex = 0;
    std::vector<uint32> killedBosses;           // Boss entries that have been killed
    std::vector<uint16> clearedTrashPacks;      // Trash pack IDs that have been cleared
    uint32 lastUpdateTime = 0;
    bool isComplete = false;

    bool HasKilledBoss(uint32 bossEntry) const
    {
        return std::find(killedBosses.begin(), killedBosses.end(), bossEntry) != killedBosses.end();
    }

    bool HasClearedTrash(uint16 trashPackId) const
    {
        return std::find(clearedTrashPacks.begin(), clearedTrashPacks.end(), trashPackId) != clearedTrashPacks.end();
    }

    void MarkBossKilled(uint32 bossEntry)
    {
        if (!HasKilledBoss(bossEntry))
            killedBosses.push_back(bossEntry);
    }

    void MarkTrashCleared(uint16 trashPackId)
    {
        if (!HasClearedTrash(trashPackId))
            clearedTrashPacks.push_back(trashPackId);
    }

    float GetProgressPercent(uint16 totalWaypoints) const
    {
        if (totalWaypoints == 0)
            return 0.0f;
        return (static_cast<float>(currentWaypointIndex) / static_cast<float>(totalWaypoints)) * 100.0f;
    }
};

/**
 * NavigationResult - Result of pathfinding request
 */
struct NavigationResult
{
    bool found = false;
    std::vector<Position> path;
    float totalDistance = 0.0f;
    uint16 targetWaypointIndex = 0;
    const DungeonWaypoint* targetWaypoint = nullptr;

    operator bool() const { return found; }
};

/**
 * DungeonNavigator - Singleton for dungeon navigation
 *
 * Loads dungeon waypoints from database and provides pathfinding
 * through dungeons for tank-led navigation.
 */
class DungeonNavigator
{
public:
    static DungeonNavigator* instance()
    {
        static DungeonNavigator instance;
        return &instance;
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * Load all dungeon waypoints from database
     * Called during server startup
     */
    void Initialize();

    /**
     * Reload waypoints (for hot-reloading during development)
     */
    void Reload();

    /**
     * Check if navigator is initialized
     */
    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Path Queries
    // =========================================================================

    /**
     * Get the dungeon path for a map
     */
    const DungeonPath* GetDungeonPath(uint32 mapId) const;

    /**
     * Check if a map has dungeon navigation data
     */
    bool HasDungeonPath(uint32 mapId) const;

    /**
     * Get all dungeon map IDs that have paths defined
     */
    std::vector<uint32> GetSupportedDungeons() const;

    // =========================================================================
    // Navigation
    // =========================================================================

    /**
     * Get the path from current position to next waypoint
     */
    NavigationResult GetPathToNextWaypoint(uint32 mapId, const Position& currentPos,
                                            uint16 currentWaypointIndex) const;

    /**
     * Get the path from current position to next boss
     */
    NavigationResult GetPathToNextBoss(uint32 mapId, const Position& currentPos,
                                        uint16 currentWaypointIndex) const;

    /**
     * Get the path from current position to nearest safe spot
     */
    NavigationResult GetPathToNearestSafeSpot(uint32 mapId, const Position& currentPos) const;

    /**
     * Find the closest waypoint to a position
     */
    const DungeonWaypoint* FindNearestWaypoint(uint32 mapId, const Position& pos,
                                                float maxDistance = 100.0f) const;

    /**
     * Find the waypoint index closest to a position
     */
    std::optional<uint16> FindNearestWaypointIndex(uint32 mapId, const Position& pos,
                                                    float maxDistance = 100.0f) const;

    /**
     * Check if position is at a waypoint (within safe radius)
     */
    bool IsAtWaypoint(uint32 mapId, const Position& pos, uint16 waypointIndex) const;

    /**
     * Check if position is at any waypoint
     */
    const DungeonWaypoint* GetWaypointAtPosition(uint32 mapId, const Position& pos,
                                                   float tolerance = 5.0f) const;

    // =========================================================================
    // Progress Tracking
    // =========================================================================

    /**
     * Get or create progress tracking for a group
     */
    GroupProgress* GetGroupProgress(uint32 groupId, uint32 mapId);

    /**
     * Get existing progress (nullptr if none)
     */
    const GroupProgress* GetGroupProgress(uint32 groupId) const;

    /**
     * Update group's current waypoint
     */
    void SetGroupWaypoint(uint32 groupId, uint16 waypointIndex);

    /**
     * Record a boss kill
     */
    void RecordBossKill(uint32 groupId, uint32 bossEntry);

    /**
     * Record trash clear
     */
    void RecordTrashClear(uint32 groupId, uint16 trashPackId);

    /**
     * Mark dungeon as complete
     */
    void MarkDungeonComplete(uint32 groupId);

    /**
     * Reset progress for a group
     */
    void ResetGroupProgress(uint32 groupId);

    /**
     * Get progress for a player's group
     */
    GroupProgress* GetPlayerGroupProgress(Player* player);

    // =========================================================================
    // Boss & Trash Detection
    // =========================================================================

    /**
     * Get waypoint for a specific boss entry
     */
    const DungeonWaypoint* GetBossWaypoint(uint32 mapId, uint32 bossEntry) const;

    /**
     * Get all boss waypoints in a dungeon
     */
    std::vector<const DungeonWaypoint*> GetAllBossWaypoints(uint32 mapId) const;

    /**
     * Get waypoints for a trash pack
     */
    const DungeonWaypoint* GetTrashPackWaypoint(uint32 mapId, uint16 trashPackId) const;

    /**
     * Check if a unit is a dungeon boss
     */
    bool IsDungeonBoss(uint32 mapId, Unit* unit) const;

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * Calculate total distance of a path
     */
    static float CalculatePathDistance(const std::vector<Position>& path);

    /**
     * Get readable dungeon name
     */
    std::string GetDungeonName(uint32 mapId) const;

    // =========================================================================
    // Maintenance
    // =========================================================================

    void Update(uint32 diff);
    void Clear();
    void ClearProgress();

    /**
     * Get statistics
     */
    size_t GetLoadedDungeonCount() const;
    size_t GetTotalWaypointCount() const;
    size_t GetActiveProgressCount() const;

private:
    DungeonNavigator() : m_initialized(false), m_lastCleanupTime(0) {}
    ~DungeonNavigator() = default;
    DungeonNavigator(const DungeonNavigator&) = delete;
    DungeonNavigator& operator=(const DungeonNavigator&) = delete;

    void LoadWaypointsFromDB();
    void CleanupStaleProgress();

    // Storage
    std::unordered_map<uint32, DungeonPath> m_dungeonPaths;        // mapId -> path
    std::unordered_map<uint32, GroupProgress> m_groupProgress;    // groupId -> progress

    mutable std::shared_mutex m_pathMutex;
    mutable std::shared_mutex m_progressMutex;

    bool m_initialized;
    uint32 m_lastCleanupTime;

    static constexpr uint32 PROGRESS_CLEANUP_INTERVAL_MS = 300000;  // 5 minutes
    static constexpr uint32 STALE_PROGRESS_AGE_MS = 3600000;        // 1 hour
};

#define sDungeonNavigator DungeonNavigator::instance()

#endif
