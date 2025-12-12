/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "TankLeadTriggers.h"
#include "DungeonChatter.h"
#include "DungeonNavigator.h"
#include "Group.h"
#include "GroupAICoordinator.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PlayerbotAIConfig.h"

// ============================================================================
// TankLeadEnabledTrigger
// ============================================================================

bool TankLeadEnabledTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a group
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Must be a tank role
    if (!botAI->ContainsStrategy(STRATEGY_TYPE_TANK))
        return false;

    // Must be in a dungeon with path data
    uint32 mapId = bot->GetMapId();
    if (!sDungeonNavigator->HasDungeonPath(mapId))
        return false;

    // Must not be in combat (tank lead is for navigation between pulls)
    if (bot->IsInCombat())
        return false;

    return true;
}

// ============================================================================
// AtDungeonWaypointTrigger
// ============================================================================

bool AtDungeonWaypointTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    return sDungeonNavigator->IsAtWaypoint(mapId, pos, progress->currentWaypointIndex);
}

// ============================================================================
// ShouldMoveToNextWaypointTrigger
// ============================================================================

bool ShouldMoveToNextWaypointTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must not be in combat
    if (bot->IsInCombat())
        return false;

    // Must be at current waypoint
    if (!AI_VALUE(bool, "at dungeon waypoint"))
        return true;  // Not at waypoint, should move to it first

    // Check if current waypoint is clear
    if (!IsCurrentWaypointClear())
        return false;

    // Check if group is ready
    if (!IsGroupReady())
        return false;

    return true;
}

bool ShouldMoveToNextWaypointTrigger::IsCurrentWaypointClear()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return true;

    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return true;

    const DungeonWaypoint* wp = path->GetWaypoint(progress->currentWaypointIndex);
    if (!wp)
        return true;

    // If waypoint requires clear and it's a trash pack, check if cleared
    if (wp->requiresClear && wp->IsTrash())
    {
        return progress->HasClearedTrash(wp->trashPackId);
    }

    // If waypoint is a boss, check if killed
    if (wp->IsBoss())
    {
        return progress->HasKilledBoss(wp->bossEntry);
    }

    return true;
}

bool ShouldMoveToNextWaypointTrigger::IsGroupReady()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;  // Solo is always "ready"

    uint32 groupId = group->GetGUID().GetCounter();
    GroupCoordinatorData* data = sGroupAICoordinator->GetGroupData(groupId);
    if (!data)
        return true;

    // Check healer mana using config threshold
    uint32 manaThreshold = sPlayerbotAIConfig->tankLeadManaBreakThreshold;
    if (data->NeedsManaBreak(static_cast<uint8>(manaThreshold)))
        return false;

    // Check group health - use 70% as default, consistent with pull ready
    if (data->GetAverageHealthPct() < 70.0f)
        return false;

    // Check if anyone is dead
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && !member->IsAlive())
            return false;
    }

    return true;
}

// ============================================================================
// GroupNotReadyTrigger
// ============================================================================

bool GroupNotReadyTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 groupId = group->GetGUID().GetCounter();
    return !sGroupAICoordinator->IsGroupReady(groupId);
}

// ============================================================================
// WaitForGroupTrigger
// ============================================================================

bool WaitForGroupTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    const DungeonWaypoint* wp = path->GetWaypoint(progress->currentWaypointIndex);
    if (!wp)
        return false;

    // Check if current waypoint requires waiting
    if (!wp->waitForGroup)
        return false;

    // Check if we're at the waypoint
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    if (!sDungeonNavigator->IsAtWaypoint(mapId, pos, progress->currentWaypointIndex))
        return false;

    // Check if group is spread out
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        float dist = bot->GetDistance(member);
        if (dist > 40.0f)  // Someone is far behind
            return true;
    }

    return false;
}

// ============================================================================
// TrashPackAheadTrigger
// ============================================================================

bool TrashPackAheadTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    // Check next waypoint
    uint16 nextIndex = progress->currentWaypointIndex + 1;
    const DungeonWaypoint* nextWp = path->GetWaypoint(nextIndex);
    if (!nextWp)
        return false;

    return nextWp->IsTrash() && !progress->HasClearedTrash(nextWp->trashPackId);
}

