/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHFINDINGBOTTRIGGERS_H
#define _PLAYERBOT_PATHFINDINGBOTTRIGGERS_H

#include "Trigger.h"
#include "PathfindingBotContext.h"

class PlayerbotAI;

/**
 * Base class for pathfinding triggers
 */
class PathfindingBotTrigger : public Trigger
{
public:
    PathfindingBotTrigger(PlayerbotAI* botAI, std::string const name = "pathfinding trigger")
        : Trigger(botAI, name) {}

protected:
    bool IsPathfindingActive() const;
    PathfindingState GetPathfindingState() const;
};

/**
 * Triggers when the pathfinding bot should start exploring
 */
class PathfindingShouldExploreTrigger : public PathfindingBotTrigger
{
public:
    PathfindingShouldExploreTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding should explore") {}

    bool IsActive() override;
};

/**
 * Triggers when the pathfinding bot is stuck
 */
class PathfindingIsStuckTrigger : public PathfindingBotTrigger
{
public:
    PathfindingIsStuckTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding is stuck") {}

    bool IsActive() override;
};

/**
 * Triggers when a boss is encountered
 */
class PathfindingBossEncounteredTrigger : public PathfindingBotTrigger
{
public:
    PathfindingBossEncounteredTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding boss encountered") {}

    bool IsActive() override;
};

/**
 * Triggers when trash mobs are encountered
 */
class PathfindingTrashEncounteredTrigger : public PathfindingBotTrigger
{
public:
    PathfindingTrashEncounteredTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding trash encountered") {}

    bool IsActive() override;
};

/**
 * Triggers when a run is complete (all bosses killed or dungeon cleared)
 */
class PathfindingRunCompleteTrigger : public PathfindingBotTrigger
{
public:
    PathfindingRunCompleteTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding run complete") {}

    bool IsActive() override;
};

/**
 * Triggers when analysis should be performed
 */
class PathfindingShouldAnalyzeTrigger : public PathfindingBotTrigger
{
public:
    PathfindingShouldAnalyzeTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding should analyze") {}

    bool IsActive() override;
};

/**
 * Triggers when the route has converged
 */
class PathfindingConvergedTrigger : public PathfindingBotTrigger
{
public:
    PathfindingConvergedTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding converged") {}

    bool IsActive() override;
};

/**
 * Triggers when instance should be reset for new iteration
 */
class PathfindingShouldResetTrigger : public PathfindingBotTrigger
{
public:
    PathfindingShouldResetTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding should reset") {}

    bool IsActive() override;
};

/**
 * Triggers when the bot has died and needs to respawn
 */
class PathfindingBotDeadTrigger : public PathfindingBotTrigger
{
public:
    PathfindingBotDeadTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding bot dead") {}

    bool IsActive() override;
};

/**
 * Triggers when pathfinding is active and bot needs to move
 */
class PathfindingNeedMoveTrigger : public PathfindingBotTrigger
{
public:
    PathfindingNeedMoveTrigger(PlayerbotAI* botAI)
        : PathfindingBotTrigger(botAI, "pathfinding need move") {}

    bool IsActive() override;
};

#endif  // _PLAYERBOT_PATHFINDINGBOTTRIGGERS_H
