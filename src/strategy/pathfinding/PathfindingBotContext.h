/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHFINDINGBOTCONTEXT_H
#define _PLAYERBOT_PATHFINDINGBOTCONTEXT_H

#include "Common.h"
#include "Duration.h"
#include "Position.h"
#include <vector>
#include <string>
#include <unordered_map>

/**
 * PathfindingBot State Machine States
 */
enum class PathfindingState : uint8
{
    IDLE = 0,           // Waiting for command to start
    ENTERING,           // Teleporting into dungeon instance
    EXPLORING,          // Active frontier-based exploration
    STUCK_RECOVERY,     // Attempting to recover from stuck position
    COMBAT,             // Fighting trash mobs
    BOSS_ENCOUNTER,     // Fighting a boss (record location)
    EXITING,            // Leaving dungeon after completion
    RESETTING,          // Resetting instance for next iteration
    ANALYZING,          // Processing iteration data, checking convergence
    COMPLETE            // Route has converged, waypoints generated
};

/**
 * Waypoint types for classification
 */
enum class PathfindingWaypointType : uint8
{
    PATH = 0,           // Regular path waypoint
    BOSS = 1,           // Boss encounter location
    TRASH_PACK = 2,     // Trash pack pull location
    SAFE_SPOT = 3,      // Safe resting location
    EVENT = 4           // Special event location (RP, trigger, etc.)
};

/**
 * Stuck recovery methods in order of preference
 */
enum class StuckRecoveryMethod : uint8
{
    NONE = 0,
    TURN_AND_MOVE,      // Turn 45-90 degrees and move
    JUMP_FORWARD,       // Jump in current direction
    JUMP_SIDEWAYS,      // Jump left or right
    BACKTRACK,          // Return to last good position
    TELEPORT_RESET      // Last resort - reset to entrance
};

/**
 * Result of a single exploration iteration
 */
struct IterationResult
{
    uint32 mapId = 0;
    uint32 iteration = 0;
    uint32 durationMs = 0;
    uint32 deaths = 0;
    uint32 stuckEvents = 0;
    float totalDistance = 0.0f;
    float explorationPct = 0.0f;
    float score = 0.0f;
    std::vector<Position> path;
    std::vector<uint32> bossesKilled;
    std::string pathJson;

    void Clear()
    {
        mapId = 0;
        iteration = 0;
        durationMs = 0;
        deaths = 0;
        stuckEvents = 0;
        totalDistance = 0.0f;
        explorationPct = 0.0f;
        score = 0.0f;
        path.clear();
        bossesKilled.clear();
        pathJson.clear();
    }
};

/**
 * Waypoint candidate learned from exploration
 */
struct WaypointCandidate
{
    uint32 mapId = 0;
    uint32 waypointIndex = 0;
    Position pos;
    PathfindingWaypointType type = PathfindingWaypointType::PATH;
    uint32 bossEntry = 0;
    uint32 trashPackId = 0;
    float safeRadius = 5.0f;
    float confidence = 0.0f;        // 0-1, based on consistency across iterations
    uint32 timesVisited = 0;
    uint32 avgCombatDurationMs = 0;
};

/**
 * Stuck location record
 */
struct StuckLocation
{
    Position pos;
    uint32 stuckCount = 0;
    uint32 recoverySuccessCount = 0;
    StuckRecoveryMethod bestRecoveryMethod = StuckRecoveryMethod::NONE;
    float avoidRadius = 3.0f;
};

/**
 * Result of stuck recovery attempt
 */
struct RecoveryResult
{
    bool success = false;
    StuckRecoveryMethod methodUsed = StuckRecoveryMethod::NONE;
    Position newPosition;
    uint32 attemptNumber = 0;
};

/**
 * Grid cell for exploration tracking
 */
struct ExplorationGridCell
{
    bool explored = false;
    bool reachable = true;
    bool hasObstacle = false;
    bool isFrontier = false;
    uint32 visitCount = 0;
    uint32 lastVisitTime = 0;
};

/**
 * Combat encounter record
 */
struct CombatEncounter
{
    Position pos;
    uint32 startTime = 0;
    uint32 endTime = 0;
    uint32 enemyCount = 0;
    bool isBoss = false;
    uint32 bossEntry = 0;
    std::vector<uint32> enemyEntries;
};

/**
 * Main pathfinding context for a bot
 * Contains all state for the pathfinding operation
 */
struct PathfindingContext
{
    // Current state
    PathfindingState state = PathfindingState::IDLE;
    uint32 mapId = 0;
    uint32 currentIteration = 0;
    uint32 maxIterations = 10;
    bool isActive = false;

    // Timing
    uint32 runStartTime = 0;
    uint32 lastUpdateTime = 0;
    uint32 stateStartTime = 0;

    // Current run metrics
    uint32 deathCount = 0;
    uint32 stuckCount = 0;
    float totalDistance = 0.0f;
    Position lastPosition;
    uint32 lastMoveTime = 0;

    // Path tracking
    std::vector<Position> pathTaken;
    std::vector<uint32> bossesKilled;
    std::vector<CombatEncounter> combatEncounters;
    std::vector<Position> trashPackLocations;

    // Stuck recovery state
    Position stuckPosition;
    uint32 stuckStartTime = 0;
    uint32 recoveryAttempts = 0;
    StuckRecoveryMethod currentRecoveryMethod = StuckRecoveryMethod::NONE;
    std::vector<Position> breadcrumbTrail;  // Good positions for backtracking

