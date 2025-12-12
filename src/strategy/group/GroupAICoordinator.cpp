/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "GroupAICoordinator.h"
#include "IntentBroadcaster.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

#include <algorithm>
#include <cmath>

// ============================================================================
// GroupCoordinatorData Implementation
// ============================================================================

GroupCoordinatorData::GroupCoordinatorData(uint32 groupId)
    : m_groupId(groupId)
{
}

// =========================================================================
// Member Management
// =========================================================================

void GroupCoordinatorData::AddMember(ObjectGuid guid, GroupRole role, bool isBot)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    GroupMemberInfo info;
    info.guid = guid;
    info.role = role;
    info.isBot = isBot;
    info.lastUpdateTime = getMSTime();

    m_members[guid.GetCounter()] = info;
}

void GroupCoordinatorData::RemoveMember(ObjectGuid guid)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    m_members.erase(guid.GetCounter());

    // Remove from any target assignments
    for (auto& [targetKey, assignment] : m_targetAssignments)
    {
        assignment.assignedDps.erase(
            std::remove(assignment.assignedDps.begin(), assignment.assignedDps.end(), guid),
            assignment.assignedDps.end());

        if (assignment.assignedTank == guid)
            assignment.assignedTank = ObjectGuid::Empty;
        if (assignment.assignedCC == guid)
        {
            assignment.assignedCC = ObjectGuid::Empty;
            assignment.ccSpellId = 0;
        }
    }
}

void GroupCoordinatorData::UpdateMemberInfo(ObjectGuid guid, const GroupMemberInfo& info)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_members.find(guid.GetCounter());
    if (it != m_members.end())
    {
        it->second.healthPct = info.healthPct;
        it->second.manaPct = info.manaPct;
        it->second.position = info.position;
        it->second.isAlive = info.isAlive;
        it->second.isInRange = info.isInRange;
        it->second.lastUpdateTime = getMSTime();
    }
}

GroupMemberInfo* GroupCoordinatorData::GetMemberInfo(ObjectGuid guid)
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_members.find(guid.GetCounter());
    if (it != m_members.end())
        return &it->second;
    return nullptr;
}

const GroupMemberInfo* GroupCoordinatorData::GetMemberInfo(ObjectGuid guid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_members.find(guid.GetCounter());
    if (it != m_members.end())
        return &it->second;
    return nullptr;
}

std::vector<ObjectGuid> GroupCoordinatorData::GetMembersByRole(GroupRole role) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<ObjectGuid> result;
    for (const auto& [key, info] : m_members)
    {
        if (info.role == role && info.isAlive)
            result.push_back(info.guid);
    }
    return result;
}

std::vector<ObjectGuid> GroupCoordinatorData::GetAllMembers() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<ObjectGuid> result;
    result.reserve(m_members.size());
    for (const auto& [key, info] : m_members)
    {
        result.push_back(info.guid);
    }
    return result;
}

bool GroupCoordinatorData::HasMember(ObjectGuid guid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_members.find(guid.GetCounter()) != m_members.end();
}

size_t GroupCoordinatorData::GetMemberCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_members.size();
}

// =========================================================================
// Target Coordination
// =========================================================================

void GroupCoordinatorData::SetFocusTarget(ObjectGuid targetGuid)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_focusTarget = targetGuid;

    if (!targetGuid.IsEmpty())
    {
        auto& assignment = m_targetAssignments[targetGuid.GetCounter()];
        assignment.targetGuid = targetGuid;
        assignment.isFocusTarget = true;
        assignment.priority = 255;  // Highest priority
    }
}

ObjectGuid GroupCoordinatorData::GetFocusTarget() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_focusTarget;
}

bool GroupCoordinatorData::AssignDpsToTarget(ObjectGuid dpsGuid, ObjectGuid targetGuid)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    // Create or get target assignment
    auto& assignment = m_targetAssignments[targetGuid.GetCounter()];
    assignment.targetGuid = targetGuid;

    // Check if already assigned
    auto it = std::find(assignment.assignedDps.begin(), assignment.assignedDps.end(), dpsGuid);
    if (it != assignment.assignedDps.end())
        return true;  // Already assigned

    // Remove from any previous assignment
    for (auto& [key, otherAssignment] : m_targetAssignments)
    {
        if (key != targetGuid.GetCounter())
        {
            otherAssignment.assignedDps.erase(
                std::remove(otherAssignment.assignedDps.begin(), otherAssignment.assignedDps.end(), dpsGuid),
                otherAssignment.assignedDps.end());
        }
    }

    assignment.assignedDps.push_back(dpsGuid);
    return true;
}

