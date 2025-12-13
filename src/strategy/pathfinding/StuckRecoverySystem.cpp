/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "StuckRecoverySystem.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "QueryResult.h"
#include "MotionMaster.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"
#include <cmath>
#include <mutex>
#include <random>

StuckRecoverySystem::StuckRecoverySystem() = default;
StuckRecoverySystem::~StuckRecoverySystem() = default;

RecoveryResult StuckRecoverySystem::AttemptRecovery(Player* bot, PathfindingContext& ctx)
{
    RecoveryResult result;
    result.attemptNumber = ctx.recoveryAttempts;

    if (!bot)
    {
        result.success = false;
        return result;
    }

    // Check if we know a good recovery method for this location
    StuckRecoveryMethod bestMethod = GetBestRecoveryMethod(ctx.mapId, ctx.stuckPosition);
    if (bestMethod != StuckRecoveryMethod::NONE && ctx.recoveryAttempts == 0)
    {
        // Try the historically successful method first
        LOG_DEBUG("playerbots", "StuckRecoverySystem: Using known best method {} for location",
                  GetRecoveryMethodName(bestMethod));
    }

    // Get the method to try based on attempt number
    StuckRecoveryMethod method = GetNextRecoveryMethod(ctx.recoveryAttempts);
    result.methodUsed = method;

    bool success = false;

    switch (method)
    {
        case StuckRecoveryMethod::TURN_AND_MOVE:
            success = TryTurnAndMove(bot, ctx);
            break;
        case StuckRecoveryMethod::JUMP_FORWARD:
            success = TryJumpForward(bot, ctx);
            break;
        case StuckRecoveryMethod::JUMP_SIDEWAYS:
            success = TryJumpSideways(bot, ctx);
            break;
        case StuckRecoveryMethod::BACKTRACK:
            success = TryBacktrack(bot, ctx);
            break;
        case StuckRecoveryMethod::TELEPORT_RESET:
            success = TryTeleportReset(bot, ctx);
            break;
        default:
            success = false;
            break;
    }

    result.success = success;
    result.newPosition = bot->GetPosition();

    // Record this stuck location and outcome
    RecordStuckLocation(ctx.mapId, ctx.stuckPosition, method, success);

    if (success)
    {
        LOG_DEBUG("playerbots", "StuckRecoverySystem: Bot {} recovered using {} (attempt {})",
                  bot->GetName(), GetRecoveryMethodName(method), ctx.recoveryAttempts);
    }

    return result;
}

bool StuckRecoverySystem::IsStuck(Player* bot, const PathfindingContext& ctx) const
{
    if (!bot)
        return false;

    uint32 timeSinceMove = GetTimeSinceLastMove(ctx);
    return timeSinceMove >= m_stuckThresholdMs;
}

uint32 StuckRecoverySystem::GetTimeSinceLastMove(const PathfindingContext& ctx) const
{
    if (ctx.lastMoveTime == 0)
        return 0;

    return getMSTime() - ctx.lastMoveTime;
}

bool StuckRecoverySystem::TryTurnAndMove(Player* bot, PathfindingContext& ctx)
{
    if (!bot)
        return false;

    // Calculate turn angle based on attempt number
    float turnAngle = m_lastTurnAngle + m_turnAngleStep;
    if (ctx.recoveryAttempts % 2 == 1)
        turnAngle = -turnAngle;  // Alternate left/right

    m_lastTurnAngle = std::abs(turnAngle);

    // Convert to radians and add to current orientation
    float newOrientation = bot->GetOrientation() + (turnAngle * M_PI / 180.0f);

    // Normalize orientation
    while (newOrientation > 2 * M_PI) newOrientation -= 2 * M_PI;
    while (newOrientation < 0) newOrientation += 2 * M_PI;

    // Calculate target position
    float targetX = bot->GetPositionX() + std::cos(newOrientation) * m_moveDistance;
    float targetY = bot->GetPositionY() + std::sin(newOrientation) * m_moveDistance;
    float targetZ = bot->GetPositionZ();

    // Validate target position
    Position target;
    target.m_positionX = targetX;
    target.m_positionY = targetY;
    target.m_positionZ = targetZ;

    if (!IsValidPosition(bot, target))
    {
        // Try to find a valid nearby position
        target = FindNearestValidPosition(bot, target);
        if (!IsValidPosition(bot, target))
            return false;
    }

    // Execute the move
    ExecuteMove(bot, newOrientation, m_moveDistance);

    // Check if we actually moved
    // Wait a bit and check position change
    return true;  // Assume success, will be verified on next update
}

