/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_STUCKRECOVERYSYSTEM_H
#define _PLAYERBOT_STUCKRECOVERYSYSTEM_H

#include "PathfindingBotContext.h"
#include <vector>

class Player;

/**
 * StuckRecoverySystem - Enhanced stuck detection and recovery
 *
 * Provides multiple recovery methods for when a bot gets stuck:
 * 1. Turn and move - Try different directions
 * 2. Jump forward - Attempt to jump over obstacle
 * 3. Jump sideways - Jump left or right
 * 4. Backtrack - Return to last known good position
 * 5. Teleport reset - Last resort, reset to entrance
 *
 * Features:
 * - 10 second stuck detection threshold
 * - Progressive recovery attempts
 * - Learning from successful recoveries
 * - Recording stuck locations to avoid
 */
class StuckRecoverySystem
{
public:
    StuckRecoverySystem();
    ~StuckRecoverySystem();

    // Main recovery interface
    RecoveryResult AttemptRecovery(Player* bot, PathfindingContext& ctx);

    // Stuck detection
    bool IsStuck(Player* bot, const PathfindingContext& ctx) const;
    uint32 GetTimeSinceLastMove(const PathfindingContext& ctx) const;

    // Recovery methods
    bool TryTurnAndMove(Player* bot, PathfindingContext& ctx);
    bool TryJumpForward(Player* bot, PathfindingContext& ctx);
    bool TryJumpSideways(Player* bot, PathfindingContext& ctx);
    bool TryBacktrack(Player* bot, PathfindingContext& ctx);
    bool TryTeleportReset(Player* bot, PathfindingContext& ctx);

    // Configuration
    void SetStuckThreshold(uint32 ms) { m_stuckThresholdMs = ms; }
    uint32 GetStuckThreshold() const { return m_stuckThresholdMs; }
    void SetMaxAttempts(uint32 attempts) { m_maxAttempts = attempts; }

    // Stuck location tracking
    void RecordStuckLocation(uint32 mapId, const Position& pos, StuckRecoveryMethod method, bool success);
    bool IsKnownStuckLocation(uint32 mapId, const Position& pos) const;
    StuckRecoveryMethod GetBestRecoveryMethod(uint32 mapId, const Position& pos) const;

    // Database operations
    void LoadStuckLocationsFromDatabase();
    void SaveStuckLocationToDatabase(uint32 mapId, const StuckLocation& loc);

private:
    // Recovery helpers
    StuckRecoveryMethod GetNextRecoveryMethod(uint32 attemptNumber) const;
    float GetRandomAngle(float baseAngle, float variance) const;
    Position GetJumpTargetPosition(Player* bot, float angle, float distance) const;
    bool CanJumpTo(Player* bot, const Position& target) const;
    void ExecuteJump(Player* bot, const Position& target);
    void ExecuteMove(Player* bot, float angle, float distance);

    // Position validation
    bool IsValidPosition(Player* bot, const Position& pos) const;
    bool HasLineOfSight(Player* bot, const Position& target) const;
    Position FindNearestValidPosition(Player* bot, const Position& target) const;

    // Configuration
    uint32 m_stuckThresholdMs = 10000;     // 10 seconds
    uint32 m_maxAttempts = 5;
    float m_turnAngleStep = 45.0f;          // Degrees to turn per attempt
    float m_jumpDistance = 5.0f;            // Distance to jump
    float m_moveDistance = 8.0f;            // Distance to move after turn

    // Recovery state
    uint32 m_currentAttempt = 0;
    float m_lastTurnAngle = 0.0f;

    // Stuck location cache
    std::vector<StuckLocation> m_knownStuckLocations;
    mutable std::shared_mutex m_stuckLocationMutex;
};

#endif  // _PLAYERBOT_STUCKRECOVERYSYSTEM_H