ObjectGuid GroupCoordinatorData::GetRecommendedTarget(ObjectGuid dpsGuid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    // If there's a focus target, always recommend that
    if (!m_focusTarget.IsEmpty())
        return m_focusTarget;

    // Find target with fewest DPS assigned (excluding CC'd targets)
    ObjectGuid bestTarget;
    size_t minDps = SIZE_MAX;

    for (const auto& [key, assignment] : m_targetAssignments)
    {
        // Skip CC'd targets
        if (assignment.HasAssignedCC())
            continue;

        // Check if this DPS is already assigned here
        auto it = std::find(assignment.assignedDps.begin(), assignment.assignedDps.end(), dpsGuid);
        if (it != assignment.assignedDps.end())
            return assignment.targetGuid;  // Stay on current target

        if (assignment.assignedDps.size() < minDps)
        {
            minDps = assignment.assignedDps.size();
            bestTarget = assignment.targetGuid;
        }
    }

    return bestTarget;
}

void GroupCoordinatorData::SetTankTarget(ObjectGuid tankGuid, ObjectGuid targetGuid)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    // Clear previous tank assignment
    for (auto& [key, assignment] : m_targetAssignments)
    {
        if (assignment.assignedTank == tankGuid)
            assignment.assignedTank = ObjectGuid::Empty;
    }

    if (!targetGuid.IsEmpty())
    {
        auto& assignment = m_targetAssignments[targetGuid.GetCounter()];
        assignment.targetGuid = targetGuid;
        assignment.assignedTank = tankGuid;
    }
}

ObjectGuid GroupCoordinatorData::GetTankTarget(ObjectGuid tankGuid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    for (const auto& [key, assignment] : m_targetAssignments)
    {
        if (assignment.assignedTank == tankGuid)
            return assignment.targetGuid;
    }

    return ObjectGuid::Empty;
}

bool GroupCoordinatorData::AssignCrowdControl(ObjectGuid ccerGuid, ObjectGuid targetGuid, uint32 spellId)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    auto& assignment = m_targetAssignments[targetGuid.GetCounter()];

    // If already CC'd by someone else, fail
    if (assignment.HasAssignedCC() && assignment.assignedCC != ccerGuid)
        return false;

    assignment.targetGuid = targetGuid;
    assignment.assignedCC = ccerGuid;
    assignment.ccSpellId = spellId;

    // Remove any DPS from this target
    assignment.assignedDps.clear();

    return true;
}

bool GroupCoordinatorData::IsCrowdControlled(ObjectGuid targetGuid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_targetAssignments.find(targetGuid.GetCounter());
    if (it == m_targetAssignments.end())
        return false;

    return it->second.HasAssignedCC();
}

ObjectGuid GroupCoordinatorData::GetCrowdController(ObjectGuid targetGuid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_targetAssignments.find(targetGuid.GetCounter());
    if (it == m_targetAssignments.end())
        return ObjectGuid::Empty;

    return it->second.assignedCC;
}

std::vector<TargetAssignment> GroupCoordinatorData::GetTargetAssignments() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<TargetAssignment> result;
    result.reserve(m_targetAssignments.size());

    for (const auto& [key, assignment] : m_targetAssignments)
    {
        result.push_back(assignment);
    }

    // Sort by priority (highest first)
    std::sort(result.begin(), result.end(),
        [](const TargetAssignment& a, const TargetAssignment& b) {
            return a.priority > b.priority;
        });

    return result;
}

void GroupCoordinatorData::ClearTargetAssignments()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_targetAssignments.clear();
    m_focusTarget = ObjectGuid::Empty;
}

// =========================================================================
// Ready Check System
// =========================================================================

void GroupCoordinatorData::SetMemberReadyState(ObjectGuid guid, GroupReadyState state)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_members.find(guid.GetCounter());
    if (it != m_members.end())
    {
        it->second.readyState = state;
    }
}

GroupReadyState GroupCoordinatorData::GetMemberReadyState(ObjectGuid guid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_members.find(guid.GetCounter());
    if (it != m_members.end())
        return it->second.readyState;

    return GroupReadyState::NOT_READY;
}

bool GroupCoordinatorData::IsGroupReady() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    for (const auto& [key, info] : m_members)
    {
        if (info.isAlive && info.readyState != GroupReadyState::READY)
            return false;
    }

    return !m_members.empty();
}

