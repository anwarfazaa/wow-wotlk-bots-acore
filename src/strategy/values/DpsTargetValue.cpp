/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "DpsTargetValue.h"

#include "Group.h"
#include "GroupAICoordinator.h"
#include "ObjectAccessor.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

// ============================================================================
// Constants for smart target switching
// ============================================================================
namespace
{
    // Health percentage thresholds
    constexpr float EXECUTE_HEALTH_PCT = 20.0f;         // Execute range
    constexpr float LOW_HEALTH_PCT = 35.0f;             // Low HP priority
    constexpr float SWITCH_HEALTH_DIFF_PCT = 15.0f;     // Min HP difference to consider switch

    // Time-to-kill thresholds (in seconds)
    constexpr float TTK_IMMEDIATE = 3.0f;               // Kill within 3 seconds - always switch
    constexpr float TTK_SOON = 8.0f;                    // Kill soon - high priority
    constexpr float TTK_MEDIUM = 15.0f;                 // Medium term target

    // Score bonuses/penalties
    constexpr float SCORE_EXECUTE_RANGE = 30.0f;        // Bonus for execute range
    constexpr float SCORE_LOW_HEALTH = 20.0f;           // Bonus for low health
    constexpr float SCORE_FOCUS_TARGET = 25.0f;         // Bonus for group focus target
    constexpr float SCORE_CURRENT_TARGET = 10.0f;       // Bonus to stay on current target (hysteresis)
    constexpr float SCORE_IN_RANGE = 15.0f;             // Bonus for being in attack range
    constexpr float SCORE_TTK_IMMEDIATE = 40.0f;        // Bonus for immediate kill
    constexpr float SCORE_TTK_SOON = 25.0f;             // Bonus for soon kill
    constexpr float SCORE_CLEAVE_POTENTIAL = 10.0f;     // Bonus for cleave potential

    // Hysteresis: don't switch unless new target is significantly better
    constexpr float SWITCH_THRESHOLD = 15.0f;
}

class FindMaxThreatGapTargetStrategy : public FindTargetStrategy
{
public:
    FindMaxThreatGapTargetStrategy(PlayerbotAI* botAI) : FindTargetStrategy(botAI), minThreat(0) {}

    void CheckAttacker(Unit* attacker, ThreatMgr* threatMgr) override
    {
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        Unit* victim = attacker->GetVictim();
        if (!result || CalcThreatGap(attacker, threatMgr) > CalcThreatGap(result, &result->GetThreatMgr()))
            result = attacker;
    }
    float CalcThreatGap(Unit* attacker, ThreatMgr* threatMgr)
    {
        Unit* victim = attacker->GetVictim();
        return threatMgr->GetThreat(victim) - threatMgr->GetThreat(attacker);
    }

protected:
    float minThreat;
};

// caster
class CasterFindTargetSmartStrategy : public FindTargetStrategy
{
public:
    CasterFindTargetSmartStrategy(PlayerbotAI* botAI, float dps)
        : FindTargetStrategy(botAI), dps_(dps), targetExpectedLifeTime(1000000)
    {
        result = nullptr;
    }

    void CheckAttacker(Unit* attacker, ThreatMgr* threatMgr) override
    {
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            ObjectGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
        }
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        float expectedLifeTime = attacker->GetHealth() / dps_;
        // Unit* victim = attacker->GetVictim();
        if (!result || IsBetter(attacker, result))
        {
            targetExpectedLifeTime = expectedLifeTime;
            result = attacker;
        }
    }
    bool IsBetter(Unit* new_unit, Unit* old_unit)
    {
        float new_time = new_unit->GetHealth() / dps_;
        float old_time = old_unit->GetHealth() / dps_;
        // [5-30] > (5-0] > (20-inf)
        int new_level = GetIntervalLevel(new_unit);
        int old_level = GetIntervalLevel(old_unit);
        if (new_level != old_level)
        {
            return new_level > old_level;
        }
        int32_t level = new_level;
        if (level % 10 == 2 || level % 10 == 0)
        {
            return new_time < old_time;
        }
        // dont switch targets when all of them with low health
        Unit* currentTarget = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        if (currentTarget == new_unit)
        {
            return true;
        }
        if (currentTarget == old_unit)
        {
            return false;
        }
        return new_time > old_time;
    }
    int32_t GetIntervalLevel(Unit* unit)
    {
        float time = unit->GetHealth() / dps_;
        float dis = unit->GetDistance(botAI->GetBot());
        float attackRange =
            botAI->IsRanged(botAI->GetBot()) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
        attackRange += 5.0f;
        int level = dis < attackRange ? 10 : 0;
        if (time >= 5 && time <= 30)
        {
            return level + 2;
        }
        if (time > 30)
        {
            return level;
        }
        return level + 1;
    }

