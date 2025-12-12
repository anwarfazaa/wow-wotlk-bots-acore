/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "DungeonNavigator.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "Playerbots.h"
#include "Timer.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>

// =========================================================================
// Initialization
// =========================================================================

void DungeonNavigator::Initialize()
{
    if (m_initialized)
        return;

    LOG_INFO("playerbots", "DungeonNavigator: Loading dungeon waypoints...");
    LoadWaypointsFromDB();
    m_initialized = true;

    LOG_INFO("playerbots", "DungeonNavigator: Loaded {} dungeons with {} total waypoints",
             GetLoadedDungeonCount(), GetTotalWaypointCount());
}

void DungeonNavigator::Reload()
{
    LOG_INFO("playerbots", "DungeonNavigator: Reloading dungeon waypoints...");

    {
        std::unique_lock<std::shared_mutex> lock(m_pathMutex);
        m_dungeonPaths.clear();
    }

    LoadWaypointsFromDB();

    LOG_INFO("playerbots", "DungeonNavigator: Reloaded {} dungeons with {} total waypoints",
             GetLoadedDungeonCount(), GetTotalWaypointCount());
}

void DungeonNavigator::LoadWaypointsFromDB()
{
    std::unique_lock<std::shared_mutex> lock(m_pathMutex);

    QueryResult result = PlayerbotsDatabase.Query(
        "SELECT id, map_id, dungeon_name, waypoint_index, x, y, z, orientation, "
        "waypoint_type, boss_entry, trash_pack_id, requires_clear, safe_radius, "
        "wait_for_group, is_optional, description "
        "FROM playerbots_dungeon_waypoints "
        "ORDER BY map_id, waypoint_index");

    if (!result)
    {
        LOG_WARN("playerbots", "DungeonNavigator: No dungeon waypoints found in database");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        DungeonWaypoint waypoint;
        waypoint.id = fields[0].Get<uint32>();
        waypoint.mapId = fields[1].Get<uint32>();
        waypoint.dungeonName = fields[2].Get<std::string>();
        waypoint.waypointIndex = fields[3].Get<uint16>();
        waypoint.position.Relocate(
            fields[4].Get<float>(),
            fields[5].Get<float>(),
            fields[6].Get<float>(),
            fields[7].Get<float>());
        waypoint.type = static_cast<WaypointType>(fields[8].Get<uint8>());
        waypoint.bossEntry = fields[9].Get<uint32>();
        waypoint.trashPackId = fields[10].Get<uint16>();
        waypoint.requiresClear = fields[11].Get<bool>();
        waypoint.safeRadius = fields[12].Get<float>();
        waypoint.waitForGroup = fields[13].Get<bool>();
        waypoint.isOptional = fields[14].Get<bool>();
        waypoint.description = fields[15].Get<std::string>();

        // Get or create path for this dungeon
        DungeonPath& path = m_dungeonPaths[waypoint.mapId];
        if (path.waypoints.empty())
        {
            path.mapId = waypoint.mapId;
            path.dungeonName = waypoint.dungeonName;
        }

        path.waypoints.push_back(waypoint);

    } while (result->NextRow());

    // Sort waypoints by index for each dungeon
    for (auto& [mapId, path] : m_dungeonPaths)
    {
        std::sort(path.waypoints.begin(), path.waypoints.end(),
            [](const DungeonWaypoint& a, const DungeonWaypoint& b) {
                return a.waypointIndex < b.waypointIndex;
            });
    }
}

// =========================================================================
// Path Queries
// =========================================================================

const DungeonPath* DungeonNavigator::GetDungeonPath(uint32 mapId) const
{
    std::shared_lock<std::shared_mutex> lock(m_pathMutex);

    auto it = m_dungeonPaths.find(mapId);
    if (it != m_dungeonPaths.end())
        return &it->second;

    return nullptr;
}

bool DungeonNavigator::HasDungeonPath(uint32 mapId) const
{
    std::shared_lock<std::shared_mutex> lock(m_pathMutex);
    return m_dungeonPaths.find(mapId) != m_dungeonPaths.end();
}

