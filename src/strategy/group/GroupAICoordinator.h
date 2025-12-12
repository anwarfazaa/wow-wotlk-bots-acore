/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_GROUPAICOORDINATOR_H
#define _PLAYERBOT_GROUPAICOORDINATOR_H

#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "Timer.h"

class Player;
class Unit;
class Group;
class PlayerbotAI;

/**
 * GroupRole - Combat roles for coordination
 */
enum class GroupRole : uint8
{
    NONE = 0,
    TANK,
    HEALER,
    MELEE_DPS,
    RANGED_DPS
};

/**
 * GroupReadyState - Ready check state for group members
 */
enum class GroupReadyState : uint8
{
    NOT_READY = 0,
    READY,
    AFK,
    BUFFING,
    DRINKING,
    WAITING_FOR_SUMMON
};

/**
 * TargetAssignment - Who is assigned to attack which target
 */
struct TargetAssignment
{
    ObjectGuid targetGuid;
    std::vector<ObjectGuid> assignedDps;
    ObjectGuid assignedTank;
    ObjectGuid assignedCC;          // Crowd controller
    uint32 ccSpellId = 0;           // CC spell being used
    uint8 priority = 0;             // Kill priority (higher = kill first)
    bool isFocusTarget = false;     // All DPS should switch to this

    bool HasAssignedCC() const { return !assignedCC.IsEmpty(); }
    bool HasAssignedTank() const { return !assignedTank.IsEmpty(); }
    size_t GetDpsCount() const { return assignedDps.size(); }
};

/**
 * GroupMemberInfo - Cached information about a group member
 */
struct GroupMemberInfo
{
    ObjectGuid guid;
    GroupRole role = GroupRole::NONE;
    GroupReadyState readyState = GroupReadyState::NOT_READY;
    uint8 healthPct = 100;
    uint8 manaPct = 100;
    uint32 lastUpdateTime = 0;
    bool isBot = false;
    bool isAlive = true;
    bool isInRange = true;          // Within coordination range
    Position position;

    bool NeedsUpdate() const
    {
        return getMSTimeDiff(lastUpdateTime, getMSTime()) > 500;
    }
};

/**
 * FormationPosition - Calculated position for a group member
 */
struct FormationPosition
{
    ObjectGuid memberGuid;
    Position targetPos;
    float distanceFromTarget;
    float angleFromTarget;
    bool isMelee;
};

/**
 * GroupCoordinatorData - Per-group coordination data
 */
class GroupCoordinatorData
{
public:
    explicit GroupCoordinatorData(uint32 groupId);

    // =========================================================================
    // Member Management
    // =========================================================================

    void AddMember(ObjectGuid guid, GroupRole role, bool isBot);
    void RemoveMember(ObjectGuid guid);
    void UpdateMemberInfo(ObjectGuid guid, const GroupMemberInfo& info);
    GroupMemberInfo* GetMemberInfo(ObjectGuid guid);
    const GroupMemberInfo* GetMemberInfo(ObjectGuid guid) const;
    std::vector<ObjectGuid> GetMembersByRole(GroupRole role) const;
    std::vector<ObjectGuid> GetAllMembers() const;
    bool HasMember(ObjectGuid guid) const;
    size_t GetMemberCount() const;

    // =========================================================================
    // Target Coordination
    // =========================================================================

    /**
     * Set the current kill target for the group
     */
    void SetFocusTarget(ObjectGuid targetGuid);
    ObjectGuid GetFocusTarget() const;

    /**
     * Assign a DPS to attack a specific target
     * Returns false if target is already at max DPS or if DPS is already assigned
     */
    bool AssignDpsToTarget(ObjectGuid dpsGuid, ObjectGuid targetGuid);

    /**
     * Get the recommended target for a DPS (balances DPS across targets)
     */
    ObjectGuid GetRecommendedTarget(ObjectGuid dpsGuid) const;

    /**
     * Set/clear the primary tank target
     */
    void SetTankTarget(ObjectGuid tankGuid, ObjectGuid targetGuid);
    ObjectGuid GetTankTarget(ObjectGuid tankGuid) const;

    /**
     * Register a crowd control assignment
     */
    bool AssignCrowdControl(ObjectGuid ccerGuid, ObjectGuid targetGuid, uint32 spellId);
    bool IsCrowdControlled(ObjectGuid targetGuid) const;
    ObjectGuid GetCrowdController(ObjectGuid targetGuid) const;

    /**
     * Get all target assignments
     */
    std::vector<TargetAssignment> GetTargetAssignments() const;

    /**
     * Clear all target assignments (end of combat)
     */
    void ClearTargetAssignments();

    // =========================================================================
    // Ready Check System
    // =========================================================================

    void SetMemberReadyState(ObjectGuid guid, GroupReadyState state);
    GroupReadyState GetMemberReadyState(ObjectGuid guid) const;
    bool IsGroupReady() const;
    float GetGroupReadyPercent() const;
    std::vector<ObjectGuid> GetNotReadyMembers() const;

    // =========================================================================
    // Resource Tracking
    // =========================================================================

    /**
     * Get average health percentage of the group
     */
    float GetAverageHealthPct() const;

    /**
     * Get average mana percentage of healers
     */
    float GetHealerManaPct() const;

    /**
     * Check if any healer is low on mana
     */
    bool NeedsManaBreak(uint8 threshold = 30) const;

