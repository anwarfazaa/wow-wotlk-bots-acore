/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "WaypointGenerator.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <algorithm>
#include <cmath>

WaypointGenerator::WaypointGenerator() = default;
WaypointGenerator::~WaypointGenerator() = default;

std::vector<WaypointCandidate> WaypointGenerator::GenerateWaypoints(const PathfindingContext& ctx)
{
    return GenerateWaypoints(ctx.mapId, ctx.pathTaken, ctx.bossesKilled, ctx.combatEncounters);
}

std::vector<WaypointCandidate> WaypointGenerator::GenerateWaypoints(
    uint32 mapId,
    const std::vector<Position>& path,
    const std::vector<uint32>& bossesKilled,
    const std::vector<CombatEncounter>& combatEncounters)
{
    if (path.empty())
        return {};

    // Step 1: Simplify the path
    std::vector<Position> simplifiedPath = SimplifyPath(path);

    LOG_DEBUG("playerbots", "WaypointGenerator: Simplified path from {} to {} points",
              path.size(), simplifiedPath.size());

    // Step 2: Generate waypoint candidates
    std::vector<WaypointCandidate> candidates;
    uint32 waypointIndex = 0;

    for (size_t i = 0; i < simplifiedPath.size(); ++i)
    {
        const Position& pos = simplifiedPath[i];
        bool isNearEntrance = (i == 0);

        WaypointCandidate candidate;
        candidate.mapId = mapId;
        candidate.waypointIndex = waypointIndex++;
        candidate.pos = pos;
        candidate.type = ClassifyPosition(pos, combatEncounters, isNearEntrance);

        // Set boss entry if this is a boss waypoint
        if (candidate.type == PathfindingWaypointType::BOSS)
        {
            candidate.bossEntry = GetBossAtPosition(pos, combatEncounters);
        }

        // Calculate orientation (face toward next waypoint)
        if (i + 1 < simplifiedPath.size())
        {
            candidate.pos.m_orientation = CalculateOrientation(pos, &simplifiedPath[i + 1]);
        }

        // Calculate safe radius
        candidate.safeRadius = CalculateSafeRadius(pos, mapId, combatEncounters);

        // Initial confidence (will be updated with more data)
        candidate.confidence = 0.5f;  // Base confidence
        candidate.timesVisited = 1;

        candidates.push_back(candidate);
    }

    // Step 3: Add boss encounter waypoints if not already included
    for (const auto& encounter : combatEncounters)
    {
        if (!encounter.isBoss)
            continue;

        bool alreadyIncluded = false;
        for (const auto& candidate : candidates)
        {
            if (candidate.type == PathfindingWaypointType::BOSS &&
                candidate.bossEntry == encounter.bossEntry)
            {
                alreadyIncluded = true;
                break;
            }
        }

        if (!alreadyIncluded)
        {
            WaypointCandidate bossCandidate;
            bossCandidate.mapId = mapId;
            bossCandidate.waypointIndex = waypointIndex++;
            bossCandidate.pos = encounter.pos;
            bossCandidate.type = PathfindingWaypointType::BOSS;
            bossCandidate.bossEntry = encounter.bossEntry;
            bossCandidate.safeRadius = 30.0f;
            bossCandidate.confidence = 1.0f;  // Boss locations are always confident
            bossCandidate.timesVisited = 1;

            candidates.push_back(bossCandidate);
        }
    }

    // Step 4: Sort by waypoint index
    std::sort(candidates.begin(), candidates.end(),
              [](const WaypointCandidate& a, const WaypointCandidate& b)
              {
                  return a.waypointIndex < b.waypointIndex;
              });

    // Reassign indices after sorting
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        candidates[i].waypointIndex = i;
    }

    LOG_INFO("playerbots", "WaypointGenerator: Generated {} waypoint candidates for map {}",
             candidates.size(), mapId);

    // Save to database
    SaveCandidatesToDatabase(mapId, candidates);

    return candidates;
}

