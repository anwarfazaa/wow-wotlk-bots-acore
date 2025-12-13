/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "TankLeadActions.h"
#include "DungeonNavigator.h"
#include "Group.h"
#include "GroupAICoordinator.h"
#include "IntentBroadcaster.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "Timer.h"

#include <cmath>

// ============================================================================
// MoveToNextWaypointAction
// ============================================================================

bool MoveToNextWaypointAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    // Use bot's GUID as fallback group ID for solo players to avoid conflicts
    Group* group = bot->GetGroup();
    uint32 groupId = group ? group->GetGUID().GetCounter() : bot->GetGUID().GetCounter();

    GroupProgress* progress = sDungeonNavigator->GetGroupProgress(groupId, mapId);
    if (!progress)
        return false;

    // Get path to next waypoint
    Position currentPos;
    currentPos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    NavigationResult nav = sDungeonNavigator->GetPathToNextWaypoint(
        mapId, currentPos, progress->currentWaypointIndex);

    if (!nav.found || nav.path.empty())
        return false;

    // Move to next position in path
    const Position& target = nav.path.back();

    bool result = MoveTo(mapId, target.GetPositionX(), target.GetPositionY(),
                         target.GetPositionZ(), false, false, false, true,
                         MovementPriority::MOVEMENT_NORMAL, true);

    if (result)
    {
        // Broadcast movement intent
        sIntentBroadcaster->BroadcastMovingToPosition(bot->GetGUID(), target, 5000);
    }

    return result;
}

bool MoveToNextWaypointAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Don't move in combat
    if (bot->IsInCombat())
        return false;

    // Must be tank
    if (!botAI->ContainsStrategy(STRATEGY_TYPE_TANK))
        return false;

    // Must have dungeon path
    uint32 mapId = bot->GetMapId();
    return sDungeonNavigator->HasDungeonPath(mapId);
}

bool MoveToNextWaypointAction::UpdateProgress()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    // Check if we've reached the next waypoint
    uint16 nextIndex = progress->currentWaypointIndex + 1;
    if (sDungeonNavigator->IsAtWaypoint(mapId, pos, nextIndex))
    {
        // Use consistent groupId (bot GUID for solo players)
        Group* grp = bot->GetGroup();
        uint32 grpId = grp ? grp->GetGUID().GetCounter() : bot->GetGUID().GetCounter();
        sDungeonNavigator->SetGroupWaypoint(grpId, nextIndex);
        return true;
    }

    return false;
}

// ============================================================================
// MoveToWaypointAction
// ============================================================================

bool MoveToWaypointAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    // Get target waypoint index from event or current progress
    uint16 targetIndex = 0;
    std::string param = event.getParam();
    if (!param.empty())
    {
        targetIndex = static_cast<uint16>(std::stoi(param));
    }
    else
    {
        GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
        if (progress)
            targetIndex = progress->currentWaypointIndex;
    }

    const DungeonWaypoint* wp = path->GetWaypoint(targetIndex);
    if (!wp)
        return false;

    return MoveTo(mapId, wp->position.GetPositionX(), wp->position.GetPositionY(),
                  wp->position.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_NORMAL, true);
}

bool MoveToWaypointAction::isUseful()
{
    return MoveToNextWaypointAction(botAI).isUseful();
}

// ============================================================================
// WaitForGroupAction
// ============================================================================

bool WaitForGroupAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    float maxDist = GetFurthestMemberDistance();

    // If everyone is close, we're done waiting
    if (maxDist < 30.0f)
    {
        botAI->TellMaster("Group assembled, ready to proceed.");
        return true;
    }

    // Still waiting - announce less frequently (every ~10 seconds on average)
    static uint32 lastAnnounceTime = 0;
    uint32 now = getMSTime();
    if (getMSTimeDiff(lastAnnounceTime, now) > 10000)
    {
        botAI->TellMaster("Waiting for group to catch up...");
        lastAnnounceTime = now;
    }

    // Set a short delay before next action
    botAI->SetNextCheckDelay(3000);

    return true;
}