float GroupCoordinatorData::GetGroupReadyPercent() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    if (m_members.empty())
        return 0.0f;

    size_t readyCount = 0;
    size_t aliveCount = 0;

    for (const auto& [key, info] : m_members)
    {
        if (info.isAlive)
        {
            ++aliveCount;
            if (info.readyState == GroupReadyState::READY)
                ++readyCount;
        }
    }

    if (aliveCount == 0)
        return 0.0f;

    return (static_cast<float>(readyCount) / static_cast<float>(aliveCount)) * 100.0f;
}

std::vector<ObjectGuid> GroupCoordinatorData::GetNotReadyMembers() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<ObjectGuid> result;
    for (const auto& [key, info] : m_members)
    {
        if (info.isAlive && info.readyState != GroupReadyState::READY)
            result.push_back(info.guid);
    }

    return result;
}

// =========================================================================
// Resource Tracking
// =========================================================================

float GroupCoordinatorData::GetAverageHealthPct() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    if (m_members.empty())
        return 100.0f;

    uint32 total = 0;
    uint32 count = 0;

    for (const auto& [key, info] : m_members)
    {
        if (info.isAlive)
        {
            total += info.healthPct;
            ++count;
        }
    }

    if (count == 0)
        return 0.0f;

    return static_cast<float>(total) / static_cast<float>(count);
}

float GroupCoordinatorData::GetHealerManaPct() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    uint32 total = 0;
    uint32 count = 0;

    for (const auto& [key, info] : m_members)
    {
        if (info.isAlive && info.role == GroupRole::HEALER)
        {
            total += info.manaPct;
            ++count;
        }
    }

    if (count == 0)
        return 100.0f;

    return static_cast<float>(total) / static_cast<float>(count);
}

bool GroupCoordinatorData::NeedsManaBreak(uint8 threshold) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    for (const auto& [key, info] : m_members)
    {
        if (info.isAlive && info.role == GroupRole::HEALER && info.manaPct < threshold)
            return true;
    }

    return false;
}

std::vector<ObjectGuid> GroupCoordinatorData::GetLowHealthMembers(uint8 threshold) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<ObjectGuid> result;
    for (const auto& [key, info] : m_members)
    {
        if (info.isAlive && info.healthPct < threshold)
            result.push_back(info.guid);
    }

    // Sort by health (lowest first)
    std::sort(result.begin(), result.end(),
        [this](ObjectGuid a, ObjectGuid b) {
            auto itA = m_members.find(a.GetCounter());
            auto itB = m_members.find(b.GetCounter());
            if (itA == m_members.end()) return false;
            if (itB == m_members.end()) return true;
            return itA->second.healthPct < itB->second.healthPct;
        });

    return result;
}

// =========================================================================
// Formation System
// =========================================================================