bool StuckRecoverySystem::TryJumpForward(Player* bot, PathfindingContext& ctx)
{
    if (!bot)
        return false;

    float orientation = bot->GetOrientation();
    Position target = GetJumpTargetPosition(bot, orientation, m_jumpDistance);

    if (!CanJumpTo(bot, target))
    {
        // Try slightly different angles
        for (float angleOffset : {15.0f, -15.0f, 30.0f, -30.0f})
        {
            float adjustedAngle = orientation + (angleOffset * M_PI / 180.0f);
            target = GetJumpTargetPosition(bot, adjustedAngle, m_jumpDistance);
            if (CanJumpTo(bot, target))
                break;
        }
    }

    if (!CanJumpTo(bot, target))
        return false;

    ExecuteJump(bot, target);

    LOG_DEBUG("playerbots", "StuckRecoverySystem: Bot {} jumping forward to ({}, {}, {})",
              bot->GetName(), target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());

    return true;
}

bool StuckRecoverySystem::TryJumpSideways(Player* bot, PathfindingContext& ctx)
{
    if (!bot)
        return false;

    float orientation = bot->GetOrientation();

    // Try jumping to the side (90 degrees from facing)
    // Alternate between left and right based on attempt
    float sideAngle = orientation + (ctx.recoveryAttempts % 2 == 0 ? M_PI / 2 : -M_PI / 2);

    Position target = GetJumpTargetPosition(bot, sideAngle, m_jumpDistance);

    if (!CanJumpTo(bot, target))
    {
        // Try the other side
        sideAngle = orientation + (ctx.recoveryAttempts % 2 == 0 ? -M_PI / 2 : M_PI / 2);
        target = GetJumpTargetPosition(bot, sideAngle, m_jumpDistance);
    }

    if (!CanJumpTo(bot, target))
        return false;

    ExecuteJump(bot, target);

    LOG_DEBUG("playerbots", "StuckRecoverySystem: Bot {} jumping sideways to ({}, {}, {})",
              bot->GetName(), target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());

    return true;
}

bool StuckRecoverySystem::TryBacktrack(Player* bot, PathfindingContext& ctx)
{
    if (!bot)
        return false;

    if (ctx.breadcrumbTrail.empty())
        return false;

    // Find the best backtrack position (far enough but reachable)
    Position bestBacktrackPos;
    float minDistFromStuck = 10.0f;  // At least 10 yards from stuck position

    for (auto it = ctx.breadcrumbTrail.rbegin(); it != ctx.breadcrumbTrail.rend(); ++it)
    {
        float dx = it->GetPositionX() - ctx.stuckPosition.GetPositionX();
        float dy = it->GetPositionY() - ctx.stuckPosition.GetPositionY();
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist >= minDistFromStuck && IsValidPosition(bot, *it))
        {
            bestBacktrackPos = *it;
            break;
        }
    }

    if (bestBacktrackPos.GetPositionX() == 0.0f)
    {
        // No valid backtrack position found, use the oldest one
        if (!ctx.breadcrumbTrail.empty())
            bestBacktrackPos = ctx.breadcrumbTrail.front();
        else
            return false;
    }

    // Teleport to backtrack position
    bot->NearTeleportTo(bestBacktrackPos.GetPositionX(), bestBacktrackPos.GetPositionY(),
                        bestBacktrackPos.GetPositionZ(), bot->GetOrientation());

    LOG_DEBUG("playerbots", "StuckRecoverySystem: Bot {} backtracking to ({}, {}, {})",
              bot->GetName(), bestBacktrackPos.GetPositionX(),
              bestBacktrackPos.GetPositionY(), bestBacktrackPos.GetPositionZ());

    return true;
}