protected:
    float dps_;
    float targetExpectedLifeTime;
};

// General
class GeneralFindTargetSmartStrategy : public FindTargetStrategy
{
public:
    GeneralFindTargetSmartStrategy(PlayerbotAI* botAI, float dps)
        : FindTargetStrategy(botAI), dps_(dps), targetExpectedLifeTime(1000000)
    {
    }

    void CheckAttacker(Unit* attacker, ThreatMgr* threatMgr) override
    {
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            ObjectGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
        }
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        float expectedLifeTime = attacker->GetHealth() / dps_;
        // Unit* victim = attacker->GetVictim();
        if (!result || IsBetter(attacker, result))
        {
            targetExpectedLifeTime = expectedLifeTime;
            result = attacker;
        }
    }
    bool IsBetter(Unit* new_unit, Unit* old_unit)
    {
        float new_time = new_unit->GetHealth() / dps_;
        float old_time = old_unit->GetHealth() / dps_;
        int new_level = GetIntervalLevel(new_unit);
        int old_level = GetIntervalLevel(old_unit);
        if (new_level != old_level)
        {
            return new_level > old_level;
        }
        // attack enemy in range and with lowest health
        int level = new_level;
        if (level == 10)
        {
            return new_time < old_time;
        }
        // all targets are far away, choose the closest one
        return botAI->GetBot()->GetDistance(new_unit) < botAI->GetBot()->GetDistance(old_unit);
    }
    int32_t GetIntervalLevel(Unit* unit)
    {
        float time = unit->GetHealth() / dps_;
        float dis = unit->GetDistance(botAI->GetBot());
        float attackRange =
            botAI->IsRanged(botAI->GetBot()) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
        attackRange += 5.0f;
        int level = dis < attackRange ? 10 : 0;
        return level;
    }

protected:
    float dps_;
    float targetExpectedLifeTime;
};

// combo
class ComboFindTargetSmartStrategy : public FindTargetStrategy
{
public:
    ComboFindTargetSmartStrategy(PlayerbotAI* botAI, float dps)
        : FindTargetStrategy(botAI), dps_(dps), targetExpectedLifeTime(1000000)
    {
    }

    void CheckAttacker(Unit* attacker, ThreatMgr* threatMgr) override
    {
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            ObjectGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
        }
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        float expectedLifeTime = attacker->GetHealth() / dps_;
        // Unit* victim = attacker->GetVictim();
        if (!result || IsBetter(attacker, result))
        {
            targetExpectedLifeTime = expectedLifeTime;
            result = attacker;
        }
    }
    bool IsBetter(Unit* new_unit, Unit* old_unit)
    {
        float new_time = new_unit->GetHealth() / dps_;
        float old_time = old_unit->GetHealth() / dps_;
        // [5-20] > (5-0] > (20-inf)
        int new_level = GetIntervalLevel(new_unit);
        int old_level = GetIntervalLevel(old_unit);
        if (new_level != old_level)
        {
            return new_level > old_level;
        }
        // attack enemy in range and with lowest health
        int level = new_level;
        Player* bot = botAI->GetBot();
        if (level == 10)
        {
            Unit* combo_unit = bot->GetComboTarget();
            if (new_unit == combo_unit)
            {
                return true;
            }
            return new_time < old_time;
        }
        // all targets are far away, choose the closest one
        return bot->GetDistance(new_unit) < bot->GetDistance(old_unit);
    }
    int32_t GetIntervalLevel(Unit* unit)
    {
        float time = unit->GetHealth() / dps_;
        float dis = unit->GetDistance(botAI->GetBot());
        float attackRange =
            botAI->IsRanged(botAI->GetBot()) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
        attackRange += 5.0f;
        int level = dis < attackRange ? 10 : 0;
        return level;
    }

