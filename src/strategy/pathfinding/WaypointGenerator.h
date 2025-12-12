/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_WAYPOINTGENERATOR_H
#define _PLAYERBOT_WAYPOINTGENERATOR_H

#include "PathfindingBotContext.h"
#include <vector>

/**
 * WaypointGenerator - Generates waypoint candidates from learned paths
 *
 * Process:
 * 1. Simplify path by removing redundant points
 * 2. Classify positions (boss, trash, safe spot, path)
 * 3. Calculate confidence scores based on iteration consistency
 * 4. Generate waypoint candidates for the dungeon
 * 5. Optionally promote high-confidence candidates to main table
 *
 * Output:
 * - Waypoint candidates stored in playerbots_pathfinding_waypoint_candidates
 * - Can be promoted to playerbots_dungeon_waypoints for use by TankLeadStrategy
 */
class WaypointGenerator
{
public:
    WaypointGenerator();
    ~WaypointGenerator();

    // Main generation interface
    std::vector<WaypointCandidate> GenerateWaypoints(const PathfindingContext& ctx);
    std::vector<WaypointCandidate> GenerateWaypoints(uint32 mapId,
                                                      const std::vector<Position>& path,
                                                      const std::vector<uint32>& bossesKilled,
                                                      const std::vector<CombatEncounter>& combatEncounters);

    // Path simplification
    std::vector<Position> SimplifyPath(const std::vector<Position>& path, float tolerance = 2.0f);
    std::vector<Position> DouglasPeucker(const std::vector<Position>& path, float epsilon);

    // Waypoint classification
    PathfindingWaypointType ClassifyPosition(const Position& pos,
                                             const std::vector<CombatEncounter>& encounters,
                                             bool nearEntrance = false);
    bool IsNearBossEncounter(const Position& pos, const std::vector<CombatEncounter>& encounters) const;
    bool IsNearTrashEncounter(const Position& pos, const std::vector<CombatEncounter>& encounters) const;
    uint32 GetBossAtPosition(const Position& pos, const std::vector<CombatEncounter>& encounters) const;

    // Confidence calculation
    float CalculateConfidence(const Position& pos, const std::vector<std::vector<Position>>& allPaths);
    void UpdateCandidateConfidence(WaypointCandidate& candidate, const std::vector<IterationResult>& iterations);

    // Safe radius calculation
    float CalculateSafeRadius(const Position& pos, uint32 mapId, const std::vector<CombatEncounter>& encounters);

    // Database operations
    void SaveCandidatesToDatabase(uint32 mapId, const std::vector<WaypointCandidate>& candidates);
    void PromoteToWaypoints(uint32 mapId, float minConfidence = 0.8f);
    std::vector<WaypointCandidate> LoadCandidatesFromDatabase(uint32 mapId);
    void ClearCandidates(uint32 mapId);

    // Orientation calculation
    float CalculateOrientation(const Position& current, const Position* next);

private:
    // Path simplification helpers
    float PerpendicularDistance(const Position& point, const Position& lineStart, const Position& lineEnd) const;
    float PointDistance(const Position& a, const Position& b) const;

    // Waypoint spacing
    float m_minWaypointSpacing = 10.0f;   // Minimum distance between waypoints
    float m_maxWaypointSpacing = 50.0f;   // Maximum distance between waypoints
    float m_bossProximityRadius = 30.0f;  // Radius to consider "near" a boss
    float m_trashProximityRadius = 15.0f; // Radius to consider "near" trash

    // Safe spot detection
    bool IsSafeSpot(const Position& pos, const std::vector<CombatEncounter>& encounters) const;
    float DistanceToNearestCombat(const Position& pos, const std::vector<CombatEncounter>& encounters) const;
};

#endif  // _PLAYERBOT_WAYPOINTGENERATOR_H
