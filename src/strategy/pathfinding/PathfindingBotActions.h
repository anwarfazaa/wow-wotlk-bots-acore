/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHFINDINGBOTACTIONS_H
#define _PLAYERBOT_PATHFINDINGBOTACTIONS_H

#include "Action.h"
#include "PathfindingBotContext.h"

class PlayerbotAI;

/**
 * Base class for pathfinding actions
 */
class PathfindingBotAction : public Action
{
public:
    PathfindingBotAction(PlayerbotAI* botAI, std::string const name = "pathfinding action")
        : Action(botAI, name) {}

protected:
    bool IsPathfindingActive() const;
    PathfindingContext* GetContext() const;
};

/**
 * Start pathfinding for a dungeon
 */
class PathfindingStartAction : public PathfindingBotAction
{
public:
    PathfindingStartAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding start") {}

    bool Execute(Event event) override;
};

/**
 * Stop pathfinding
 */
class PathfindingStopAction : public PathfindingBotAction
{
public:
    PathfindingStopAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding stop") {}

    bool Execute(Event event) override;
};

/**
 * Move to the next frontier target during exploration
 */
class PathfindingExploreAction : public PathfindingBotAction
{
public:
    PathfindingExploreAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding explore") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    bool MoveTo(float x, float y, float z);
};

/**
 * Attempt to recover from being stuck
 */
class PathfindingRecoverStuckAction : public PathfindingBotAction
{
public:
    PathfindingRecoverStuckAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding recover stuck") {}

    bool Execute(Event event) override;
};

/**
 * Record a boss encounter location
 */
class PathfindingRecordBossAction : public PathfindingBotAction
{
public:
    PathfindingRecordBossAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding record boss") {}

    bool Execute(Event event) override;
};

/**
 * Record a trash pack location
 */
class PathfindingRecordTrashAction : public PathfindingBotAction
{
public:
    PathfindingRecordTrashAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding record trash") {}

    bool Execute(Event event) override;
};

/**
 * Analyze the current iteration and check for convergence
 */
class PathfindingAnalyzeAction : public PathfindingBotAction
{
public:
    PathfindingAnalyzeAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding analyze") {}

    bool Execute(Event event) override;
};

/**
 * Reset the dungeon instance for a new iteration
 */
class PathfindingResetInstanceAction : public PathfindingBotAction
{
public:
    PathfindingResetInstanceAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding reset instance") {}

    bool Execute(Event event) override;
};

/**
 * Generate waypoint candidates from learned path
 */
class PathfindingGenerateWaypointsAction : public PathfindingBotAction
{
public:
    PathfindingGenerateWaypointsAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding generate waypoints") {}

    bool Execute(Event event) override;
};

/**
 * Report current pathfinding status
 */
class PathfindingStatusAction : public PathfindingBotAction
{
public:
    PathfindingStatusAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding status") {}

    bool Execute(Event event) override;
};

/**
 * Handle bot death during pathfinding
 */
class PathfindingHandleDeathAction : public PathfindingBotAction
{
public:
    PathfindingHandleDeathAction(PlayerbotAI* botAI)
        : PathfindingBotAction(botAI, "pathfinding handle death") {}

    bool Execute(Event event) override;
};

#endif  // _PLAYERBOT_PATHFINDINGBOTACTIONS_H