// ============================================================================
// BossAheadTrigger
// ============================================================================

bool BossAheadTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    // Check next waypoint
    uint16 nextIndex = progress->currentWaypointIndex + 1;
    const DungeonWaypoint* nextWp = path->GetWaypoint(nextIndex);
    if (!nextWp)
        return false;

    return nextWp->IsBoss() && !progress->HasKilledBoss(nextWp->bossEntry);
}

// ============================================================================
// SafeSpotReachedTrigger
// ============================================================================

bool SafeSpotReachedTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    const DungeonWaypoint* wp = sDungeonNavigator->GetWaypointAtPosition(mapId, pos);
    return wp && wp->IsSafeSpot();
}

// ============================================================================
// HealerNeedsManaBreakTrigger
// ============================================================================

bool HealerNeedsManaBreakTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 groupId = group->GetGUID().GetCounter();
    uint32 manaThreshold = sPlayerbotAIConfig->tankLeadManaBreakThreshold;
    return sGroupAICoordinator->GroupNeedsManaBreak(groupId, static_cast<uint8>(manaThreshold));
}

// ============================================================================
// GroupSpreadOutTrigger
// ============================================================================

bool GroupSpreadOutTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float maxDistance = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        float dist = bot->GetDistance(member);
        if (dist > maxDistance)
            maxDistance = dist;
    }

    return maxDistance > 50.0f;  // Group considered spread out if > 50 yards apart
}

// ============================================================================
// DungeonCompleteTrigger
// ============================================================================

bool DungeonCompleteTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    return progress->isComplete;
}

// ============================================================================
// NoDungeonPathTrigger
// ============================================================================

bool NoDungeonPathTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Only trigger in dungeon maps
    if (!bot->GetMap()->IsDungeon())
        return false;

    return !sDungeonNavigator->HasDungeonPath(bot->GetMapId());
}

// ============================================================================
// PullReadyTrigger
// ============================================================================

bool PullReadyTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be a tank
    if (!botAI->ContainsStrategy(STRATEGY_TYPE_TANK))
        return false;

    // Must not be in combat
    if (bot->IsInCombat())
        return false;

    // Group must be ready
    Group* group = bot->GetGroup();
    if (group)
    {
        uint32 groupId = group->GetGUID().GetCounter();
        GroupCoordinatorData* data = sGroupAICoordinator->GetGroupData(groupId);
        if (data)
        {
            // Check resources using config threshold
            uint32 manaThreshold = sPlayerbotAIConfig->tankLeadManaBreakThreshold;
            if (data->NeedsManaBreak(static_cast<uint8>(manaThreshold)))
                return false;
            // Use 70% health threshold to be consistent with IsGroupReady
            if (data->GetAverageHealthPct() < 70.0f)
                return false;
        }
    }

    // Check if at a trash waypoint
    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    const DungeonWaypoint* wp = sDungeonNavigator->GetWaypointAtPosition(mapId, pos, 15.0f);
    if (!wp)
        return false;

    // Must be at a trash or boss waypoint
    return wp->IsTrash() || wp->IsBoss();
}

// ============================================================================
// WaypointRequiresClearTrigger
// ============================================================================

bool WaypointRequiresClearTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    const DungeonWaypoint* wp = path->GetWaypoint(progress->currentWaypointIndex);
    if (!wp)
        return false;

    if (!wp->requiresClear)
        return false;

    // Check if already cleared
    if (wp->IsTrash() && progress->HasClearedTrash(wp->trashPackId))
        return false;
    if (wp->IsBoss() && progress->HasKilledBoss(wp->bossEntry))
        return false;

    return true;
}

// ============================================================================
// GroupMemberFarBehindTrigger
// ============================================================================

bool GroupMemberFarBehindTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        float dist = bot->GetDistance(member);
        if (dist > 60.0f)
            return true;
    }

    return false;
}

// ============================================================================
// GroupMemberDeadDungeonTrigger
// ============================================================================

bool GroupMemberDeadDungeonTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Only in dungeons
    if (!bot->GetMap()->IsDungeon())
        return false;

    // Must not be in combat
    if (bot->IsInCombat())
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && !member->IsAlive())
            return true;
    }

    return false;
}

