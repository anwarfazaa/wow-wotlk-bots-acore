/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHFINDINGBOTTRIGGERCONTEXT_H
#define _PLAYERBOT_PATHFINDINGBOTTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "PathfindingBotTriggers.h"

class PathfindingBotTriggerContext : public NamedObjectContext<Trigger>
{
public:
    PathfindingBotTriggerContext()
    {
        creators["pathfinding should explore"] = &PathfindingBotTriggerContext::pathfinding_should_explore;
        creators["pathfinding is stuck"] = &PathfindingBotTriggerContext::pathfinding_is_stuck;
        creators["pathfinding boss encountered"] = &PathfindingBotTriggerContext::pathfinding_boss_encountered;
        creators["pathfinding trash encountered"] = &PathfindingBotTriggerContext::pathfinding_trash_encountered;
        creators["pathfinding run complete"] = &PathfindingBotTriggerContext::pathfinding_run_complete;
        creators["pathfinding should analyze"] = &PathfindingBotTriggerContext::pathfinding_should_analyze;
        creators["pathfinding converged"] = &PathfindingBotTriggerContext::pathfinding_converged;
        creators["pathfinding should reset"] = &PathfindingBotTriggerContext::pathfinding_should_reset;
        creators["pathfinding bot dead"] = &PathfindingBotTriggerContext::pathfinding_bot_dead;
        creators["pathfinding need move"] = &PathfindingBotTriggerContext::pathfinding_need_move;
    }

private:
    static Trigger* pathfinding_should_explore(PlayerbotAI* ai) { return new PathfindingShouldExploreTrigger(ai); }
    static Trigger* pathfinding_is_stuck(PlayerbotAI* ai) { return new PathfindingIsStuckTrigger(ai); }
    static Trigger* pathfinding_boss_encountered(PlayerbotAI* ai) { return new PathfindingBossEncounteredTrigger(ai); }
    static Trigger* pathfinding_trash_encountered(PlayerbotAI* ai) { return new PathfindingTrashEncounteredTrigger(ai); }
    static Trigger* pathfinding_run_complete(PlayerbotAI* ai) { return new PathfindingRunCompleteTrigger(ai); }
    static Trigger* pathfinding_should_analyze(PlayerbotAI* ai) { return new PathfindingShouldAnalyzeTrigger(ai); }
    static Trigger* pathfinding_converged(PlayerbotAI* ai) { return new PathfindingConvergedTrigger(ai); }
    static Trigger* pathfinding_should_reset(PlayerbotAI* ai) { return new PathfindingShouldResetTrigger(ai); }
    static Trigger* pathfinding_bot_dead(PlayerbotAI* ai) { return new PathfindingBotDeadTrigger(ai); }
    static Trigger* pathfinding_need_move(PlayerbotAI* ai) { return new PathfindingNeedMoveTrigger(ai); }
};

#endif  // _PLAYERBOT_PATHFINDINGBOTTRIGGERCONTEXT_H