bool WaitForGroupAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be tank
    if (!botAI->ContainsStrategy(STRATEGY_TYPE_TANK))
        return false;

    // Must have group
    if (!bot->GetGroup())
        return false;

    return GetFurthestMemberDistance() > 30.0f;
}

float WaitForGroupAction::GetFurthestMemberDistance()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return 0.0f;

    Group* group = bot->GetGroup();
    if (!group)
        return 0.0f;

    float maxDist = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        float dist = bot->GetDistance(member);
        if (dist > maxDist)
            maxDist = dist;
    }

    return maxDist;
}

// ============================================================================
// AnnouncePullAction
// ============================================================================

bool AnnouncePullAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Get current waypoint info
    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    const DungeonWaypoint* wp = sDungeonNavigator->GetWaypointAtPosition(mapId, pos, 15.0f);
    if (wp)
    {
        if (wp->IsBoss())
        {
            botAI->TellMaster("Pulling boss: " + wp->description);
        }
        else if (wp->IsTrash())
        {
            botAI->TellMaster("Pulling trash pack");
        }
    }
    else
    {
        botAI->TellMaster("Pulling!");
    }

    // Broadcast pull intent
    Group* group = bot->GetGroup();
    if (group)
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (target)
        {
            sIntentBroadcaster->BroadcastPulling(
                bot->GetGUID(), target->GetGUID(),
                group->GetGUID().GetCounter(), 5000);
        }
    }

    return true;
}

bool AnnouncePullAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    return botAI->ContainsStrategy(STRATEGY_TYPE_TANK) && !bot->IsInCombat();
}

// ============================================================================
// AnnounceMovementAction
// ============================================================================

bool AnnounceMovementAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    uint16 nextIndex = progress->currentWaypointIndex + 1;
    const DungeonWaypoint* nextWp = path->GetWaypoint(nextIndex);
    if (!nextWp)
    {
        botAI->TellMaster("Moving forward.");
        return true;
    }

    if (nextWp->IsBoss())
    {
        botAI->TellMaster("Moving to boss: " + nextWp->description);
    }
    else if (nextWp->IsSafeSpot())
    {
        botAI->TellMaster("Moving to safe spot.");
    }
    else if (nextWp->IsTrash())
    {
        botAI->TellMaster("Moving to next pack.");
    }
    else
    {
        botAI->TellMaster("Moving forward.");
    }

    return true;
}

bool AnnounceMovementAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return bot && botAI->ContainsStrategy(STRATEGY_TYPE_TANK) && !bot->IsInCombat();
}

// ============================================================================
// AnnounceBossAction
// ============================================================================

bool AnnounceBossAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    const DungeonWaypoint* nextBoss = path->GetNextBoss(progress->currentWaypointIndex);
    if (!nextBoss)
    {
        botAI->TellMaster("No more bosses ahead.");
        return true;
    }

    std::string msg = "Next boss: " + nextBoss->description;
    botAI->TellMaster(msg);

    return true;
}

bool AnnounceBossAction::isUseful()
{
    return botAI->ContainsStrategy(STRATEGY_TYPE_TANK);
}

// ============================================================================
// WaitForManaBreakAction
// ============================================================================

bool WaitForManaBreakAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    if (!urand(0, 3))
    {
        botAI->TellMaster("Waiting for mana...");
    }

    botAI->SetNextCheckDelay(3000);
    return true;
}

bool WaitForManaBreakAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    return sGroupAICoordinator->GroupNeedsManaBreak(group->GetGUID().GetCounter(), 30);
}

// ============================================================================
// UpdateDungeonProgressAction
// ============================================================================

bool UpdateDungeonProgressAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    // Find nearest waypoint
    auto nearestIndex = sDungeonNavigator->FindNearestWaypointIndex(mapId, pos, 20.0f);
    if (!nearestIndex.has_value())
        return false;

    Group* group = bot->GetGroup();
    uint32 groupId = group ? group->GetGUID().GetCounter() : 0;

    GroupProgress* progress = sDungeonNavigator->GetGroupProgress(groupId, mapId);
    if (!progress)
        return false;

    // Update to nearest waypoint if it's ahead of current
    if (*nearestIndex > progress->currentWaypointIndex)
    {
        sDungeonNavigator->SetGroupWaypoint(groupId, *nearestIndex);
    }

    return true;
}