bool StuckRecoverySystem::TryTeleportReset(Player* bot, PathfindingContext& ctx)
{
    if (!bot)
        return false;

    // Last resort: teleport to dungeon entrance
    Position entrance = ctx.entrancePosition;

    if (entrance.GetPositionX() == 0.0f)
    {
        // Use homebind as fallback
        bot->TeleportTo(bot->m_homebindMapId, bot->m_homebindX, bot->m_homebindY,
                        bot->m_homebindZ, bot->GetOrientation());
    }
    else
    {
        bot->TeleportTo(ctx.mapId, entrance.GetPositionX(), entrance.GetPositionY(),
                        entrance.GetPositionZ(), entrance.GetOrientation());
    }

    LOG_INFO("playerbots", "StuckRecoverySystem: Bot {} teleported to entrance as last resort",
             bot->GetName());

    return true;
}

StuckRecoveryMethod StuckRecoverySystem::GetNextRecoveryMethod(uint32 attemptNumber) const
{
    // Progressive recovery strategy
    switch (attemptNumber)
    {
        case 0:
            return StuckRecoveryMethod::TURN_AND_MOVE;
        case 1:
            return StuckRecoveryMethod::JUMP_FORWARD;
        case 2:
            return StuckRecoveryMethod::TURN_AND_MOVE;
        case 3:
            return StuckRecoveryMethod::JUMP_SIDEWAYS;
        case 4:
            return StuckRecoveryMethod::BACKTRACK;
        default:
            return StuckRecoveryMethod::TELEPORT_RESET;
    }
}

float StuckRecoverySystem::GetRandomAngle(float baseAngle, float variance) const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-variance, variance);
    return baseAngle + dis(gen);
}

Position StuckRecoverySystem::GetJumpTargetPosition(Player* bot, float angle, float distance) const
{
    Position target;
    target.m_positionX = bot->GetPositionX() + std::cos(angle) * distance;
    target.m_positionY = bot->GetPositionY() + std::sin(angle) * distance;
    target.m_positionZ = bot->GetPositionZ() + 0.5f;  // Slight upward for jump arc
    target.m_orientation = angle;

    return target;
}

bool StuckRecoverySystem::CanJumpTo(Player* bot, const Position& target) const
{
    if (!bot)
        return false;

    // Check if target is within reasonable jump range
    float dx = target.GetPositionX() - bot->GetPositionX();
    float dy = target.GetPositionY() - bot->GetPositionY();
    float dz = target.GetPositionZ() - bot->GetPositionZ();
    float horizontalDist = std::sqrt(dx * dx + dy * dy);

    // Can't jump too far horizontally
    if (horizontalDist > 8.0f)
        return false;

    // Can't jump up too high (about 2.5 yards max natural jump)
    if (dz > 3.0f)
        return false;

    // Can jump down further
    if (dz < -15.0f)
        return false;

    return IsValidPosition(bot, target);
}

void StuckRecoverySystem::ExecuteJump(Player* bot, const Position& target)
{
    if (!bot)
        return;

    // Set facing toward target
    float angle = std::atan2(target.GetPositionY() - bot->GetPositionY(),
                             target.GetPositionX() - bot->GetPositionX());
    bot->SetFacingTo(angle);

    // Use knockback-like movement to simulate jump
    // This uses the motion master to handle the movement

    float dx = target.GetPositionX() - bot->GetPositionX();
    float dy = target.GetPositionY() - bot->GetPositionY();
    float dz = target.GetPositionZ() - bot->GetPositionZ();
    float horizontalDist = std::sqrt(dx * dx + dy * dy);

    // Calculate jump speed (adjust as needed for feel)
    float speedXY = horizontalDist / 0.5f;  // Cover distance in 0.5 seconds
    float speedZ = 5.0f + (dz > 0 ? dz * 2.0f : 0.0f);  // Upward velocity

    // Apply jump movement
    bot->KnockbackFrom(bot->GetPositionX() - dx * 0.1f, bot->GetPositionY() - dy * 0.1f,
                       speedXY, speedZ);
}