std::vector<Position> WaypointGenerator::SimplifyPath(const std::vector<Position>& path, float tolerance)
{
    if (path.size() <= 2)
        return path;

    // Use Douglas-Peucker algorithm for path simplification
    std::vector<Position> simplified = DouglasPeucker(path, tolerance);

    // Ensure minimum spacing between waypoints
    std::vector<Position> spaced;
    if (simplified.empty())
        return spaced;

    spaced.push_back(simplified[0]);

    for (size_t i = 1; i < simplified.size(); ++i)
    {
        float dist = PointDistance(spaced.back(), simplified[i]);

        // If too close, skip
        if (dist < m_minWaypointSpacing && i < simplified.size() - 1)
            continue;

        // If too far, add intermediate points
        while (dist > m_maxWaypointSpacing)
        {
            // Add intermediate point
            float t = m_maxWaypointSpacing / dist;
            Position intermediate;
            intermediate.m_positionX = spaced.back().GetPositionX() +
                                       t * (simplified[i].GetPositionX() - spaced.back().GetPositionX());
            intermediate.m_positionY = spaced.back().GetPositionY() +
                                       t * (simplified[i].GetPositionY() - spaced.back().GetPositionY());
            intermediate.m_positionZ = spaced.back().GetPositionZ() +
                                       t * (simplified[i].GetPositionZ() - spaced.back().GetPositionZ());

            spaced.push_back(intermediate);
            dist = PointDistance(spaced.back(), simplified[i]);
        }

        spaced.push_back(simplified[i]);
    }

    return spaced;
}

std::vector<Position> WaypointGenerator::DouglasPeucker(const std::vector<Position>& path, float epsilon)
{
    if (path.size() < 3)
        return path;

    // Find the point with maximum distance from line between first and last
    float maxDist = 0.0f;
    size_t maxIndex = 0;

    for (size_t i = 1; i < path.size() - 1; ++i)
    {
        float dist = PerpendicularDistance(path[i], path.front(), path.back());
        if (dist > maxDist)
        {
            maxDist = dist;
            maxIndex = i;
        }
    }

    std::vector<Position> result;

    // If max distance is greater than epsilon, recursively simplify
    if (maxDist > epsilon)
    {
        // Split and recursively simplify
        std::vector<Position> first(path.begin(), path.begin() + maxIndex + 1);
        std::vector<Position> second(path.begin() + maxIndex, path.end());

        std::vector<Position> result1 = DouglasPeucker(first, epsilon);
        std::vector<Position> result2 = DouglasPeucker(second, epsilon);

        // Combine results (excluding duplicate point at maxIndex)
        result = result1;
        result.insert(result.end(), result2.begin() + 1, result2.end());
    }
    else
    {
        // All points between first and last are within epsilon
        result.push_back(path.front());
        result.push_back(path.back());
    }

    return result;
}

float WaypointGenerator::PerpendicularDistance(const Position& point,
                                               const Position& lineStart,
                                               const Position& lineEnd) const
{
    float dx = lineEnd.GetPositionX() - lineStart.GetPositionX();
    float dy = lineEnd.GetPositionY() - lineStart.GetPositionY();
    float dz = lineEnd.GetPositionZ() - lineStart.GetPositionZ();

    float lineLenSq = dx * dx + dy * dy + dz * dz;
    if (lineLenSq == 0.0f)
        return PointDistance(point, lineStart);

    // Project point onto line
    float t = ((point.GetPositionX() - lineStart.GetPositionX()) * dx +
               (point.GetPositionY() - lineStart.GetPositionY()) * dy +
               (point.GetPositionZ() - lineStart.GetPositionZ()) * dz) / lineLenSq;

    t = std::max(0.0f, std::min(1.0f, t));

    Position projection;
    projection.m_positionX = lineStart.GetPositionX() + t * dx;
    projection.m_positionY = lineStart.GetPositionY() + t * dy;
    projection.m_positionZ = lineStart.GetPositionZ() + t * dz;

    return PointDistance(point, projection);
}