std::vector<FormationPosition> GroupCoordinatorData::CalculateFormation(Unit* target) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<FormationPosition> positions;
    if (!target)
        return positions;

    Position targetPos;
    targetPos.Relocate(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), target->GetOrientation());

    // Separate members by role
    std::vector<ObjectGuid> tanks, melees, ranged, healers;

    for (const auto& [key, info] : m_members)
    {
        if (!info.isAlive)
            continue;

        switch (info.role)
        {
            case GroupRole::TANK:
                tanks.push_back(info.guid);
                break;
            case GroupRole::MELEE_DPS:
                melees.push_back(info.guid);
                break;
            case GroupRole::HEALER:
                healers.push_back(info.guid);
                break;
            case GroupRole::RANGED_DPS:
                ranged.push_back(info.guid);
                break;
            default:
                break;
        }
    }

    // Calculate tank position (in front of target)
    float baseAngle = target->GetOrientation();
    for (size_t i = 0; i < tanks.size(); ++i)
    {
        FormationPosition pos;
        pos.memberGuid = tanks[i];
        pos.distanceFromTarget = MELEE_FORMATION_RADIUS;
        pos.angleFromTarget = baseAngle;  // Face the target
        pos.isMelee = true;

        float x = targetPos.GetPositionX() + pos.distanceFromTarget * std::cos(pos.angleFromTarget);
        float y = targetPos.GetPositionY() + pos.distanceFromTarget * std::sin(pos.angleFromTarget);
        pos.targetPos.Relocate(x, y, targetPos.GetPositionZ());

        positions.push_back(pos);
    }

    // Calculate melee DPS positions (semicircle behind target)
    float meleeArcStart = baseAngle + static_cast<float>(M_PI) - static_cast<float>(M_PI / 3);  // Behind, offset
    float meleeArcStep = melees.empty() ? 0 : (2.0f * static_cast<float>(M_PI / 3)) / static_cast<float>(melees.size());

    for (size_t i = 0; i < melees.size(); ++i)
    {
        FormationPosition pos;
        pos.memberGuid = melees[i];
        pos.distanceFromTarget = MELEE_FORMATION_RADIUS;
        pos.angleFromTarget = meleeArcStart + meleeArcStep * static_cast<float>(i);
        pos.isMelee = true;

        float x = targetPos.GetPositionX() + pos.distanceFromTarget * std::cos(pos.angleFromTarget);
        float y = targetPos.GetPositionY() + pos.distanceFromTarget * std::sin(pos.angleFromTarget);
        pos.targetPos.Relocate(x, y, targetPos.GetPositionZ());

        positions.push_back(pos);
    }

    // Calculate ranged DPS positions (wider arc at range)
    float rangedArcStart = baseAngle + static_cast<float>(M_PI) - static_cast<float>(M_PI / 2);
    float rangedArcStep = ranged.empty() ? 0 : static_cast<float>(M_PI) / static_cast<float>(ranged.size() + 1);

    for (size_t i = 0; i < ranged.size(); ++i)
    {
        FormationPosition pos;
        pos.memberGuid = ranged[i];
        pos.distanceFromTarget = RANGED_FORMATION_RADIUS;
        pos.angleFromTarget = rangedArcStart + rangedArcStep * static_cast<float>(i + 1);
        pos.isMelee = false;

        float x = targetPos.GetPositionX() + pos.distanceFromTarget * std::cos(pos.angleFromTarget);
        float y = targetPos.GetPositionY() + pos.distanceFromTarget * std::sin(pos.angleFromTarget);
        pos.targetPos.Relocate(x, y, targetPos.GetPositionZ());

        positions.push_back(pos);
    }

    // Calculate healer positions (behind ranged, spread)
    float healerArcStart = baseAngle + static_cast<float>(M_PI) - static_cast<float>(M_PI / 4);
    float healerArcStep = healers.empty() ? 0 : static_cast<float>(M_PI / 2) / static_cast<float>(healers.size() + 1);

    for (size_t i = 0; i < healers.size(); ++i)
    {
        FormationPosition pos;
        pos.memberGuid = healers[i];
        pos.distanceFromTarget = RANGED_FORMATION_RADIUS + 5.0f;  // Behind ranged
        pos.angleFromTarget = healerArcStart + healerArcStep * static_cast<float>(i + 1);
        pos.isMelee = false;

        float x = targetPos.GetPositionX() + pos.distanceFromTarget * std::cos(pos.angleFromTarget);
        float y = targetPos.GetPositionY() + pos.distanceFromTarget * std::sin(pos.angleFromTarget);
        pos.targetPos.Relocate(x, y, targetPos.GetPositionZ());

        positions.push_back(pos);
    }

    return positions;
}

Position GroupCoordinatorData::GetFormationPosition(ObjectGuid memberGuid, Unit* target) const
{
    auto positions = CalculateFormation(target);

    for (const auto& pos : positions)
    {
        if (pos.memberGuid == memberGuid)
            return pos.targetPos;
    }

    return Position();
}

// =========================================================================
// Pull Coordination
// =========================================================================

void GroupCoordinatorData::SetPullTarget(ObjectGuid targetGuid)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_pullTarget = targetGuid;
}

ObjectGuid GroupCoordinatorData::GetPullTarget() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_pullTarget;
}

bool GroupCoordinatorData::IsPullInProgress() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_pullStartTime > 0 && !m_pullingTank.IsEmpty();
}

void GroupCoordinatorData::StartPull(ObjectGuid tankGuid, ObjectGuid targetGuid)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_pullingTank = tankGuid;
    m_pullTarget = targetGuid;
    m_pullStartTime = getMSTime();
}

void GroupCoordinatorData::EndPull()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_pullingTank = ObjectGuid::Empty;
    m_pullTarget = ObjectGuid::Empty;
    m_pullStartTime = 0;
}

uint32 GroupCoordinatorData::GetPullDuration() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    if (m_pullStartTime == 0)
        return 0;

    return getMSTimeDiff(m_pullStartTime, getMSTime());
}

// =========================================================================
// Movement Coordination
// =========================================================================

void GroupCoordinatorData::SetGroupDestination(const Position& pos)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_groupDestination = pos;
    m_hasDestination = true;
}

