/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_TANKLEADACTIONCONTEXT_H
#define _PLAYERBOT_TANKLEADACTIONCONTEXT_H

#include "NamedObjectContext.h"
#include "TankLeadActions.h"

class TankLeadActionContext : public NamedObjectContext<Action>
{
public:
    TankLeadActionContext()
    {
        creators["move to next waypoint"] = &TankLeadActionContext::move_to_next_waypoint;
        creators["move to waypoint"] = &TankLeadActionContext::move_to_waypoint;
        creators["wait for group"] = &TankLeadActionContext::wait_for_group;
        creators["announce pull"] = &TankLeadActionContext::announce_pull;
        creators["announce movement"] = &TankLeadActionContext::announce_movement;
        creators["announce boss"] = &TankLeadActionContext::announce_boss;
        creators["wait for mana break"] = &TankLeadActionContext::wait_for_mana_break;
        creators["update dungeon progress"] = &TankLeadActionContext::update_dungeon_progress;
        creators["initialize dungeon progress"] = &TankLeadActionContext::initialize_dungeon_progress;
        creators["mark trash cleared"] = &TankLeadActionContext::mark_trash_cleared;
        creators["mark boss killed"] = &TankLeadActionContext::mark_boss_killed;
        creators["move to safe spot"] = &TankLeadActionContext::move_to_safe_spot;
        creators["pull trash"] = &TankLeadActionContext::pull_trash;
        creators["start boss encounter"] = &TankLeadActionContext::start_boss_encounter;
        creators["check group ready"] = &TankLeadActionContext::check_group_ready;
        creators["sync group progress"] = &TankLeadActionContext::sync_group_progress;
        creators["request human lead"] = &TankLeadActionContext::request_human_lead;
        creators["follow human leader"] = &TankLeadActionContext::follow_human_leader;
        // Chatter actions
        creators["dungeon chatter"] = &TankLeadActionContext::dungeon_chatter;
        creators["dungeon enter chatter"] = &TankLeadActionContext::dungeon_enter_chatter;
        creators["after combat chatter"] = &TankLeadActionContext::after_combat_chatter;
        creators["low health chatter"] = &TankLeadActionContext::low_health_chatter;
        creators["low mana chatter"] = &TankLeadActionContext::low_mana_chatter;
        creators["death chatter"] = &TankLeadActionContext::death_chatter;
        creators["resurrect chatter"] = &TankLeadActionContext::resurrect_chatter;
        creators["boss pull chatter"] = &TankLeadActionContext::boss_pull_chatter;
        creators["boss kill chatter"] = &TankLeadActionContext::boss_kill_chatter;
    }

private:
    static Action* move_to_next_waypoint(PlayerbotAI* ai) { return new MoveToNextWaypointAction(ai); }
    static Action* move_to_waypoint(PlayerbotAI* ai) { return new MoveToWaypointAction(ai); }
    static Action* wait_for_group(PlayerbotAI* ai) { return new WaitForGroupAction(ai); }
    static Action* announce_pull(PlayerbotAI* ai) { return new AnnouncePullAction(ai); }
    static Action* announce_movement(PlayerbotAI* ai) { return new AnnounceMovementAction(ai); }
    static Action* announce_boss(PlayerbotAI* ai) { return new AnnounceBossAction(ai); }
    static Action* wait_for_mana_break(PlayerbotAI* ai) { return new WaitForManaBreakAction(ai); }
    static Action* update_dungeon_progress(PlayerbotAI* ai) { return new UpdateDungeonProgressAction(ai); }
    static Action* initialize_dungeon_progress(PlayerbotAI* ai) { return new InitializeDungeonProgressAction(ai); }
    static Action* mark_trash_cleared(PlayerbotAI* ai) { return new MarkTrashClearedAction(ai); }
    static Action* mark_boss_killed(PlayerbotAI* ai) { return new MarkBossKilledAction(ai); }
    static Action* move_to_safe_spot(PlayerbotAI* ai) { return new MoveToSafeSpotAction(ai); }
    static Action* pull_trash(PlayerbotAI* ai) { return new PullTrashAction(ai); }
    static Action* start_boss_encounter(PlayerbotAI* ai) { return new StartBossEncounterAction(ai); }
    static Action* check_group_ready(PlayerbotAI* ai) { return new CheckGroupReadyAction(ai); }
    static Action* sync_group_progress(PlayerbotAI* ai) { return new SyncGroupProgressAction(ai); }
    static Action* request_human_lead(PlayerbotAI* ai) { return new RequestHumanLeadAction(ai); }
    static Action* follow_human_leader(PlayerbotAI* ai) { return new FollowHumanLeaderAction(ai); }
    // Chatter actions
    static Action* dungeon_chatter(PlayerbotAI* ai) { return new DungeonChatterAction(ai); }
    static Action* dungeon_enter_chatter(PlayerbotAI* ai) { return new DungeonEnterChatterAction(ai); }
    static Action* after_combat_chatter(PlayerbotAI* ai) { return new AfterCombatChatterAction(ai); }
    static Action* low_health_chatter(PlayerbotAI* ai) { return new LowHealthChatterAction(ai); }
    static Action* low_mana_chatter(PlayerbotAI* ai) { return new LowManaChatterAction(ai); }
    static Action* death_chatter(PlayerbotAI* ai) { return new DeathChatterAction(ai); }
    static Action* resurrect_chatter(PlayerbotAI* ai) { return new ResurrectChatterAction(ai); }
    static Action* boss_pull_chatter(PlayerbotAI* ai) { return new BossPullChatterAction(ai); }
    static Action* boss_kill_chatter(PlayerbotAI* ai) { return new BossKillChatterAction(ai); }
};

#endif