float WaypointGenerator::PointDistance(const Position& a, const Position& b) const
{
    float dx = a.GetPositionX() - b.GetPositionX();
    float dy = a.GetPositionY() - b.GetPositionY();
    float dz = a.GetPositionZ() - b.GetPositionZ();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

PathfindingWaypointType WaypointGenerator::ClassifyPosition(
    const Position& pos,
    const std::vector<CombatEncounter>& encounters,
    bool nearEntrance)
{
    // Check for boss proximity
    if (IsNearBossEncounter(pos, encounters))
        return PathfindingWaypointType::BOSS;

    // Check for trash proximity
    if (IsNearTrashEncounter(pos, encounters))
        return PathfindingWaypointType::TRASH_PACK;

    // Check if this is a safe spot
    if (IsSafeSpot(pos, encounters))
        return PathfindingWaypointType::SAFE_SPOT;

    // Default to path waypoint
    return PathfindingWaypointType::PATH;
}

bool WaypointGenerator::IsNearBossEncounter(const Position& pos,
                                            const std::vector<CombatEncounter>& encounters) const
{
    for (const auto& encounter : encounters)
    {
        if (!encounter.isBoss)
            continue;

        float dist = PointDistance(pos, encounter.pos);
        if (dist <= m_bossProximityRadius)
            return true;
    }
    return false;
}

bool WaypointGenerator::IsNearTrashEncounter(const Position& pos,
                                             const std::vector<CombatEncounter>& encounters) const
{
    for (const auto& encounter : encounters)
    {
        if (encounter.isBoss)
            continue;

        float dist = PointDistance(pos, encounter.pos);
        if (dist <= m_trashProximityRadius)
            return true;
    }
    return false;
}

uint32 WaypointGenerator::GetBossAtPosition(const Position& pos,
                                            const std::vector<CombatEncounter>& encounters) const
{
    for (const auto& encounter : encounters)
    {
        if (!encounter.isBoss)
            continue;

        float dist = PointDistance(pos, encounter.pos);
        if (dist <= m_bossProximityRadius)
            return encounter.bossEntry;
    }
    return 0;
}

bool WaypointGenerator::IsSafeSpot(const Position& pos, const std::vector<CombatEncounter>& encounters) const
{
    // A safe spot is far from all combat encounters
    float minDist = DistanceToNearestCombat(pos, encounters);
    return minDist > 40.0f;  // At least 40 yards from any combat
}

float WaypointGenerator::DistanceToNearestCombat(const Position& pos,
                                                 const std::vector<CombatEncounter>& encounters) const
{
    float minDist = std::numeric_limits<float>::max();

    for (const auto& encounter : encounters)
    {
        float dist = PointDistance(pos, encounter.pos);
        minDist = std::min(minDist, dist);
    }

    return minDist;
}

float WaypointGenerator::CalculateConfidence(const Position& pos,
                                             const std::vector<std::vector<Position>>& allPaths)
{
    if (allPaths.empty())
        return 0.5f;

    // Count how many paths pass near this position
    uint32 hitCount = 0;
    const float proximityRadius = 10.0f;

    for (const auto& path : allPaths)
    {
        for (const Position& pathPos : path)
        {
            if (PointDistance(pos, pathPos) <= proximityRadius)
            {
                hitCount++;
                break;  // Count each path only once
            }
        }
    }

    return static_cast<float>(hitCount) / static_cast<float>(allPaths.size());
}

void WaypointGenerator::UpdateCandidateConfidence(WaypointCandidate& candidate,
                                                  const std::vector<IterationResult>& iterations)
{
    if (iterations.empty())
        return;

    std::vector<std::vector<Position>> allPaths;
    for (const auto& iter : iterations)
    {
        allPaths.push_back(iter.path);
    }

    candidate.confidence = CalculateConfidence(candidate.pos, allPaths);
    candidate.timesVisited = iterations.size();
}

float WaypointGenerator::CalculateSafeRadius(const Position& pos, uint32 mapId,
                                             const std::vector<CombatEncounter>& encounters)
{
    // Default safe radius
    float safeRadius = 5.0f;

    // Larger radius for boss encounters
    if (IsNearBossEncounter(pos, encounters))
        safeRadius = 30.0f;
    // Medium radius for trash
    else if (IsNearTrashEncounter(pos, encounters))
        safeRadius = 15.0f;
    // Small radius for path waypoints
    else
        safeRadius = 5.0f;

    return safeRadius;
}

float WaypointGenerator::CalculateOrientation(const Position& current, const Position* next)
{
    if (!next)
        return 0.0f;

    float dx = next->GetPositionX() - current.GetPositionX();
    float dy = next->GetPositionY() - current.GetPositionY();

    return std::atan2(dy, dx);
}

void WaypointGenerator::SaveCandidatesToDatabase(uint32 mapId, const std::vector<WaypointCandidate>& candidates)
{
    // Clear existing candidates for this map
    CharacterDatabase.Execute("DELETE FROM playerbots_pathfinding_waypoint_candidates WHERE map_id = {}",
                              mapId);

    // Insert new candidates
    for (const auto& candidate : candidates)
    {
        CharacterDatabase.Execute(
            "INSERT INTO playerbots_pathfinding_waypoint_candidates "
            "(map_id, waypoint_index, x, y, z, orientation, waypoint_type, boss_entry, "
            "trash_pack_id, safe_radius, confidence, times_visited) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
            candidate.mapId, candidate.waypointIndex,
            candidate.pos.GetPositionX(), candidate.pos.GetPositionY(), candidate.pos.GetPositionZ(),
            candidate.pos.GetOrientation(), static_cast<uint8>(candidate.type),
            candidate.bossEntry, candidate.trashPackId, candidate.safeRadius,
            candidate.confidence, candidate.timesVisited);
    }

    LOG_INFO("playerbots", "WaypointGenerator: Saved {} waypoint candidates to database for map {}",
             candidates.size(), mapId);
}

void WaypointGenerator::PromoteToWaypoints(uint32 mapId, float minConfidence)
{
    // Load candidates with sufficient confidence
    QueryResult result = CharacterDatabase.Query(
        "SELECT waypoint_index, x, y, z, orientation, waypoint_type, boss_entry, safe_radius "
        "FROM playerbots_pathfinding_waypoint_candidates "
        "WHERE map_id = {} AND confidence >= {} AND promoted = 0 "
        "ORDER BY waypoint_index",
        mapId, minConfidence);

    if (!result)
    {
        LOG_WARN("playerbots", "WaypointGenerator: No candidates to promote for map {} "
                 "(confidence threshold: {})", mapId, minConfidence);
        return;
    }

    uint32 promoted = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 waypointIndex = fields[0].Get<uint32>();
        float x = fields[1].Get<float>();
        float y = fields[2].Get<float>();
        float z = fields[3].Get<float>();
        float orientation = fields[4].Get<float>();
        uint8 waypointType = fields[5].Get<uint8>();
        uint32 bossEntry = fields[6].Get<uint32>();
        float safeRadius = fields[7].Get<float>();

        // Insert into main waypoints table
        CharacterDatabase.Execute(
            "INSERT INTO playerbots_dungeon_waypoints "
            "(map_id, waypoint_index, x, y, z, orientation, waypoint_type, boss_entry, "
            "safe_radius, wait_for_group, requires_clear) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE x = {}, y = {}, z = {}, orientation = {}",
            mapId, waypointIndex, x, y, z, orientation, waypointType, bossEntry,
            safeRadius, (waypointType == 1 ? 1 : 0), (waypointType == 2 ? 1 : 0),
            x, y, z, orientation);

        // Mark as promoted
        CharacterDatabase.Execute(
            "UPDATE playerbots_pathfinding_waypoint_candidates SET promoted = 1 "
            "WHERE map_id = {} AND waypoint_index = {}",
            mapId, waypointIndex);

        promoted++;
    }
    while (result->NextRow());

    LOG_INFO("playerbots", "WaypointGenerator: Promoted {} waypoints to main table for map {}",
             promoted, mapId);
}