std::vector<uint32> DungeonNavigator::GetSupportedDungeons() const
{
    std::shared_lock<std::shared_mutex> lock(m_pathMutex);

    std::vector<uint32> result;
    result.reserve(m_dungeonPaths.size());

    for (const auto& [mapId, path] : m_dungeonPaths)
    {
        result.push_back(mapId);
    }

    return result;
}

// =========================================================================
// Navigation
// =========================================================================

NavigationResult DungeonNavigator::GetPathToNextWaypoint(uint32 mapId, const Position& currentPos,
                                                          uint16 currentWaypointIndex) const
{
    NavigationResult result;

    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path || currentWaypointIndex >= path->GetWaypointCount())
        return result;

    uint16 nextIndex = currentWaypointIndex + 1;
    if (nextIndex >= path->GetWaypointCount())
        return result;  // At end of dungeon

    const DungeonWaypoint* nextWaypoint = path->GetWaypoint(nextIndex);
    if (!nextWaypoint)
        return result;

    result.found = true;
    result.targetWaypointIndex = nextIndex;
    result.targetWaypoint = nextWaypoint;

    // Build path from current position through intermediate waypoints if needed
    result.path.push_back(currentPos);

    // Add intermediate waypoints
    for (uint16 i = currentWaypointIndex + 1; i <= nextIndex; ++i)
    {
        const DungeonWaypoint* wp = path->GetWaypoint(i);
        if (wp)
            result.path.push_back(wp->position);
    }

    result.totalDistance = CalculatePathDistance(result.path);

    return result;
}

NavigationResult DungeonNavigator::GetPathToNextBoss(uint32 mapId, const Position& currentPos,
                                                      uint16 currentWaypointIndex) const
{
    NavigationResult result;

    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return result;

    // Find next boss waypoint
    const DungeonWaypoint* bossWaypoint = path->GetNextBoss(currentWaypointIndex + 1);
    if (!bossWaypoint)
        return result;

    result.found = true;
    result.targetWaypointIndex = bossWaypoint->waypointIndex;
    result.targetWaypoint = bossWaypoint;

    // Build path including all waypoints between current and boss
    result.path.push_back(currentPos);

    for (uint16 i = currentWaypointIndex + 1; i <= bossWaypoint->waypointIndex; ++i)
    {
        const DungeonWaypoint* wp = path->GetWaypoint(i);
        if (wp)
            result.path.push_back(wp->position);
    }

    result.totalDistance = CalculatePathDistance(result.path);

    return result;
}

NavigationResult DungeonNavigator::GetPathToNearestSafeSpot(uint32 mapId, const Position& currentPos) const
{
    NavigationResult result;

    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return result;

    // Find nearest safe spot
    const DungeonWaypoint* nearestSafe = nullptr;
    float nearestDist = std::numeric_limits<float>::max();

    for (const auto& wp : path->waypoints)
    {
        if (wp.IsSafeSpot())
        {
            float dist = wp.GetDistanceTo(currentPos);
            if (dist < nearestDist)
            {
                nearestDist = dist;
                nearestSafe = &wp;
            }
        }
    }

    if (!nearestSafe)
        return result;

    result.found = true;
    result.targetWaypointIndex = nearestSafe->waypointIndex;
    result.targetWaypoint = nearestSafe;
    result.path.push_back(currentPos);
    result.path.push_back(nearestSafe->position);
    result.totalDistance = nearestDist;

    return result;
}

const DungeonWaypoint* DungeonNavigator::FindNearestWaypoint(uint32 mapId, const Position& pos,
                                                              float maxDistance) const
{
    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return nullptr;

    const DungeonWaypoint* nearest = nullptr;
    float nearestDist = maxDistance;

    for (const auto& wp : path->waypoints)
    {
        float dist = wp.GetDistanceTo(pos);
        if (dist < nearestDist)
        {
            nearestDist = dist;
            nearest = &wp;
        }
    }

    return nearest;
}

