/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHFINDINGBOTACTIONCONTEXT_H
#define _PLAYERBOT_PATHFINDINGBOTACTIONCONTEXT_H

#include "NamedObjectContext.h"
#include "PathfindingBotActions.h"

class PathfindingBotActionContext : public NamedObjectContext<Action>
{
public:
    PathfindingBotActionContext()
    {
        creators["pathfinding start"] = &PathfindingBotActionContext::pathfinding_start;
        creators["pathfinding stop"] = &PathfindingBotActionContext::pathfinding_stop;
        creators["pathfinding explore"] = &PathfindingBotActionContext::pathfinding_explore;
        creators["pathfinding recover stuck"] = &PathfindingBotActionContext::pathfinding_recover_stuck;
        creators["pathfinding record boss"] = &PathfindingBotActionContext::pathfinding_record_boss;
        creators["pathfinding record trash"] = &PathfindingBotActionContext::pathfinding_record_trash;
        creators["pathfinding analyze"] = &PathfindingBotActionContext::pathfinding_analyze;
        creators["pathfinding reset instance"] = &PathfindingBotActionContext::pathfinding_reset_instance;
        creators["pathfinding generate waypoints"] = &PathfindingBotActionContext::pathfinding_generate_waypoints;
        creators["pathfinding status"] = &PathfindingBotActionContext::pathfinding_status;
        creators["pathfinding handle death"] = &PathfindingBotActionContext::pathfinding_handle_death;
    }

private:
    static Action* pathfinding_start(PlayerbotAI* ai) { return new PathfindingStartAction(ai); }
    static Action* pathfinding_stop(PlayerbotAI* ai) { return new PathfindingStopAction(ai); }
    static Action* pathfinding_explore(PlayerbotAI* ai) { return new PathfindingExploreAction(ai); }
    static Action* pathfinding_recover_stuck(PlayerbotAI* ai) { return new PathfindingRecoverStuckAction(ai); }
    static Action* pathfinding_record_boss(PlayerbotAI* ai) { return new PathfindingRecordBossAction(ai); }
    static Action* pathfinding_record_trash(PlayerbotAI* ai) { return new PathfindingRecordTrashAction(ai); }
    static Action* pathfinding_analyze(PlayerbotAI* ai) { return new PathfindingAnalyzeAction(ai); }
    static Action* pathfinding_reset_instance(PlayerbotAI* ai) { return new PathfindingResetInstanceAction(ai); }
    static Action* pathfinding_generate_waypoints(PlayerbotAI* ai) { return new PathfindingGenerateWaypointsAction(ai); }
    static Action* pathfinding_status(PlayerbotAI* ai) { return new PathfindingStatusAction(ai); }
    static Action* pathfinding_handle_death(PlayerbotAI* ai) { return new PathfindingHandleDeathAction(ai); }
};

#endif  // _PLAYERBOT_PATHFINDINGBOTACTIONCONTEXT_H