bool UpdateDungeonProgressAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    return sDungeonNavigator->HasDungeonPath(bot->GetMapId());
}

// ============================================================================
// InitializeDungeonProgressAction
// ============================================================================

bool InitializeDungeonProgressAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    if (!sDungeonNavigator->HasDungeonPath(mapId))
        return false;

    Group* group = bot->GetGroup();
    uint32 groupId = group ? group->GetGUID().GetCounter() : 0;

    // Initialize progress at waypoint 0
    GroupProgress* progress = sDungeonNavigator->GetGroupProgress(groupId, mapId);
    if (!progress)
        return false;

    // Find nearest waypoint to current position
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    auto nearestIndex = sDungeonNavigator->FindNearestWaypointIndex(mapId, pos, 100.0f);
    if (nearestIndex.has_value())
    {
        sDungeonNavigator->SetGroupWaypoint(groupId, *nearestIndex);
    }

    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (path)
    {
        botAI->TellMaster("Entering " + path->dungeonName + ". Tank lead enabled.");
    }

    return true;
}

bool InitializeDungeonProgressAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Only useful if in dungeon with path data and no progress yet
    uint32 mapId = bot->GetMapId();
    if (!sDungeonNavigator->HasDungeonPath(mapId))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    return sDungeonNavigator->GetGroupProgress(group->GetGUID().GetCounter()) == nullptr;
}

// ============================================================================
// MarkTrashClearedAction
// ============================================================================

bool MarkTrashClearedAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    const DungeonWaypoint* wp = sDungeonNavigator->GetWaypointAtPosition(mapId, pos, 30.0f);
    if (!wp || !wp->IsTrash())
        return false;

    Group* group = bot->GetGroup();
    uint32 groupId = group ? group->GetGUID().GetCounter() : 0;

    sDungeonNavigator->RecordTrashClear(groupId, wp->trashPackId);

    return true;
}

bool MarkTrashClearedAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Only useful when out of combat after a fight
    return !bot->IsInCombat() && sDungeonNavigator->HasDungeonPath(bot->GetMapId());
}

// ============================================================================
// MarkBossKilledAction
// ============================================================================

bool MarkBossKilledAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    const DungeonWaypoint* wp = sDungeonNavigator->GetWaypointAtPosition(mapId, pos, 50.0f);
    if (!wp || !wp->IsBoss())
        return false;

    Group* group = bot->GetGroup();
    uint32 groupId = group ? group->GetGUID().GetCounter() : 0;

    sDungeonNavigator->RecordBossKill(groupId, wp->bossEntry);

    botAI->TellMaster(wp->description + " defeated!");

    return true;
}

bool MarkBossKilledAction::isUseful()
{
    return MarkTrashClearedAction(botAI).isUseful();
}

// ============================================================================
// MoveToSafeSpotAction
// ============================================================================

bool MoveToSafeSpotAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    NavigationResult nav = sDungeonNavigator->GetPathToNearestSafeSpot(mapId, pos);
    if (!nav.found || nav.path.empty())
        return false;

    const Position& target = nav.path.back();
    return MoveTo(mapId, target.GetPositionX(), target.GetPositionY(),
                  target.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_NORMAL, true);
}

bool MoveToSafeSpotAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    return !bot->IsInCombat() && sDungeonNavigator->HasDungeonPath(bot->GetMapId());
}

// ============================================================================
// PullTrashAction
// ============================================================================

bool PullTrashAction::Execute(Event event)
{
    // This delegates to standard attack logic after announcing
    AnnouncePullAction(botAI).Execute(event);

    // The actual pull is handled by combat strategies
    // This action just prepares and announces

    return true;
}

bool PullTrashAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    return botAI->ContainsStrategy(STRATEGY_TYPE_TANK) && !bot->IsInCombat();
}

// ============================================================================
// StartBossEncounterAction
// ============================================================================