// ============================================================================
// DungeonChatterTrigger
// ============================================================================

bool DungeonChatterTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    if (!bot->GetMap()->IsDungeon())
        return false;

    // Must not be in combat
    if (bot->IsInCombat())
        return false;

    // Check if chatter system allows it
    return sDungeonChatter->ShouldChatter(bot, ChatterCategory::RANDOM);
}

// ============================================================================
// DungeonEnterChatterTrigger
// ============================================================================

std::unordered_map<uint64, uint32> DungeonEnterChatterTrigger::s_lastDungeonMap;

bool DungeonEnterChatterTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    if (!bot->GetMap()->IsDungeon())
        return false;

    uint64 guid = bot->GetGUID().GetCounter();
    uint32 mapId = bot->GetMapId();

    auto it = s_lastDungeonMap.find(guid);
    if (it == s_lastDungeonMap.end() || it->second != mapId)
    {
        // New dungeon or first time
        s_lastDungeonMap[guid] = mapId;
        return sDungeonChatter->ShouldChatter(bot, ChatterCategory::ENTERING_DUNGEON);
    }

    return false;
}

// ============================================================================
// AfterCombatChatterTrigger
// ============================================================================

std::unordered_map<uint64, bool> AfterCombatChatterTrigger::s_wasInCombat;

bool AfterCombatChatterTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    if (!bot->GetMap()->IsDungeon())
        return false;

    uint64 guid = bot->GetGUID().GetCounter();
    bool inCombat = bot->IsInCombat();
    bool wasInCombat = s_wasInCombat[guid];

    s_wasInCombat[guid] = inCombat;

    // Trigger when combat ends
    if (wasInCombat && !inCombat)
    {
        return sDungeonChatter->ShouldChatter(bot, ChatterCategory::AFTER_KILL);
    }

    return false;
}

// ============================================================================
// LowHealthChatterTrigger
// ============================================================================

bool LowHealthChatterTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    if (!bot->GetMap()->IsDungeon())
        return false;

    // Must be in combat with low health
    if (!bot->IsInCombat())
        return false;

    if (bot->GetHealthPct() > 25.0f)
        return false;

    return sDungeonChatter->ShouldChatter(bot, ChatterCategory::LOW_HEALTH);
}

// ============================================================================
// LowManaChatterTrigger
// ============================================================================

bool LowManaChatterTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    if (!bot->GetMap()->IsDungeon())
        return false;

    // Must not be in combat
    if (bot->IsInCombat())
        return false;

    // Must use mana and be low
    if (bot->GetMaxPower(POWER_MANA) == 0)
        return false;

    if (bot->GetPowerPct(POWER_MANA) > 20.0f)
        return false;

    return sDungeonChatter->ShouldChatter(bot, ChatterCategory::LOW_MANA);
}

// ============================================================================
// DeathChatterTrigger
// ============================================================================

std::unordered_map<uint64, bool> DeathChatterTrigger::s_wasAlive;

bool DeathChatterTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    if (!bot->GetMap()->IsDungeon())
        return false;

    uint64 guid = bot->GetGUID().GetCounter();
    bool isAlive = bot->IsAlive();
    bool wasAlive = s_wasAlive.count(guid) ? s_wasAlive[guid] : true;

    s_wasAlive[guid] = isAlive;

    // Trigger when dying
    if (wasAlive && !isAlive)
    {
        return true;  // Always chatter on death
    }

    return false;
}

// ============================================================================
// ResurrectChatterTrigger
// ============================================================================

std::unordered_map<uint64, bool> ResurrectChatterTrigger::s_wasDead;

bool ResurrectChatterTrigger::IsActive()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    if (!bot->GetMap()->IsDungeon())
        return false;

    uint64 guid = bot->GetGUID().GetCounter();
    bool isAlive = bot->IsAlive();
    bool wasDead = s_wasDead.count(guid) ? s_wasDead[guid] : false;

    s_wasDead[guid] = !isAlive;

    // Trigger when resurrected
    if (wasDead && isAlive)
    {
        return true;  // Always chatter on res
    }

    return false;
}