std::vector<WaypointCandidate> WaypointGenerator::LoadCandidatesFromDatabase(uint32 mapId)
{
    std::vector<WaypointCandidate> candidates;

    QueryResult result = CharacterDatabase.Query(
        "SELECT waypoint_index, x, y, z, orientation, waypoint_type, boss_entry, "
        "trash_pack_id, safe_radius, confidence, times_visited "
        "FROM playerbots_pathfinding_waypoint_candidates "
        "WHERE map_id = {} ORDER BY waypoint_index",
        mapId);

    if (!result)
        return candidates;

    do
    {
        Field* fields = result->Fetch();

        WaypointCandidate candidate;
        candidate.mapId = mapId;
        candidate.waypointIndex = fields[0].Get<uint32>();
        candidate.pos.m_positionX = fields[1].Get<float>();
        candidate.pos.m_positionY = fields[2].Get<float>();
        candidate.pos.m_positionZ = fields[3].Get<float>();
        candidate.pos.m_orientation = fields[4].Get<float>();
        candidate.type = static_cast<PathfindingWaypointType>(fields[5].Get<uint8>());
        candidate.bossEntry = fields[6].Get<uint32>();
        candidate.trashPackId = fields[7].Get<uint32>();
        candidate.safeRadius = fields[8].Get<float>();
        candidate.confidence = fields[9].Get<float>();
        candidate.timesVisited = fields[10].Get<uint32>();

        candidates.push_back(candidate);
    }
    while (result->NextRow());

    return candidates;
}

void WaypointGenerator::ClearCandidates(uint32 mapId)
{
    CharacterDatabase.Execute("DELETE FROM playerbots_pathfinding_waypoint_candidates WHERE map_id = {}",
                              mapId);
    LOG_INFO("playerbots", "WaypointGenerator: Cleared all waypoint candidates for map {}", mapId);
}