bool StartBossEncounterAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    const DungeonWaypoint* wp = path->GetWaypoint(progress->currentWaypointIndex);
    if (!wp || !wp->IsBoss())
        return false;

    // Announce boss
    botAI->TellMaster("Engaging " + wp->description + "!");

    // Broadcast pull intent
    Group* group = bot->GetGroup();
    if (group)
    {
        sIntentBroadcaster->BroadcastPulling(
            bot->GetGUID(), ObjectGuid::Empty,
            group->GetGUID().GetCounter(), 10000);
    }

    return true;
}

bool StartBossEncounterAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    if (!botAI->ContainsStrategy(STRATEGY_TYPE_TANK) || bot->IsInCombat())
        return false;

    uint32 mapId = bot->GetMapId();
    GroupProgress* progress = sDungeonNavigator->GetPlayerGroupProgress(bot);
    if (!progress)
        return false;

    const DungeonPath* path = sDungeonNavigator->GetDungeonPath(mapId);
    if (!path)
        return false;

    const DungeonWaypoint* wp = path->GetWaypoint(progress->currentWaypointIndex);
    return wp && wp->IsBoss();
}

// ============================================================================
// CheckGroupReadyAction
// ============================================================================

bool CheckGroupReadyAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    uint32 groupId = group->GetGUID().GetCounter();
    GroupCoordinatorData* data = sGroupAICoordinator->GetGroupData(groupId);
    if (!data)
        return false;

    // Update bot's ready state
    GroupMemberInfo info;
    info.guid = bot->GetGUID();
    info.healthPct = static_cast<uint8>(bot->GetHealthPct());
    info.manaPct = static_cast<uint8>(bot->GetPowerPct(POWER_MANA));
    info.isAlive = bot->IsAlive();
    info.position.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    data->UpdateMemberInfo(bot->GetGUID(), info);

    // Set ready state
    if (!bot->IsAlive())
        data->SetMemberReadyState(bot->GetGUID(), GroupReadyState::NOT_READY);
    else if (bot->HasUnitState(UNIT_STATE_CASTING))
        data->SetMemberReadyState(bot->GetGUID(), GroupReadyState::BUFFING);
    else if (bot->GetPowerPct(POWER_MANA) < 80 && botAI->ContainsStrategy(STRATEGY_TYPE_HEAL))
        data->SetMemberReadyState(bot->GetGUID(), GroupReadyState::DRINKING);
    else
        data->SetMemberReadyState(bot->GetGUID(), GroupReadyState::READY);

    return true;
}

bool CheckGroupReadyAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return bot && bot->GetGroup() != nullptr;
}

// ============================================================================
// SyncGroupProgressAction
// ============================================================================

bool SyncGroupProgressAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 mapId = bot->GetMapId();
    uint32 groupId = group->GetGUID().GetCounter();

    // Find the furthest progress among group members on the same map
    uint16 maxProgress = 0;
    Position leadPos;
    bool foundMemberOnMap = false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != mapId)
            continue;

        foundMemberOnMap = true;

        Position memberPos;
        memberPos.Relocate(member->GetPositionX(), member->GetPositionY(), member->GetPositionZ());

        auto index = sDungeonNavigator->FindNearestWaypointIndex(mapId, memberPos, 50.0f);
        if (index.has_value() && *index > maxProgress)
        {
            maxProgress = *index;
            leadPos = memberPos;
        }
    }

    // Only update if we found valid members on this map
    if (!foundMemberOnMap)
        return false;

    // Update group progress to match furthest member
    GroupProgress* progress = sDungeonNavigator->GetGroupProgress(groupId, mapId);
    if (progress && maxProgress > progress->currentWaypointIndex)
    {
        sDungeonNavigator->SetGroupWaypoint(groupId, maxProgress);
    }

    return true;
}

bool SyncGroupProgressAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    return bot->GetGroup() && sDungeonNavigator->HasDungeonPath(bot->GetMapId());
}

// ============================================================================
// RequestHumanLeadAction
// ============================================================================

std::unordered_map<uint64, uint32> RequestHumanLeadAction::s_lastAnnounceTime;