    // Learning data
    std::vector<IterationResult> previousRuns;
    float bestScore = 0.0f;
    uint32 iterationsWithoutImprovement = 0;

    // Exploration state
    std::unordered_map<uint64, ExplorationGridCell> explorationGrid;
    std::vector<Position> frontierCells;
    Position currentExplorationTarget;
    float explorationPercent = 0.0f;

    // Dungeon info
    Position entrancePosition;
    std::vector<uint32> expectedBosses;  // Boss entries we expect to find
    uint32 totalBossCount = 0;

    void Reset()
    {
        state = PathfindingState::IDLE;
        currentIteration = 0;
        isActive = false;
        runStartTime = 0;
        lastUpdateTime = 0;
        stateStartTime = 0;
        deathCount = 0;
        stuckCount = 0;
        totalDistance = 0.0f;
        lastMoveTime = 0;
        pathTaken.clear();
        bossesKilled.clear();
        combatEncounters.clear();
        trashPackLocations.clear();
        stuckStartTime = 0;
        recoveryAttempts = 0;
        currentRecoveryMethod = StuckRecoveryMethod::NONE;
        breadcrumbTrail.clear();
        previousRuns.clear();
        bestScore = 0.0f;
        iterationsWithoutImprovement = 0;
        explorationGrid.clear();
        frontierCells.clear();
        explorationPercent = 0.0f;
    }

    void ResetForNewIteration()
    {
        // Keep learning data, reset run-specific data
        deathCount = 0;
        stuckCount = 0;
        totalDistance = 0.0f;
        lastMoveTime = 0;
        pathTaken.clear();
        bossesKilled.clear();
        combatEncounters.clear();
        trashPackLocations.clear();
        stuckStartTime = 0;
        recoveryAttempts = 0;
        currentRecoveryMethod = StuckRecoveryMethod::NONE;
        breadcrumbTrail.clear();
        explorationGrid.clear();
        frontierCells.clear();
        explorationPercent = 0.0f;
        currentIteration++;
        runStartTime = getMSTime();
        stateStartTime = runStartTime;
    }
};

/**
 * Configuration for pathfinding bot
 */
struct PathfindingConfig
{
    bool enabled = true;
    uint32 maxIterations = 10;
    uint32 stuckThresholdMs = 10000;        // 10 seconds
    uint32 maxRecoveryAttempts = 5;
    uint32 convergenceIterations = 3;
    float convergenceThreshold = 0.02f;     // 2% improvement threshold
    bool autoPromoteWaypoints = false;
    float minConfidenceForPromotion = 0.8f;
    float explorationCellSize = 5.0f;       // Grid cell size in yards
    uint32 breadcrumbInterval = 1000;       // Record position every 1 second
    uint32 maxBreadcrumbs = 100;            // Keep last 100 positions
    float pathSimplificationTolerance = 2.0f;  // Merge waypoints within 2 yards

    // Score weights
    float weightTime = 0.4f;
    float weightDeaths = 0.3f;
    float weightStuck = 0.2f;
    float weightDistance = 0.1f;
};

/**
 * Helper functions for state names
 */
inline const char* GetPathfindingStateName(PathfindingState state)
{
    switch (state)
    {
        case PathfindingState::IDLE: return "IDLE";
        case PathfindingState::ENTERING: return "ENTERING";
        case PathfindingState::EXPLORING: return "EXPLORING";
        case PathfindingState::STUCK_RECOVERY: return "STUCK_RECOVERY";
        case PathfindingState::COMBAT: return "COMBAT";
        case PathfindingState::BOSS_ENCOUNTER: return "BOSS_ENCOUNTER";
        case PathfindingState::EXITING: return "EXITING";
        case PathfindingState::RESETTING: return "RESETTING";
        case PathfindingState::ANALYZING: return "ANALYZING";
        case PathfindingState::COMPLETE: return "COMPLETE";
        default: return "UNKNOWN";
    }
}

inline const char* GetWaypointTypeName(PathfindingWaypointType type)
{
    switch (type)
    {
        case PathfindingWaypointType::PATH: return "PATH";
        case PathfindingWaypointType::BOSS: return "BOSS";
        case PathfindingWaypointType::TRASH_PACK: return "TRASH_PACK";
        case PathfindingWaypointType::SAFE_SPOT: return "SAFE_SPOT";
        case PathfindingWaypointType::EVENT: return "EVENT";
        default: return "UNKNOWN";
    }
}

inline const char* GetRecoveryMethodName(StuckRecoveryMethod method)
{
    switch (method)
    {
        case StuckRecoveryMethod::NONE: return "NONE";
        case StuckRecoveryMethod::TURN_AND_MOVE: return "TURN_AND_MOVE";
        case StuckRecoveryMethod::JUMP_FORWARD: return "JUMP_FORWARD";
        case StuckRecoveryMethod::JUMP_SIDEWAYS: return "JUMP_SIDEWAYS";
        case StuckRecoveryMethod::BACKTRACK: return "BACKTRACK";
        case StuckRecoveryMethod::TELEPORT_RESET: return "TELEPORT_RESET";
        default: return "UNKNOWN";
    }
}

#endif  // _PLAYERBOT_PATHFINDINGBOTCONTEXT_H