std::optional<uint16> DungeonNavigator::FindNearestWaypointIndex(uint32 mapId, const Position& pos,
                                                                   float maxDistance) const
{
    const DungeonWaypoint* wp = FindNearestWaypoint(mapId, pos, maxDistance);
    if (wp)
        return wp->waypointIndex;
    return std::nullopt;
}

bool DungeonNavigator::IsAtWaypoint(uint32 mapId, const Position& pos, uint16 waypointIndex) const
{
    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return false;

    const DungeonWaypoint* wp = path->GetWaypoint(waypointIndex);
    if (!wp)
        return false;

    return wp->GetDistanceTo(pos) <= wp->safeRadius;
}

const DungeonWaypoint* DungeonNavigator::GetWaypointAtPosition(uint32 mapId, const Position& pos,
                                                                 float tolerance) const
{
    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return nullptr;

    for (const auto& wp : path->waypoints)
    {
        if (wp.GetDistanceTo(pos) <= tolerance)
            return &wp;
    }

    return nullptr;
}

// =========================================================================
// Progress Tracking
// =========================================================================

GroupProgress* DungeonNavigator::GetGroupProgress(uint32 groupId, uint32 mapId)
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);

    auto it = m_groupProgress.find(groupId);
    if (it != m_groupProgress.end())
    {
        it->second.lastUpdateTime = getMSTime();
        return &it->second;
    }

    // Create new progress
    GroupProgress& progress = m_groupProgress[groupId];
    progress.groupId = groupId;
    progress.mapId = mapId;
    progress.currentWaypointIndex = 0;
    progress.lastUpdateTime = getMSTime();

    return &progress;
}

const GroupProgress* DungeonNavigator::GetGroupProgress(uint32 groupId) const
{
    std::shared_lock<std::shared_mutex> lock(m_progressMutex);

    auto it = m_groupProgress.find(groupId);
    if (it != m_groupProgress.end())
        return &it->second;

    return nullptr;
}

void DungeonNavigator::SetGroupWaypoint(uint32 groupId, uint16 waypointIndex)
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);

    auto it = m_groupProgress.find(groupId);
    if (it != m_groupProgress.end())
    {
        it->second.currentWaypointIndex = waypointIndex;
        it->second.lastUpdateTime = getMSTime();
    }
}

void DungeonNavigator::RecordBossKill(uint32 groupId, uint32 bossEntry)
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);

    auto it = m_groupProgress.find(groupId);
    if (it != m_groupProgress.end())
    {
        it->second.MarkBossKilled(bossEntry);
        it->second.lastUpdateTime = getMSTime();
    }
}

void DungeonNavigator::RecordTrashClear(uint32 groupId, uint16 trashPackId)
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);

    auto it = m_groupProgress.find(groupId);
    if (it != m_groupProgress.end())
    {
        it->second.MarkTrashCleared(trashPackId);
        it->second.lastUpdateTime = getMSTime();
    }
}

void DungeonNavigator::MarkDungeonComplete(uint32 groupId)
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);

    auto it = m_groupProgress.find(groupId);
    if (it != m_groupProgress.end())
    {
        it->second.isComplete = true;
        it->second.lastUpdateTime = getMSTime();
    }
}

void DungeonNavigator::ResetGroupProgress(uint32 groupId)
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);
    m_groupProgress.erase(groupId);
}

GroupProgress* DungeonNavigator::GetPlayerGroupProgress(Player* player)
{
    if (!player)
        return nullptr;

    Group* group = player->GetGroup();
    if (!group)
        return nullptr;

    return GetGroupProgress(group->GetGUID().GetCounter(), player->GetMapId());
}

// =========================================================================
// Boss & Trash Detection
// =========================================================================

const DungeonWaypoint* DungeonNavigator::GetBossWaypoint(uint32 mapId, uint32 bossEntry) const
{
    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return nullptr;

    for (const auto& wp : path->waypoints)
    {
        if (wp.IsBoss() && wp.bossEntry == bossEntry)
            return &wp;
    }

    return nullptr;
}