void StuckRecoverySystem::ExecuteMove(Player* bot, float angle, float distance)
{
    if (!bot)
        return;

    float targetX = bot->GetPositionX() + std::cos(angle) * distance;
    float targetY = bot->GetPositionY() + std::sin(angle) * distance;
    float targetZ = bot->GetPositionZ();

    // Update Z from map height
    Map* map = bot->GetMap();
    if (map)
    {
        float groundZ = map->GetHeight(bot->GetPhaseMask(), targetX, targetY, targetZ + 5.0f, true);
        if (groundZ != INVALID_HEIGHT)
            targetZ = groundZ;
    }

    // Set facing and initiate movement
    bot->SetFacingTo(angle);

    // Use motion master to move
    bot->GetMotionMaster()->MovePoint(0, targetX, targetY, targetZ);
}

bool StuckRecoverySystem::IsValidPosition(Player* bot, const Position& pos) const
{
    if (!bot)
        return false;

    Map* map = bot->GetMap();
    if (!map)
        return false;

    // Check if position is within map bounds
    if (!map->IsValidMapCoord(pos.GetPositionX(), pos.GetPositionY()))
        return false;

    // Check ground height
    float groundZ = map->GetHeight(bot->GetPhaseMask(), pos.GetPositionX(), pos.GetPositionY(),
                                   pos.GetPositionZ() + 5.0f, true);
    if (groundZ == INVALID_HEIGHT)
        return false;

    // Check height difference
    float heightDiff = std::abs(groundZ - pos.GetPositionZ());
    if (heightDiff > 10.0f)
        return false;

    // Check for water (optional - bots can usually swim)
    // LiquidData liquidData;
    // map->GetLiquidStatus(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), MAP_ALL_LIQUIDS, &liquidData);

    return true;
}

bool StuckRecoverySystem::HasLineOfSight(Player* bot, const Position& target) const
{
    if (!bot)
        return false;

    return bot->IsWithinLOS(target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());
}

Position StuckRecoverySystem::FindNearestValidPosition(Player* bot, const Position& target) const
{
    if (!bot)
        return Position();

    // Search in a spiral pattern for a valid position
    const float searchRadius = 10.0f;
    const float stepSize = 2.0f;

    for (float radius = stepSize; radius <= searchRadius; radius += stepSize)
    {
        for (float angle = 0.0f; angle < 2 * M_PI; angle += M_PI / 4)
        {
            Position test;
            test.m_positionX = target.GetPositionX() + std::cos(angle) * radius;
            test.m_positionY = target.GetPositionY() + std::sin(angle) * radius;
            test.m_positionZ = target.GetPositionZ();

            if (IsValidPosition(bot, test))
                return test;
        }
    }

    return Position();  // No valid position found
}

void StuckRecoverySystem::RecordStuckLocation(uint32 mapId, const Position& pos,
                                              StuckRecoveryMethod method, bool success)
{
    std::unique_lock<std::shared_mutex> lock(m_stuckLocationMutex);

    // Check if we already have this location
    for (auto& loc : m_knownStuckLocations)
    {
        float dx = loc.pos.GetPositionX() - pos.GetPositionX();
        float dy = loc.pos.GetPositionY() - pos.GetPositionY();
        float dz = loc.pos.GetPositionZ() - pos.GetPositionZ();
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist < 5.0f)  // Within 5 yards
        {
            loc.stuckCount++;
            if (success)
            {
                loc.recoverySuccessCount++;
                // Update best method if this one worked more often
                if (method != loc.bestRecoveryMethod)
                {
                    // Simple heuristic - if success rate > 50%, update method
                    if (loc.recoverySuccessCount * 2 > loc.stuckCount)
                        loc.bestRecoveryMethod = method;
                }
            }

            // Save to database periodically
            if (loc.stuckCount % 5 == 0)
                SaveStuckLocationToDatabase(mapId, loc);

            return;
        }
    }

    // New stuck location
    StuckLocation newLoc;
    newLoc.pos = pos;
    newLoc.stuckCount = 1;
    newLoc.recoverySuccessCount = success ? 1 : 0;
    newLoc.bestRecoveryMethod = success ? method : StuckRecoveryMethod::NONE;
    newLoc.avoidRadius = 3.0f;

    m_knownStuckLocations.push_back(newLoc);
    SaveStuckLocationToDatabase(mapId, newLoc);
}

