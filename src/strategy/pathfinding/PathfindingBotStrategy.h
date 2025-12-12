/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHFINDINGBOTSTRATEGY_H
#define _PLAYERBOT_PATHFINDINGBOTSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

/**
 * PathfindingBotStrategy - Strategy for autonomous dungeon exploration
 *
 * This strategy enables a bot to autonomously explore dungeons,
 * learn optimal routes, and generate waypoint data for other bots.
 *
 * Usage:
 * - Add strategy: .bot add strategy pathfinding
 * - Start: .bot do pathfinding start <mapId>
 * - Status: .bot do pathfinding status
 * - Stop: .bot do pathfinding stop
 *
 * The bot will:
 * 1. Enter the specified dungeon
 * 2. Explore using frontier-based algorithm
 * 3. Fight any mobs encountered
 * 4. Record path, combat locations, boss positions
 * 5. Run multiple iterations to optimize route
 * 6. Generate waypoint candidates when converged
 */
class PathfindingBotStrategy : public Strategy
{
public:
    PathfindingBotStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "pathfinding"; }
    NextAction** getDefaultActions() override;
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;

private:
    void InitCombatTriggers(std::vector<TriggerNode*>& triggers);
    void InitExplorationTriggers(std::vector<TriggerNode*>& triggers);
    void InitRecoveryTriggers(std::vector<TriggerNode*>& triggers);
};

/**
 * Multiplier to prioritize pathfinding actions
 */
class PathfindingBotMultiplier : public Multiplier
{
public:
    PathfindingBotMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "pathfinding") {}

    float GetValue(Action* action) override;
};

#endif  // _PLAYERBOT_PATHFINDINGBOTSTRATEGY_H