std::vector<const DungeonWaypoint*> DungeonNavigator::GetAllBossWaypoints(uint32 mapId) const
{
    std::vector<const DungeonWaypoint*> result;

    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return result;

    for (const auto& wp : path->waypoints)
    {
        if (wp.IsBoss())
            result.push_back(&wp);
    }

    return result;
}

const DungeonWaypoint* DungeonNavigator::GetTrashPackWaypoint(uint32 mapId, uint16 trashPackId) const
{
    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return nullptr;

    for (const auto& wp : path->waypoints)
    {
        if (wp.IsTrash() && wp.trashPackId == trashPackId)
            return &wp;
    }

    return nullptr;
}

bool DungeonNavigator::IsDungeonBoss(uint32 mapId, Unit* unit) const
{
    if (!unit)
        return false;

    const DungeonPath* path = GetDungeonPath(mapId);
    if (!path)
        return false;

    uint32 entry = unit->GetEntry();
    for (const auto& wp : path->waypoints)
    {
        if (wp.IsBoss() && wp.bossEntry == entry)
            return true;
    }

    return false;
}

// =========================================================================
// Utility
// =========================================================================

float DungeonNavigator::CalculatePathDistance(const std::vector<Position>& path)
{
    if (path.size() < 2)
        return 0.0f;

    float totalDist = 0.0f;
    for (size_t i = 1; i < path.size(); ++i)
    {
        float dx = path[i].GetPositionX() - path[i-1].GetPositionX();
        float dy = path[i].GetPositionY() - path[i-1].GetPositionY();
        float dz = path[i].GetPositionZ() - path[i-1].GetPositionZ();
        totalDist += std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    return totalDist;
}

std::string DungeonNavigator::GetDungeonName(uint32 mapId) const
{
    const DungeonPath* path = GetDungeonPath(mapId);
    if (path)
        return path->dungeonName;
    return "";
}

// =========================================================================
// Maintenance
// =========================================================================

void DungeonNavigator::Update(uint32 diff)
{
    uint32 now = getMSTime();
    if (getMSTimeDiff(m_lastCleanupTime, now) >= PROGRESS_CLEANUP_INTERVAL_MS)
    {
        m_lastCleanupTime = now;
        CleanupStaleProgress();
    }
}

void DungeonNavigator::CleanupStaleProgress()
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);

    uint32 now = getMSTime();
    std::vector<uint32> toRemove;

    for (const auto& [groupId, progress] : m_groupProgress)
    {
        if (getMSTimeDiff(progress.lastUpdateTime, now) > STALE_PROGRESS_AGE_MS)
        {
            toRemove.push_back(groupId);
        }
    }

    for (uint32 groupId : toRemove)
    {
        m_groupProgress.erase(groupId);
    }

    if (!toRemove.empty())
    {
        LOG_DEBUG("playerbots", "DungeonNavigator: Cleaned up {} stale progress entries", toRemove.size());
    }
}

void DungeonNavigator::Clear()
{
    {
        std::unique_lock<std::shared_mutex> lock(m_pathMutex);
        m_dungeonPaths.clear();
    }
    {
        std::unique_lock<std::shared_mutex> lock(m_progressMutex);
        m_groupProgress.clear();
    }
    m_initialized = false;
}

void DungeonNavigator::ClearProgress()
{
    std::unique_lock<std::shared_mutex> lock(m_progressMutex);
    m_groupProgress.clear();
}

size_t DungeonNavigator::GetLoadedDungeonCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_pathMutex);
    return m_dungeonPaths.size();
}

size_t DungeonNavigator::GetTotalWaypointCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_pathMutex);

    size_t count = 0;
    for (const auto& [mapId, path] : m_dungeonPaths)
    {
        count += path.waypoints.size();
    }
    return count;
}

size_t DungeonNavigator::GetActiveProgressCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_progressMutex);
    return m_groupProgress.size();
}
