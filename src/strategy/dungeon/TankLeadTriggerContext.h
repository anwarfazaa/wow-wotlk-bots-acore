/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_TANKLEADTRIGGERCONTEXT_H
#define _PLAYERBOT_TANKLEADTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "TankLeadTriggers.h"

class TankLeadTriggerContext : public NamedObjectContext<Trigger>
{
public:
    TankLeadTriggerContext()
    {
        creators["tank lead enabled"] = &TankLeadTriggerContext::tank_lead_enabled;
        creators["at dungeon waypoint"] = &TankLeadTriggerContext::at_dungeon_waypoint;
        creators["should move to next waypoint"] = &TankLeadTriggerContext::should_move_to_next_waypoint;
        creators["group not ready"] = &TankLeadTriggerContext::group_not_ready;
        creators["wait for group"] = &TankLeadTriggerContext::wait_for_group;
        creators["trash pack ahead"] = &TankLeadTriggerContext::trash_pack_ahead;
        creators["boss ahead"] = &TankLeadTriggerContext::boss_ahead;
        creators["safe spot reached"] = &TankLeadTriggerContext::safe_spot_reached;
        creators["healer needs mana break"] = &TankLeadTriggerContext::healer_needs_mana_break;
        creators["group spread out"] = &TankLeadTriggerContext::group_spread_out;
        creators["dungeon complete"] = &TankLeadTriggerContext::dungeon_complete;
        creators["no dungeon path"] = &TankLeadTriggerContext::no_dungeon_path;
        creators["pull ready"] = &TankLeadTriggerContext::pull_ready;
        creators["waypoint requires clear"] = &TankLeadTriggerContext::waypoint_requires_clear;
        creators["group member far behind"] = &TankLeadTriggerContext::group_member_far_behind;
        creators["group member dead dungeon"] = &TankLeadTriggerContext::group_member_dead_dungeon;
        // Chatter triggers
        creators["dungeon chatter"] = &TankLeadTriggerContext::dungeon_chatter;
        creators["dungeon enter chatter"] = &TankLeadTriggerContext::dungeon_enter_chatter;
        creators["after combat chatter"] = &TankLeadTriggerContext::after_combat_chatter;
        creators["low health chatter"] = &TankLeadTriggerContext::low_health_chatter;
        creators["low mana chatter"] = &TankLeadTriggerContext::low_mana_chatter;
        creators["death chatter"] = &TankLeadTriggerContext::death_chatter;
        creators["resurrect chatter"] = &TankLeadTriggerContext::resurrect_chatter;
    }

private:
    static Trigger* tank_lead_enabled(PlayerbotAI* ai) { return new TankLeadEnabledTrigger(ai); }
    static Trigger* at_dungeon_waypoint(PlayerbotAI* ai) { return new AtDungeonWaypointTrigger(ai); }
    static Trigger* should_move_to_next_waypoint(PlayerbotAI* ai) { return new ShouldMoveToNextWaypointTrigger(ai); }
    static Trigger* group_not_ready(PlayerbotAI* ai) { return new GroupNotReadyTrigger(ai); }
    static Trigger* wait_for_group(PlayerbotAI* ai) { return new WaitForGroupTrigger(ai); }
    static Trigger* trash_pack_ahead(PlayerbotAI* ai) { return new TrashPackAheadTrigger(ai); }
    static Trigger* boss_ahead(PlayerbotAI* ai) { return new BossAheadTrigger(ai); }
    static Trigger* safe_spot_reached(PlayerbotAI* ai) { return new SafeSpotReachedTrigger(ai); }
    static Trigger* healer_needs_mana_break(PlayerbotAI* ai) { return new HealerNeedsManaBreakTrigger(ai); }
    static Trigger* group_spread_out(PlayerbotAI* ai) { return new GroupSpreadOutTrigger(ai); }
    static Trigger* dungeon_complete(PlayerbotAI* ai) { return new DungeonCompleteTrigger(ai); }
    static Trigger* no_dungeon_path(PlayerbotAI* ai) { return new NoDungeonPathTrigger(ai); }
    static Trigger* pull_ready(PlayerbotAI* ai) { return new PullReadyTrigger(ai); }
    static Trigger* waypoint_requires_clear(PlayerbotAI* ai) { return new WaypointRequiresClearTrigger(ai); }
    static Trigger* group_member_far_behind(PlayerbotAI* ai) { return new GroupMemberFarBehindTrigger(ai); }
    static Trigger* group_member_dead_dungeon(PlayerbotAI* ai) { return new GroupMemberDeadDungeonTrigger(ai); }
    // Chatter triggers
    static Trigger* dungeon_chatter(PlayerbotAI* ai) { return new DungeonChatterTrigger(ai); }
    static Trigger* dungeon_enter_chatter(PlayerbotAI* ai) { return new DungeonEnterChatterTrigger(ai); }
    static Trigger* after_combat_chatter(PlayerbotAI* ai) { return new AfterCombatChatterTrigger(ai); }
    static Trigger* low_health_chatter(PlayerbotAI* ai) { return new LowHealthChatterTrigger(ai); }
    static Trigger* low_mana_chatter(PlayerbotAI* ai) { return new LowManaChatterTrigger(ai); }
    static Trigger* death_chatter(PlayerbotAI* ai) { return new DeathChatterTrigger(ai); }
    static Trigger* resurrect_chatter(PlayerbotAI* ai) { return new ResurrectChatterTrigger(ai); }
};

#endif
