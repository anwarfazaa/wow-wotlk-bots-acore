/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "PathfindingBotActions.h"
#include "PathfindingBotManager.h"
#include "ExplorationEngine.h"
#include "WaypointGenerator.h"
#include "Event.h"
#include "Log.h"
#include "MotionMaster.h"
#include "MovementGenerator.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"
#include "ServerFacade.h"

bool PathfindingBotAction::IsPathfindingActive() const
{
    Player* bot = botAI->GetBot();
    return bot && sPathfindingBot->IsActive(bot);
}

PathfindingContext* PathfindingBotAction::GetContext() const
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;
    return sPathfindingBot->GetContext(bot);
}

bool PathfindingStartAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Get map ID from event param or current location
    std::string param = event.getParam();
    uint32 mapId = 0;

    if (!param.empty())
    {
        mapId = std::stoul(param);
    }
    else
    {
        // Check if in a dungeon
        if (bot->GetMap() && bot->GetMap()->IsDungeon())
        {
            mapId = bot->GetMapId();
        }
        else
        {
            botAI->TellMaster("Not in a dungeon. Specify dungeon map ID.");
            return false;
        }
    }

    if (sPathfindingBot->StartPathfinding(bot, mapId))
    {
        botAI->TellMaster("Started pathfinding for dungeon " + std::to_string(mapId));
        return true;
    }

    botAI->TellMaster("Failed to start pathfinding");
    return false;
}

bool PathfindingStopAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    sPathfindingBot->StopPathfinding(bot);
    botAI->TellMaster("Pathfinding stopped");
    return true;
}

bool PathfindingExploreAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    PathfindingContext* ctx = GetContext();
    if (!ctx)
        return false;

    // Get the current exploration target
    Position target = ctx->currentExplorationTarget;

    if (target.GetPositionX() == 0.0f && target.GetPositionY() == 0.0f)
    {
        // No target set yet
        return false;
    }

    // Move toward the target
    return MoveTo(target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());
}

bool PathfindingExploreAction::isPossible()
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->IsInCombat() || bot->isDead())
        return false;

    return IsPathfindingActive();
}

bool PathfindingExploreAction::MoveTo(float x, float y, float z)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Check distance to target
    float dist = bot->GetDistance(x, y, z);

    if (dist < 3.0f)
    {
        // Already at target
        return true;
    }

    // Use motion master to move
    bot->GetMotionMaster()->MovePoint(0, x, y, z);

    // Update last position tracking
    PathfindingContext* ctx = GetContext();
    if (ctx)
    {
        ctx->lastMoveTime = getMSTime();
        sPathfindingBot->OnPositionChanged(bot, bot->GetPosition());
    }

    return true;
}

bool PathfindingRecoverStuckAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    PathfindingContext* ctx = GetContext();
    if (!ctx)
        return false;

    // The manager handles recovery through its update loop
    // This action just acknowledges the stuck state
    LOG_DEBUG("playerbots", "PathfindingRecoverStuckAction: Bot {} attempting recovery (attempt {})",
              bot->GetName(), ctx->recoveryAttempts);

    return true;
}

bool PathfindingRecordBossAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Unit* target = bot->GetVictim();
    if (!target)
        return false;

    uint32 bossEntry = target->GetEntry();
    sPathfindingBot->OnBossKilled(bot, bossEntry);

    LOG_INFO("playerbots", "PathfindingRecordBossAction: Bot {} recorded boss kill: {}",
             bot->GetName(), bossEntry);

    return true;
}

bool PathfindingRecordTrashAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    PathfindingContext* ctx = GetContext();
    if (!ctx)
        return false;

    // Record trash pack location
    CombatEncounter encounter;
    encounter.pos = bot->GetPosition();
    encounter.startTime = getMSTime();
    encounter.isBoss = false;

    ctx->combatEncounters.push_back(encounter);
    ctx->trashPackLocations.push_back(bot->GetPosition());

    return true;
}

bool PathfindingAnalyzeAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Analysis is handled by the manager
    // This action triggers the state transition
    PathfindingContext* ctx = GetContext();
    if (ctx && ctx->state != PathfindingState::ANALYZING)
    {
        sPathfindingBot->SetState(bot, PathfindingState::ANALYZING);
    }

    return true;
}

bool PathfindingResetInstanceAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Exit dungeon and reset
    // This is handled by the manager's RESETTING state
    PathfindingContext* ctx = GetContext();
    if (ctx && ctx->state != PathfindingState::RESETTING)
    {
        sPathfindingBot->SetState(bot, PathfindingState::RESETTING);
    }

    return true;
}

bool PathfindingGenerateWaypointsAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    PathfindingContext* ctx = GetContext();
    if (!ctx)
        return false;

    // Generate waypoints from current context
    WaypointGenerator generator;
    auto waypoints = generator.GenerateWaypoints(*ctx);

    botAI->TellMaster("Generated " + std::to_string(waypoints.size()) + " waypoint candidates");

    return true;
}

bool PathfindingStatusAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    std::string status = sPathfindingBot->GetStatusString(bot);
    botAI->TellMaster(status);

    return true;
}

bool PathfindingHandleDeathAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Record death
    sPathfindingBot->OnBotDeath(bot);

    // Respawn and continue
    if (bot->isDead())
    {
        // Wait for resurrection - this would be handled elsewhere
        // Or use spirit healer
    }

    return true;
}
