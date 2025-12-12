/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "PathfindingBotTriggers.h"
#include "PathfindingBotManager.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"
#include "Player.h"

bool PathfindingBotTrigger::IsPathfindingActive() const
{
    Player* bot = botAI->GetBot();
    return bot && sPathfindingBot->IsActive(bot);
}

PathfindingState PathfindingBotTrigger::GetPathfindingState() const
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return PathfindingState::IDLE;

    return sPathfindingBot->GetState(bot);
}

bool PathfindingShouldExploreTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    PathfindingState state = GetPathfindingState();
    return state == PathfindingState::EXPLORING;
}

bool PathfindingIsStuckTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    const PathfindingContext* ctx = sPathfindingBot->GetContext(bot);
    if (!ctx)
        return false;

    // Check if we're already in recovery
    if (ctx->state == PathfindingState::STUCK_RECOVERY)
        return true;

    // Check if position hasn't changed for threshold time
    uint32 timeSinceMove = getMSTime() - ctx->lastMoveTime;
    return ctx->state == PathfindingState::EXPLORING &&
           timeSinceMove >= sPathfindingBot->GetConfig().stuckThresholdMs;
}

bool PathfindingBossEncounteredTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    Player* bot = botAI->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;

    // Check if current target is a boss
    Unit* target = bot->GetVictim();
    if (!target || !target->ToCreature())
        return false;

    const PathfindingContext* ctx = sPathfindingBot->GetContext(bot);
    if (!ctx)
        return false;

    // Check if target entry is in expected bosses list
    uint32 entry = target->GetEntry();
    for (uint32 bossEntry : ctx->expectedBosses)
    {
        if (entry == bossEntry)
            return true;
    }

    return false;
}

bool PathfindingTrashEncounteredTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    Player* bot = botAI->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;

    // In combat but not with a boss
    PathfindingState state = GetPathfindingState();
    return state == PathfindingState::EXPLORING || state == PathfindingState::COMBAT;
}

bool PathfindingRunCompleteTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    const PathfindingContext* ctx = sPathfindingBot->GetContext(bot);
    if (!ctx)
        return false;

    // Check if all bosses have been killed
    return ctx->bossesKilled.size() >= ctx->totalBossCount;
}

bool PathfindingShouldAnalyzeTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    PathfindingState state = GetPathfindingState();
    return state == PathfindingState::ANALYZING;
}

bool PathfindingConvergedTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    Player* bot = botAI->GetBot();
    return bot && sPathfindingBot->IsConverged(bot);
}

bool PathfindingShouldResetTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    PathfindingState state = GetPathfindingState();
    return state == PathfindingState::RESETTING;
}

bool PathfindingBotDeadTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    Player* bot = botAI->GetBot();
    return bot && bot->isDead();
}

bool PathfindingNeedMoveTrigger::IsActive()
{
    if (!IsPathfindingActive())
        return false;

    Player* bot = botAI->GetBot();
    if (!bot || bot->IsInCombat() || bot->isDead())
        return false;

    PathfindingState state = GetPathfindingState();

    // Need to move in EXPLORING or ENTERING states
    return state == PathfindingState::EXPLORING || state == PathfindingState::ENTERING;
}