bool RequestHumanLeadAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Player* humanLeader = FindHumanLeader();
    if (!humanLeader)
    {
        // No human in group - bot will just follow master or stay
        return false;
    }

    // Only announce periodically to avoid spam
    if (!HasAnnouncedRecently())
    {
        std::string dungeonName = bot->GetMap()->GetMapName();
        botAI->TellMaster("I don't know the route for " + dungeonName + ". Please lead the way!");
        s_lastAnnounceTime[bot->GetGUID().GetCounter()] = getMSTime();
    }

    return true;
}

bool RequestHumanLeadAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon without waypoints
    if (!bot->GetMap()->IsDungeon())
        return false;

    if (sDungeonNavigator->HasDungeonPath(bot->GetMapId()))
        return false;

    // Must have a human player in group
    return FindHumanLeader() != nullptr;
}

Player* RequestHumanLeadAction::FindHumanLeader()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
    {
        // Solo bot - check if master is human
        Player* master = botAI->GetMaster();
        if (master && !master->GetSession()->IsBot())
            return master;
        return nullptr;
    }

    // Find a human player in the group
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && !member->GetSession()->IsBot() && member->IsAlive())
            return member;
    }

    return nullptr;
}

bool RequestHumanLeadAction::HasAnnouncedRecently()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return true;

    uint64 guid = bot->GetGUID().GetCounter();
    auto it = s_lastAnnounceTime.find(guid);
    if (it == s_lastAnnounceTime.end())
        return false;

    return getMSTimeDiff(it->second, getMSTime()) < ANNOUNCE_COOLDOWN_MS;
}

// ============================================================================
// FollowHumanLeaderAction
// ============================================================================

bool FollowHumanLeaderAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Player* humanLeader = FindHumanLeader();
    if (!humanLeader)
        return false;

    // Don't follow if already close
    float dist = bot->GetDistance(humanLeader);
    if (dist < 5.0f)
        return false;

    // Follow the human leader, staying behind them
    float angle = humanLeader->GetOrientation() + M_PI;  // Behind the leader
    float followDist = 3.0f;

    float x = humanLeader->GetPositionX() + cos(angle) * followDist;
    float y = humanLeader->GetPositionY() + sin(angle) * followDist;
    float z = humanLeader->GetPositionZ();

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, true,
                  MovementPriority::MOVEMENT_NORMAL, true);
}

bool FollowHumanLeaderAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon without waypoints
    if (!bot->GetMap()->IsDungeon())
        return false;

    if (sDungeonNavigator->HasDungeonPath(bot->GetMapId()))
        return false;

    // Must not be in combat (let combat strategies handle that)
    if (bot->IsInCombat())
        return false;

    // Must have a human to follow
    Player* humanLeader = FindHumanLeader();
    if (!humanLeader)
        return false;

    // Only follow if not already close
    return bot->GetDistance(humanLeader) > 8.0f;
}

Player* FollowHumanLeaderAction::FindHumanLeader()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
    {
        // Solo bot - check if master is human
        Player* master = botAI->GetMaster();
        if (master && !master->GetSession()->IsBot())
            return master;
        return nullptr;
    }

    // Prefer group leader if human
    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
    if (leader && !leader->GetSession()->IsBot() && leader->IsAlive() && leader->GetMapId() == bot->GetMapId())
        return leader;

    // Otherwise find any human in the group
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && !member->GetSession()->IsBot() && member->IsAlive() && member->GetMapId() == bot->GetMapId())
            return member;
    }

    return nullptr;
}

// ============================================================================
// DungeonChatterAction
// ============================================================================

bool DungeonChatterAction::Execute(Event event)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    std::string chatter = sDungeonChatter->GetChatter(m_category, bot);
    if (chatter.empty())
        return false;

    // Say in party/raid chat or say
    Group* group = bot->GetGroup();
    if (group)
    {
        // Use party chat
        botAI->SayToParty(chatter);
    }
    else
    {
        // Use say
        botAI->Say(chatter);
    }

    // Record the chatter to prevent spam
    sDungeonChatter->RecordChatter(bot);

    return true;
}

bool DungeonChatterAction::isUseful()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Must be in a dungeon
    return bot->GetMap()->IsDungeon();
}