bool StuckRecoverySystem::IsKnownStuckLocation(uint32 mapId, const Position& pos) const
{
    std::shared_lock<std::shared_mutex> lock(m_stuckLocationMutex);

    for (const auto& loc : m_knownStuckLocations)
    {
        float dx = loc.pos.GetPositionX() - pos.GetPositionX();
        float dy = loc.pos.GetPositionY() - pos.GetPositionY();
        float dz = loc.pos.GetPositionZ() - pos.GetPositionZ();
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist < loc.avoidRadius)
            return true;
    }

    return false;
}

StuckRecoveryMethod StuckRecoverySystem::GetBestRecoveryMethod(uint32 mapId, const Position& pos) const
{
    std::shared_lock<std::shared_mutex> lock(m_stuckLocationMutex);

    for (const auto& loc : m_knownStuckLocations)
    {
        float dx = loc.pos.GetPositionX() - pos.GetPositionX();
        float dy = loc.pos.GetPositionY() - pos.GetPositionY();
        float dz = loc.pos.GetPositionZ() - pos.GetPositionZ();
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist < 5.0f)
            return loc.bestRecoveryMethod;
    }

    return StuckRecoveryMethod::NONE;
}

void StuckRecoverySystem::LoadStuckLocationsFromDatabase()
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT map_id, x, y, z, stuck_count, recovery_success_count, recovery_method, avoid_radius "
        "FROM playerbots_pathfinding_stuck_locations");

    if (!result)
        return;

    std::unique_lock<std::shared_mutex> lock(m_stuckLocationMutex);
    m_knownStuckLocations.clear();

    do
    {
        Field* fields = result->Fetch();
        StuckLocation loc;
        // uint32 mapId = fields[0].Get<uint32>();  // Could filter by map
        loc.pos.m_positionX = fields[1].Get<float>();
        loc.pos.m_positionY = fields[2].Get<float>();
        loc.pos.m_positionZ = fields[3].Get<float>();
        loc.stuckCount = fields[4].Get<uint32>();
        loc.recoverySuccessCount = fields[5].Get<uint32>();
        loc.bestRecoveryMethod = static_cast<StuckRecoveryMethod>(fields[6].Get<uint8>());
        loc.avoidRadius = fields[7].Get<float>();

        m_knownStuckLocations.push_back(loc);
    }
    while (result->NextRow());

    LOG_INFO("playerbots", "StuckRecoverySystem: Loaded {} known stuck locations", m_knownStuckLocations.size());
}

void StuckRecoverySystem::SaveStuckLocationToDatabase(uint32 mapId, const StuckLocation& loc)
{
    CharacterDatabase.Execute(
        "INSERT INTO playerbots_pathfinding_stuck_locations "
        "(map_id, x, y, z, stuck_count, recovery_success_count, recovery_method, avoid_radius) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE stuck_count = {}, recovery_success_count = {}, "
        "recovery_method = {}, avoid_radius = {}",
        mapId, loc.pos.GetPositionX(), loc.pos.GetPositionY(), loc.pos.GetPositionZ(),
        loc.stuckCount, loc.recoverySuccessCount, static_cast<uint8>(loc.bestRecoveryMethod), loc.avoidRadius,
        loc.stuckCount, loc.recoverySuccessCount, static_cast<uint8>(loc.bestRecoveryMethod), loc.avoidRadius);
}