protected:
    float dps_;
    float targetExpectedLifeTime;
};

Unit* DpsTargetValue::Calculate()
{
    // Priority 1: Raid target icons (skull, X, etc.)
    Unit* rti = RtiTargetValue::Calculate();
    if (rti)
        return rti;

    // Priority 2: Check group coordinator focus target
    Group* group = bot->GetGroup();
    if (group)
    {
        uint32 groupId = group->GetGUID().GetCounter();
        GroupCoordinatorData* coordData = sGroupAICoordinator->GetGroupData(groupId);
        if (coordData)
        {
            ObjectGuid focusGuid = coordData->GetFocusTarget();
            if (!focusGuid.IsEmpty())
            {
                Unit* focusTarget = ObjectAccessor::GetUnit(*bot, focusGuid);
                if (focusTarget && focusTarget->IsAlive() && bot->IsValidAttackTarget(focusTarget))
                {
                    return focusTarget;
                }
            }
        }
    }

    float dps = AI_VALUE(float, "estimated group dps");
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // Priority 3: Smart target selection with low HP focus
    // First, gather all potential targets and score them
    Unit* bestTarget = nullptr;
    float bestScore = -1000.0f;

    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const& guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (!attacker || !attacker->IsAlive())
            continue;

        // Skip CC targets (moon marker = index 4)
        if (group)
        {
            ObjectGuid ccGuid = group->GetTargetIcon(4);
            if (ccGuid && attacker->GetGUID() == ccGuid)
                continue;
        }

        float score = CalculateSwitchScore(attacker, currentTarget, dps);

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = attacker;
        }
    }

    // Apply hysteresis: only switch if significantly better
    if (currentTarget && currentTarget->IsAlive() && bestTarget != currentTarget)
    {
        float currentScore = CalculateSwitchScore(currentTarget, currentTarget, dps);
        if (bestScore < currentScore + SWITCH_THRESHOLD)
        {
            // Not worth switching
            return currentTarget;
        }
    }

    if (bestTarget)
        return bestTarget;

    // Fallback to original strategies if no targets found via attackers list
    if (botAI->GetNearGroupMemberCount() > 3)
    {
        if (botAI->IsCaster(bot))
        {
            CasterFindTargetSmartStrategy strategy(botAI, dps);
            return TargetValue::FindTarget(&strategy);
        }
        else if (botAI->IsCombo(bot))
        {
            ComboFindTargetSmartStrategy strategy(botAI, dps);
            return TargetValue::FindTarget(&strategy);
        }
    }
    GeneralFindTargetSmartStrategy strategy(botAI, dps);
    return TargetValue::FindTarget(&strategy);
}

bool DpsTargetValue::ShouldSwitchToLowHealthTarget(Unit* currentTarget, Unit* lowHealthTarget, float groupDps)
{
    if (!lowHealthTarget || !lowHealthTarget->IsAlive())
        return false;

    // Always switch to targets that will die within TTK_IMMEDIATE seconds
    float ttk = lowHealthTarget->GetHealth() / std::max(1.0f, groupDps);
    if (ttk <= TTK_IMMEDIATE)
        return true;

    // Check health difference
    float currentHealthPct = currentTarget ? currentTarget->GetHealthPct() : 100.0f;
    float lowHealthPct = lowHealthTarget->GetHealthPct();

    // If target is in execute range and current target isn't, consider switching
    if (lowHealthPct <= EXECUTE_HEALTH_PCT && currentHealthPct > EXECUTE_HEALTH_PCT)
    {
        // Check if we can actually reach the target
        float distance = bot->GetDistance(lowHealthTarget);
        float attackRange = botAI->IsRanged(bot) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
        if (distance <= attackRange + 5.0f)
            return true;
    }

    // General case: switch if health difference is significant
    return (currentHealthPct - lowHealthPct) >= SWITCH_HEALTH_DIFF_PCT;
}