Position GroupCoordinatorData::GetGroupDestination() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_groupDestination;
}

bool GroupCoordinatorData::HasGroupDestination() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_hasDestination;
}

void GroupCoordinatorData::ClearGroupDestination()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_hasDestination = false;
}

bool GroupCoordinatorData::IsGroupAtDestination(float tolerance) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    if (!m_hasDestination)
        return true;

    for (const auto& [key, info] : m_members)
    {
        if (!info.isAlive)
            continue;

        float dx = info.position.GetPositionX() - m_groupDestination.GetPositionX();
        float dy = info.position.GetPositionY() - m_groupDestination.GetPositionY();
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist > tolerance)
            return false;
    }

    return true;
}

// =========================================================================
// Combat State
// =========================================================================

void GroupCoordinatorData::SetInCombat(bool inCombat)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    if (inCombat && !m_inCombat)
    {
        m_combatStartTime = getMSTime();
    }
    else if (!inCombat && m_inCombat)
    {
        ClearTargetAssignments();
    }

    m_inCombat = inCombat;
}

uint32 GroupCoordinatorData::GetCombatDuration() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    if (!m_inCombat || m_combatStartTime == 0)
        return 0;

    return getMSTimeDiff(m_combatStartTime, getMSTime());
}

// =========================================================================
// Maintenance
// =========================================================================

void GroupCoordinatorData::Update(uint32 diff)
{
    PruneStaleAssignments();
}

void GroupCoordinatorData::Clear()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    m_members.clear();
    m_targetAssignments.clear();
    m_focusTarget = ObjectGuid::Empty;
    m_pullTarget = ObjectGuid::Empty;
    m_pullingTank = ObjectGuid::Empty;
    m_pullStartTime = 0;
    m_hasDestination = false;
    m_inCombat = false;
    m_combatStartTime = 0;
}

void GroupCoordinatorData::PruneStaleAssignments()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    // Remove assignments for targets that have been inactive too long
    std::vector<uint64> keysToRemove;

    for (auto& [key, assignment] : m_targetAssignments)
    {
        // If assignment is empty (no tank, no DPS, no CC), mark for removal
        if (!assignment.HasAssignedTank() &&
            assignment.assignedDps.empty() &&
            !assignment.HasAssignedCC() &&
            !assignment.isFocusTarget)
        {
            keysToRemove.push_back(key);
        }
    }

    for (uint64 key : keysToRemove)
    {
        m_targetAssignments.erase(key);
    }
}

void GroupCoordinatorData::UpdateMemberDistances(const Position& referencePos)
{
    // This would update isInRange for each member based on distance to referencePos
    // Called when needing to determine coordination range
}

// ============================================================================
// GroupAICoordinator Implementation
// ============================================================================

GroupCoordinatorData* GroupAICoordinator::GetGroupData(uint32 groupId)
{
    if (groupId == 0)
        return nullptr;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        return it->second.get();

    // Create new coordinator data
    auto data = std::make_unique<GroupCoordinatorData>(groupId);
    GroupCoordinatorData* ptr = data.get();
    m_groups[groupId] = std::move(data);

    return ptr;
}

GroupCoordinatorData* GroupAICoordinator::GetGroupData(Group* group)
{
    if (!group)
        return nullptr;

    return GetGroupData(group->GetGUID().GetCounter());
}

GroupCoordinatorData* GroupAICoordinator::GetPlayerGroupData(Player* player)
{
    if (!player)
        return nullptr;

    Group* group = player->GetGroup();
    if (!group)
        return nullptr;

    return GetGroupData(group);
}

GroupCoordinatorData* GroupAICoordinator::GetPlayerGroupData(ObjectGuid playerGuid)
{
    Player* player = ObjectAccessor::FindPlayer(playerGuid);
    return GetPlayerGroupData(player);
}

void GroupAICoordinator::RemoveGroupData(uint32 groupId)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_groups.erase(groupId);
}

bool GroupAICoordinator::HasGroupData(uint32 groupId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_groups.find(groupId) != m_groups.end();
}

// =========================================================================
// Quick Access Methods
// =========================================================================

ObjectGuid GroupAICoordinator::GetRecommendedTarget(Player* bot)
{
    auto* data = GetPlayerGroupData(bot);
    if (!data)
        return ObjectGuid::Empty;

    return data->GetRecommendedTarget(bot->GetGUID());
}

