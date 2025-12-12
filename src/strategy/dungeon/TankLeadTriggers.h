/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_TANKLEADTRIGGERS_H
#define _PLAYERBOT_TANKLEADTRIGGERS_H

#include "Trigger.h"
#include "DungeonNavigator.h"
#include "DungeonChatter.h"

class PlayerbotAI;

/**
 * TankLeadEnabledTrigger - Check if tank lead strategy should be active
 */
class TankLeadEnabledTrigger : public Trigger
{
public:
    TankLeadEnabledTrigger(PlayerbotAI* ai) : Trigger(ai, "tank lead enabled", 5) {}
    bool IsActive() override;
};

/**
 * AtDungeonWaypointTrigger - Tank is at current waypoint
 */
class AtDungeonWaypointTrigger : public Trigger
{
public:
    AtDungeonWaypointTrigger(PlayerbotAI* ai) : Trigger(ai, "at dungeon waypoint", 1) {}
    bool IsActive() override;
};

/**
 * ShouldMoveToNextWaypointTrigger - Tank should move to next waypoint
 */
class ShouldMoveToNextWaypointTrigger : public Trigger
{
public:
    ShouldMoveToNextWaypointTrigger(PlayerbotAI* ai) : Trigger(ai, "should move to next waypoint", 1) {}
    bool IsActive() override;

private:
    bool IsCurrentWaypointClear();
    bool IsGroupReady();
};

/**
 * GroupNotReadyTrigger - Group is not ready to proceed
 */
class GroupNotReadyTrigger : public Trigger
{
public:
    GroupNotReadyTrigger(PlayerbotAI* ai) : Trigger(ai, "group not ready", 2) {}
    bool IsActive() override;
};

/**
 * WaitForGroupTrigger - Tank should wait at current waypoint
 */
class WaitForGroupTrigger : public Trigger
{
public:
    WaitForGroupTrigger(PlayerbotAI* ai) : Trigger(ai, "wait for group", 2) {}
    bool IsActive() override;
};

/**
 * TrashPackAheadTrigger - Trash pack detected at next waypoint
 */
class TrashPackAheadTrigger : public Trigger
{
public:
    TrashPackAheadTrigger(PlayerbotAI* ai) : Trigger(ai, "trash pack ahead", 2) {}
    bool IsActive() override;
};

/**
 * BossAheadTrigger - Boss encounter at next waypoint
 */
class BossAheadTrigger : public Trigger
{
public:
    BossAheadTrigger(PlayerbotAI* ai) : Trigger(ai, "boss ahead", 2) {}
    bool IsActive() override;
};

/**
 * SafeSpotReachedTrigger - Tank reached a safe spot
 */
class SafeSpotReachedTrigger : public Trigger
{
public:
    SafeSpotReachedTrigger(PlayerbotAI* ai) : Trigger(ai, "safe spot reached", 2) {}
    bool IsActive() override;
};

/**
 * HealerNeedsManaBreakTrigger - Healer needs mana break
 */
class HealerNeedsManaBreakTrigger : public Trigger
{
public:
    HealerNeedsManaBreakTrigger(PlayerbotAI* ai) : Trigger(ai, "healer needs mana break", 3) {}
    bool IsActive() override;
};

/**
 * GroupSpreadOutTrigger - Group is too spread out
 */
class GroupSpreadOutTrigger : public Trigger
{
public:
    GroupSpreadOutTrigger(PlayerbotAI* ai) : Trigger(ai, "group spread out", 2) {}
    bool IsActive() override;
};

/**
 * DungeonCompleteTrigger - Dungeon navigation complete
 */
class DungeonCompleteTrigger : public Trigger
{
public:
    DungeonCompleteTrigger(PlayerbotAI* ai) : Trigger(ai, "dungeon complete", 5) {}
    bool IsActive() override;
};

/**
 * NoDungeonPathTrigger - No path data for current dungeon
 */
