/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "PathfindingBotStrategy.h"
#include "PathfindingBotManager.h"
#include "PathfindingBotActions.h"
#include "PathfindingBotTriggers.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"

PathfindingBotStrategy::PathfindingBotStrategy(PlayerbotAI* botAI) : Strategy(botAI)
{
}

NextAction** PathfindingBotStrategy::getDefaultActions()
{
    return NextAction::array(0,
        new NextAction("pathfinding explore", ACTION_NORMAL),
        nullptr);
}

void PathfindingBotStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    InitCombatTriggers(triggers);
    InitExplorationTriggers(triggers);
    InitRecoveryTriggers(triggers);
}

void PathfindingBotStrategy::InitCombatTriggers(std::vector<TriggerNode*>& triggers)
{
    // Boss encounter handling
    triggers.push_back(new TriggerNode(
        "pathfinding boss encountered",
        NextAction::array(0,
            new NextAction("pathfinding record boss", ACTION_HIGH + 5),
            nullptr)));

    // Trash encounter handling
    triggers.push_back(new TriggerNode(
        "pathfinding trash encountered",
        NextAction::array(0,
            new NextAction("pathfinding record trash", ACTION_HIGH),
            nullptr)));

    // Death handling
    triggers.push_back(new TriggerNode(
        "pathfinding bot dead",
        NextAction::array(0,
            new NextAction("pathfinding handle death", ACTION_EMERGENCY),
            nullptr)));
}

void PathfindingBotStrategy::InitExplorationTriggers(std::vector<TriggerNode*>& triggers)
{
    // Main exploration trigger
    triggers.push_back(new TriggerNode(
        "pathfinding should explore",
        NextAction::array(0,
            new NextAction("pathfinding explore", ACTION_NORMAL + 5),
            nullptr)));

    // Run completion
    triggers.push_back(new TriggerNode(
        "pathfinding run complete",
        NextAction::array(0,
            new NextAction("pathfinding analyze", ACTION_HIGH + 10),
            nullptr)));

    // Convergence detection
    triggers.push_back(new TriggerNode(
        "pathfinding converged",
        NextAction::array(0,
            new NextAction("pathfinding generate waypoints", ACTION_HIGH + 15),
            nullptr)));

    // Instance reset
    triggers.push_back(new TriggerNode(
        "pathfinding should reset",
        NextAction::array(0,
            new NextAction("pathfinding reset instance", ACTION_HIGH),
            nullptr)));

    // Movement needed
    triggers.push_back(new TriggerNode(
        "pathfinding need move",
        NextAction::array(0,
            new NextAction("pathfinding explore", ACTION_NORMAL),
            nullptr)));
}

void PathfindingBotStrategy::InitRecoveryTriggers(std::vector<TriggerNode*>& triggers)
{
    // Stuck recovery
    triggers.push_back(new TriggerNode(
        "pathfinding is stuck",
        NextAction::array(0,
            new NextAction("pathfinding recover stuck", ACTION_HIGH + 20),
            nullptr)));
}

void PathfindingBotStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new PathfindingBotMultiplier(botAI));
}

float PathfindingBotMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    Player* bot = botAI->GetBot();
    if (!bot)
        return 1.0f;

    // Check if pathfinding is active
    if (!sPathfindingBot->IsActive(bot))
        return 1.0f;

    std::string name = action->getName();

    // Boost pathfinding actions when active
    if (name.find("pathfinding") != std::string::npos)
    {
        return 1.5f;  // 50% boost to pathfinding actions
    }

    // Allow combat actions during combat
    if (bot->IsInCombat())
    {
        return 1.0f;  // Normal priority for combat
    }

    // Reduce non-pathfinding actions when exploring
    PathfindingState state = sPathfindingBot->GetState(bot);
    if (state == PathfindingState::EXPLORING)
    {
        // Don't reduce movement actions
        if (name.find("move") != std::string::npos ||
            name.find("follow") != std::string::npos)
        {
            return 0.5f;  // Reduce following behavior
        }

        // Reduce emotes, chat, etc during pathfinding
        if (name.find("emote") != std::string::npos ||
            name.find("say") != std::string::npos ||
            name.find("chat") != std::string::npos)
        {
            return 0.0f;  // Disable social actions
        }
    }

    return 1.0f;
}