    /**
     * Get members below health threshold
     */
    std::vector<ObjectGuid> GetLowHealthMembers(uint8 threshold = 50) const;

    // =========================================================================
    // Formation System
    // =========================================================================

    /**
     * Calculate optimal positions around a target
     */
    std::vector<FormationPosition> CalculateFormation(Unit* target) const;

    /**
     * Get the calculated position for a specific member
     */
    Position GetFormationPosition(ObjectGuid memberGuid, Unit* target) const;

    // =========================================================================
    // Pull Coordination
    // =========================================================================

    void SetPullTarget(ObjectGuid targetGuid);
    ObjectGuid GetPullTarget() const;
    bool IsPullInProgress() const;
    void StartPull(ObjectGuid tankGuid, ObjectGuid targetGuid);
    void EndPull();
    uint32 GetPullDuration() const;

    // =========================================================================
    // Movement Coordination
    // =========================================================================

    void SetGroupDestination(const Position& pos);
    Position GetGroupDestination() const;
    bool HasGroupDestination() const;
    void ClearGroupDestination();

    /**
     * Check if all members are at the destination
     */
    bool IsGroupAtDestination(float tolerance = 10.0f) const;

    // =========================================================================
    // Combat State
    // =========================================================================

    bool IsInCombat() const { return m_inCombat; }
    void SetInCombat(bool inCombat);
    uint32 GetCombatDuration() const;

    // =========================================================================
    // Maintenance
    // =========================================================================

    void Update(uint32 diff);
    void Clear();
    uint32 GetGroupId() const { return m_groupId; }

private:
    void PruneStaleAssignments();
    void UpdateMemberDistances(const Position& referencePos);

    uint32 m_groupId;
    std::unordered_map<uint64, GroupMemberInfo> m_members;
    std::unordered_map<uint64, TargetAssignment> m_targetAssignments;

    ObjectGuid m_focusTarget;
    ObjectGuid m_pullTarget;
    ObjectGuid m_pullingTank;
    uint32 m_pullStartTime = 0;

    Position m_groupDestination;
    bool m_hasDestination = false;

    bool m_inCombat = false;
    uint32 m_combatStartTime = 0;

    mutable std::shared_mutex m_mutex;

    static constexpr float MELEE_FORMATION_RADIUS = 3.0f;
    static constexpr float RANGED_FORMATION_RADIUS = 25.0f;
    static constexpr uint32 STALE_ASSIGNMENT_MS = 30000;
};

/**
 * GroupAICoordinator - Global coordinator managing all groups
 *
 * Provides coordination services across all bot groups including:
 * - Target assignment and balancing
 * - Ready checks
 * - Formation calculations
 * - Pull coordination
 */
class GroupAICoordinator
{
public:
    static GroupAICoordinator* instance()
    {
        static GroupAICoordinator instance;
        return &instance;
    }

    // =========================================================================
    // Group Management
    // =========================================================================

    /**
     * Get or create coordinator data for a group
     */
    GroupCoordinatorData* GetGroupData(uint32 groupId);
    GroupCoordinatorData* GetGroupData(Group* group);

    /**
     * Get coordinator data for a player's group
     */
    GroupCoordinatorData* GetPlayerGroupData(Player* player);
    GroupCoordinatorData* GetPlayerGroupData(ObjectGuid playerGuid);

    /**
     * Remove group data (when group disbands)
     */
    void RemoveGroupData(uint32 groupId);

    /**
     * Check if a group has coordinator data
     */
    bool HasGroupData(uint32 groupId) const;

    // =========================================================================
    // Quick Access Methods (for convenience)
    // =========================================================================

    /**
     * Get recommended target for a DPS bot
     */
    ObjectGuid GetRecommendedTarget(Player* bot);

    /**
     * Check if an interrupt is claimed for a target
     */
    bool IsInterruptClaimed(uint32 groupId, ObjectGuid targetGuid, uint32 spellId) const;

    /**
     * Claim an interrupt for a target
     */
    bool ClaimInterrupt(ObjectGuid botGuid, uint32 groupId, ObjectGuid targetGuid, uint32 spellId);

    /**
     * Check if group needs a mana break
     */
    bool GroupNeedsManaBreak(uint32 groupId, uint8 threshold = 30) const;

    /**
     * Check if group is ready to proceed
     */
    bool IsGroupReady(uint32 groupId) const;

    // =========================================================================
    // Role Detection
    // =========================================================================

    /**
     * Determine a player's combat role based on spec/gear
     */
    static GroupRole DetermineRole(Player* player);
    static GroupRole DetermineRole(PlayerbotAI* ai);

    // =========================================================================
    // Maintenance
    // =========================================================================

    void Update(uint32 diff);
    void Clear();
    size_t GetActiveGroupCount() const;

private:
    GroupAICoordinator() : m_lastUpdateTime(0) {}
    ~GroupAICoordinator() = default;
    GroupAICoordinator(const GroupAICoordinator&) = delete;
    GroupAICoordinator& operator=(const GroupAICoordinator&) = delete;

    std::unordered_map<uint32, std::unique_ptr<GroupCoordinatorData>> m_groups;
    mutable std::shared_mutex m_mutex;

    uint32 m_lastUpdateTime;
    static constexpr uint32 UPDATE_INTERVAL_MS = 100;
    static constexpr uint32 GROUP_CLEANUP_INTERVAL_MS = 60000;
};

#define sGroupAICoordinator GroupAICoordinator::instance()

#endif