class NoDungeonPathTrigger : public Trigger
{
public:
    NoDungeonPathTrigger(PlayerbotAI* ai) : Trigger(ai, "no dungeon path", 10) {}
    bool IsActive() override;
};

/**
 * PullReadyTrigger - Ready to pull the next pack
 */
class PullReadyTrigger : public Trigger
{
public:
    PullReadyTrigger(PlayerbotAI* ai) : Trigger(ai, "pull ready", 1) {}
    bool IsActive() override;
};

/**
 * WaypointRequiresClearTrigger - Current waypoint requires clearing before proceeding
 */
class WaypointRequiresClearTrigger : public Trigger
{
public:
    WaypointRequiresClearTrigger(PlayerbotAI* ai) : Trigger(ai, "waypoint requires clear", 2) {}
    bool IsActive() override;
};

/**
 * GroupMemberFarBehindTrigger - A group member is too far behind
 */
class GroupMemberFarBehindTrigger : public Trigger
{
public:
    GroupMemberFarBehindTrigger(PlayerbotAI* ai) : Trigger(ai, "group member far behind", 3) {}
    bool IsActive() override;
};

/**
 * GroupMemberDeadTrigger - A group member is dead
 */
class GroupMemberDeadDungeonTrigger : public Trigger
{
public:
    GroupMemberDeadDungeonTrigger(PlayerbotAI* ai) : Trigger(ai, "group member dead dungeon", 3) {}
    bool IsActive() override;
};

/**
 * DungeonChatterTrigger - Random chatter during dungeons
 */
class DungeonChatterTrigger : public Trigger
{
public:
    DungeonChatterTrigger(PlayerbotAI* ai) : Trigger(ai, "dungeon chatter", 15) {}
    bool IsActive() override;
};

/**
 * DungeonEnterChatterTrigger - Chatter when entering dungeon
 */
class DungeonEnterChatterTrigger : public Trigger
{
public:
    DungeonEnterChatterTrigger(PlayerbotAI* ai) : Trigger(ai, "dungeon enter chatter", 60) {}
    bool IsActive() override;

private:
    static std::unordered_map<uint64, uint32> s_lastDungeonMap;
};

/**
 * AfterCombatChatterTrigger - Chatter after combat ends
 */
class AfterCombatChatterTrigger : public Trigger
{
public:
    AfterCombatChatterTrigger(PlayerbotAI* ai) : Trigger(ai, "after combat chatter", 5) {}
    bool IsActive() override;

private:
    static std::unordered_map<uint64, bool> s_wasInCombat;
};

/**
 * LowHealthChatterTrigger - Chatter when health is low
 */
class LowHealthChatterTrigger : public Trigger
{
public:
    LowHealthChatterTrigger(PlayerbotAI* ai) : Trigger(ai, "low health chatter", 5) {}
    bool IsActive() override;
};

/**
 * LowManaChatterTrigger - Chatter when mana is low
 */
class LowManaChatterTrigger : public Trigger
{
public:
    LowManaChatterTrigger(PlayerbotAI* ai) : Trigger(ai, "low mana chatter", 5) {}
    bool IsActive() override;
};

/**
 * DeathChatterTrigger - Chatter when dying
 */
class DeathChatterTrigger : public Trigger
{
public:
    DeathChatterTrigger(PlayerbotAI* ai) : Trigger(ai, "death chatter", 1) {}
    bool IsActive() override;

private:
    static std::unordered_map<uint64, bool> s_wasAlive;
};

/**
 * ResurrectChatterTrigger - Chatter when resurrected
 */
class ResurrectChatterTrigger : public Trigger
{
public:
    ResurrectChatterTrigger(PlayerbotAI* ai) : Trigger(ai, "resurrect chatter", 1) {}
    bool IsActive() override;

private:
    static std::unordered_map<uint64, bool> s_wasDead;
};

#endif