float DpsTargetValue::CalculateSwitchScore(Unit* target, Unit* currentTarget, float groupDps)
{
    if (!target || !target->IsAlive())
        return -1000.0f;

    float score = 0.0f;
    float healthPct = target->GetHealthPct();
    float ttk = target->GetHealth() / std::max(1.0f, groupDps);

    // =========================================================================
    // Time-to-kill bonuses (prioritize quick kills)
    // =========================================================================
    if (ttk <= TTK_IMMEDIATE)
        score += SCORE_TTK_IMMEDIATE;
    else if (ttk <= TTK_SOON)
        score += SCORE_TTK_SOON;

    // =========================================================================
    // Health-based bonuses
    // =========================================================================
    if (healthPct <= EXECUTE_HEALTH_PCT)
        score += SCORE_EXECUTE_RANGE;
    else if (healthPct <= LOW_HEALTH_PCT)
        score += SCORE_LOW_HEALTH;

    // =========================================================================
    // Range bonus
    // =========================================================================
    float distance = bot->GetDistance(target);
    float attackRange = botAI->IsRanged(bot) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
    if (distance <= attackRange + 5.0f)
        score += SCORE_IN_RANGE;

    // =========================================================================
    // Focus target bonus
    // =========================================================================
    Group* group = bot->GetGroup();
    if (group)
    {
        uint32 groupId = group->GetGUID().GetCounter();
        GroupCoordinatorData* coordData = sGroupAICoordinator->GetGroupData(groupId);
        if (coordData && coordData->GetFocusTarget() == target->GetGUID())
        {
            score += SCORE_FOCUS_TARGET;
        }
    }

    // =========================================================================
    // Current target bonus (hysteresis)
    // =========================================================================
    if (currentTarget && target == currentTarget)
        score += SCORE_CURRENT_TARGET;

    // =========================================================================
    // Cleave potential bonus (for melee)
    // =========================================================================
    if (!botAI->IsRanged(bot))
    {
        // Check how many enemies are near this target (cleave potential)
        int nearbyEnemies = 0;
        GuidVector attackers = AI_VALUE(GuidVector, "attackers");
        for (ObjectGuid const& guid : attackers)
        {
            Unit* other = botAI->GetUnit(guid);
            if (other && other != target && other->IsAlive())
            {
                if (target->GetDistance(other) <= 8.0f)  // Typical cleave range
                    nearbyEnemies++;
            }
        }
        if (nearbyEnemies >= 2)
            score += SCORE_CLEAVE_POTENTIAL;
    }

    // =========================================================================
    // Combo point consideration
    // =========================================================================
    if (botAI->IsCombo(bot) && currentTarget == target && bot->GetComboTarget() == target)
    {
        // Bonus for target we have combo points on
        uint8 comboPoints = bot->GetComboPoints();
        score += comboPoints * 5.0f;  // Up to 25 bonus for 5 combo points
    }

    // =========================================================================
    // High priority target bonus
    // =========================================================================
    // TODO: Could check creature type, boss status, etc.

    return score;
}

class FindMaxHpTargetStrategy : public FindTargetStrategy
{
public:
    FindMaxHpTargetStrategy(PlayerbotAI* botAI) : FindTargetStrategy(botAI), maxHealth(0) {}

    void CheckAttacker(Unit* attacker, ThreatMgr* threatMgr) override
    {
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            ObjectGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
        }

        if (!result || result->GetHealth() < attacker->GetHealth())
            result = attacker;
    }

protected:
    float maxHealth;
};

Unit* DpsAoeTargetValue::Calculate()
{
    Unit* rti = RtiTargetValue::Calculate();
    if (rti)
        return rti;

    FindMaxHpTargetStrategy strategy(botAI);
    return TargetValue::FindTarget(&strategy);
}
