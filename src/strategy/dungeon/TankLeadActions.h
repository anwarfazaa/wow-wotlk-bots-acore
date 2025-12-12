/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_TANKLEADACTIONS_H
#define _PLAYERBOT_TANKLEADACTIONS_H

#include "Action.h"
#include "MovementActions.h"
#include "DungeonChatter.h"
#include "DungeonNavigator.h"

class PlayerbotAI;

/**
 * MoveToNextWaypointAction - Tank moves to next dungeon waypoint
 */
class MoveToNextWaypointAction : public MovementAction
{
public:
    MoveToNextWaypointAction(PlayerbotAI* ai) : MovementAction(ai, "move to next waypoint") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool UpdateProgress();
};

/**
 * MoveToWaypointAction - Move to specific waypoint by index
 */
class MoveToWaypointAction : public MovementAction
{
public:
    MoveToWaypointAction(PlayerbotAI* ai) : MovementAction(ai, "move to waypoint") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * WaitForGroupAction - Tank waits for group to catch up
 */
class WaitForGroupAction : public Action
{
public:
    WaitForGroupAction(PlayerbotAI* ai) : Action(ai, "wait for group") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    float GetFurthestMemberDistance();
};

/**
 * AnnouncePullAction - Announce intent to pull
 */
class AnnouncePullAction : public Action
{
public:
    AnnouncePullAction(PlayerbotAI* ai) : Action(ai, "announce pull") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * AnnounceMovementAction - Announce movement to group
 */
class AnnounceMovementAction : public Action
{
public:
    AnnounceMovementAction(PlayerbotAI* ai) : Action(ai, "announce movement") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * AnnounceBossAction - Announce boss encounter
 */
class AnnounceBossAction : public Action
{
public:
    AnnounceBossAction(PlayerbotAI* ai) : Action(ai, "announce boss") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * WaitForManaBreakAction - Wait for healers to drink
 */
class WaitForManaBreakAction : public Action
{
public:
    WaitForManaBreakAction(PlayerbotAI* ai) : Action(ai, "wait for mana break") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * UpdateDungeonProgressAction - Update progress tracking
 */
class UpdateDungeonProgressAction : public Action
{
public:
    UpdateDungeonProgressAction(PlayerbotAI* ai) : Action(ai, "update dungeon progress") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * InitializeDungeonProgressAction - Set up progress for new dungeon
 */
class InitializeDungeonProgressAction : public Action
{
public:
    InitializeDungeonProgressAction(PlayerbotAI* ai) : Action(ai, "initialize dungeon progress") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * MarkTrashClearedAction - Mark current trash pack as cleared
 */
class MarkTrashClearedAction : public Action
{
public:
    MarkTrashClearedAction(PlayerbotAI* ai) : Action(ai, "mark trash cleared") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * MarkBossKilledAction - Mark boss as killed
 */
class MarkBossKilledAction : public Action
{
public:
    MarkBossKilledAction(PlayerbotAI* ai) : Action(ai, "mark boss killed") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * MoveToSafeSpotAction - Move to nearest safe spot
 */
class MoveToSafeSpotAction : public MovementAction
{
public:
    MoveToSafeSpotAction(PlayerbotAI* ai) : MovementAction(ai, "move to safe spot") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * PullTrashAction - Pull the next trash pack
 */
class PullTrashAction : public Action
{
public:
    PullTrashAction(PlayerbotAI* ai) : Action(ai, "pull trash") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * StartBossEncounterAction - Initiate boss fight
 */
class StartBossEncounterAction : public Action
{
public:
    StartBossEncounterAction(PlayerbotAI* ai) : Action(ai, "start boss encounter") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * CheckGroupReadyAction - Check and update group ready state
 */
class CheckGroupReadyAction : public Action
{
public:
    CheckGroupReadyAction(PlayerbotAI* ai) : Action(ai, "check group ready") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * SyncGroupProgressAction - Sync progress with group members
 */
class SyncGroupProgressAction : public Action
{
public:
    SyncGroupProgressAction(PlayerbotAI* ai) : Action(ai, "sync group progress") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * RequestHumanLeadAction - Ask human player to lead when no waypoints exist
 * Tank will follow the human player and request them to lead the dungeon
 */
class RequestHumanLeadAction : public Action
{
public:
    RequestHumanLeadAction(PlayerbotAI* ai) : Action(ai, "request human lead") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Player* FindHumanLeader();
    bool HasAnnouncedRecently();

    static std::unordered_map<uint64, uint32> s_lastAnnounceTime;
    static constexpr uint32 ANNOUNCE_COOLDOWN_MS = 60000;  // 1 minute between announcements
};

/**
 * FollowHumanLeaderAction - Follow human player through unknown dungeon
 */
class FollowHumanLeaderAction : public MovementAction
{
public:
    FollowHumanLeaderAction(PlayerbotAI* ai) : MovementAction(ai, "follow human leader") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Player* FindHumanLeader();
};

/**
 * DungeonChatterAction - Say funny lines during dungeons
 */
class DungeonChatterAction : public Action
{
public:
    DungeonChatterAction(PlayerbotAI* ai, ChatterCategory category = ChatterCategory::RANDOM)
        : Action(ai, "dungeon chatter"), m_category(category) {}
    bool Execute(Event event) override;
    bool isUseful() override;

    void SetCategory(ChatterCategory cat) { m_category = cat; }

private:
    ChatterCategory m_category;
};

/**
 * Specialized chatter actions for specific situations
 */
class DungeonEnterChatterAction : public DungeonChatterAction
{
public:
    DungeonEnterChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::ENTERING_DUNGEON)
    {
        name = "dungeon enter chatter";
    }
};

class AfterCombatChatterAction : public DungeonChatterAction
{
public:
    AfterCombatChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::AFTER_KILL)
    {
        name = "after combat chatter";
    }
};

class LowHealthChatterAction : public DungeonChatterAction
{
public:
    LowHealthChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::LOW_HEALTH)
    {
        name = "low health chatter";
    }
};

class LowManaChatterAction : public DungeonChatterAction
{
public:
    LowManaChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::LOW_MANA)
    {
        name = "low mana chatter";
    }
};

class DeathChatterAction : public DungeonChatterAction
{
public:
    DeathChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::DEATH)
    {
        name = "death chatter";
    }
};

class ResurrectChatterAction : public DungeonChatterAction
{
public:
    ResurrectChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::RESURRECT)
    {
        name = "resurrect chatter";
    }
};

class BossPullChatterAction : public DungeonChatterAction
{
public:
    BossPullChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::BOSS_PULL)
    {
        name = "boss pull chatter";
    }
};

class BossKillChatterAction : public DungeonChatterAction
{
public:
    BossKillChatterAction(PlayerbotAI* ai)
        : DungeonChatterAction(ai, ChatterCategory::BOSS_KILL)
    {
        name = "boss kill chatter";
    }
};

#endif