bool GroupAICoordinator::IsInterruptClaimed(uint32 groupId, ObjectGuid targetGuid, uint32 spellId) const
{
    return sIntentBroadcaster->IsInterruptClaimed(targetGuid, spellId);
}

bool GroupAICoordinator::ClaimInterrupt(ObjectGuid botGuid, uint32 groupId, ObjectGuid targetGuid, uint32 spellId)
{
    return sIntentBroadcaster->BroadcastInterruptIntent(botGuid, targetGuid, spellId);
}

bool GroupAICoordinator::GroupNeedsManaBreak(uint32 groupId, uint8 threshold) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_groups.find(groupId);
    if (it == m_groups.end())
        return false;

    return it->second->NeedsManaBreak(threshold);
}

bool GroupAICoordinator::IsGroupReady(uint32 groupId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_groups.find(groupId);
    if (it == m_groups.end())
        return false;

    return it->second->IsGroupReady();
}

// =========================================================================
// Role Detection
// =========================================================================

GroupRole GroupAICoordinator::DetermineRole(Player* player)
{
    if (!player)
        return GroupRole::NONE;

    // Check if player has a bot AI
    PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
    if (ai)
        return DetermineRole(ai);

    // For real players, use spec detection
    uint8 cls = player->getClass();
    uint32 spec = player->GetPrimaryTalentTree(player->GetActiveSpec());

    switch (cls)
    {
        case CLASS_WARRIOR:
            // Protection = tank, others = melee DPS
            if (spec == 2)  // Protection
                return GroupRole::TANK;
            return GroupRole::MELEE_DPS;

        case CLASS_PALADIN:
            // Protection = tank, Holy = healer, Retribution = melee DPS
            if (spec == 0)  // Holy
                return GroupRole::HEALER;
            if (spec == 1)  // Protection
                return GroupRole::TANK;
            return GroupRole::MELEE_DPS;

        case CLASS_HUNTER:
            return GroupRole::RANGED_DPS;

        case CLASS_ROGUE:
            return GroupRole::MELEE_DPS;

        case CLASS_PRIEST:
            // Holy/Disc = healer, Shadow = ranged DPS
            if (spec == 2)  // Shadow
                return GroupRole::RANGED_DPS;
            return GroupRole::HEALER;

        case CLASS_SHAMAN:
            // Restoration = healer, Elemental = ranged, Enhancement = melee
            if (spec == 2)  // Restoration
                return GroupRole::HEALER;
            if (spec == 0)  // Elemental
                return GroupRole::RANGED_DPS;
            return GroupRole::MELEE_DPS;

        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return GroupRole::RANGED_DPS;

        case CLASS_DRUID:
            // Restoration = healer, Balance = ranged, Feral = melee/tank
            if (spec == 2)  // Restoration
                return GroupRole::HEALER;
            if (spec == 0)  // Balance
                return GroupRole::RANGED_DPS;
            // Feral could be tank or DPS - check for tank stance/form
            return GroupRole::MELEE_DPS;

        case CLASS_DEATH_KNIGHT:
            // Blood = tank, others = melee DPS
            if (spec == 0)  // Blood
                return GroupRole::TANK;
            return GroupRole::MELEE_DPS;

        default:
            return GroupRole::NONE;
    }
}

GroupRole GroupAICoordinator::DetermineRole(PlayerbotAI* ai)
{
    if (!ai)
        return GroupRole::NONE;

    // Check bot's assigned strategies for role
    if (ai->ContainsStrategy(STRATEGY_TYPE_TANK))
        return GroupRole::TANK;

    if (ai->ContainsStrategy(STRATEGY_TYPE_HEAL))
        return GroupRole::HEALER;

    if (ai->ContainsStrategy(STRATEGY_TYPE_RANGED))
        return GroupRole::RANGED_DPS;

    if (ai->ContainsStrategy(STRATEGY_TYPE_MELEE))
        return GroupRole::MELEE_DPS;

    // Fallback to class-based detection
    return DetermineRole(ai->GetBot());
}

// =========================================================================
// Maintenance
// =========================================================================

void GroupAICoordinator::Update(uint32 diff)
{
    uint32 now = getMSTime();
    if (getMSTimeDiff(m_lastUpdateTime, now) < UPDATE_INTERVAL_MS)
        return;

    m_lastUpdateTime = now;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    for (auto& [groupId, data] : m_groups)
    {
        data->Update(diff);
    }
}

void GroupAICoordinator::Clear()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_groups.clear();
}

size_t GroupAICoordinator::GetActiveGroupCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_groups.size();
}
