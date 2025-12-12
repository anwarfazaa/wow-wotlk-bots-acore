/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_PARTYMEMBERTOHEAL_H
#define _PLAYERBOT_PARTYMEMBERTOHEAL_H

#include "PartyMemberValue.h"
#include <unordered_map>

class Pet;
class PlayerbotAI;
class Unit;

/**
 * HealthTrendTracker - Tracks health changes over time for predictive healing
 */
struct HealthTrendData
{
    uint32 lastHealth = 0;
    uint32 lastMaxHealth = 0;
    uint32 lastUpdateTime = 0;
    float healthChangeRate = 0.0f;     // Health change per second (negative = damage intake)
    float avgDamageIntake = 0.0f;       // Average damage per second over recent window
    uint8 sampleCount = 0;
};

/**
 * PartyMemberToHeal - Enhanced with predictive healing
 *
 * Priority calculation considers:
 * 1. Current health percentage
 * 2. Predicted health after incoming damage
 * 3. Role priority (tank > healer > dps)
 * 4. Health trend (rapidly dropping health gets priority)
 * 5. Distance penalty
 */
class PartyMemberToHeal : public PartyMemberValue
{
public:
    PartyMemberToHeal(PlayerbotAI* botAI, std::string const name = "party member to heal")
        : PartyMemberValue(botAI, name)
    {
    }

protected:
    Unit* Calculate() override;
    bool Check(Unit* player) override;

private:
    /**
     * Calculate heal priority score for a unit (lower = higher priority)
     */
    float CalculateHealPriority(Unit* unit, bool isRaid);

    /**
     * Get role priority bonus (subtracted from score)
     * Tank = -20, Healer = -10, DPS = 0
     */
    float GetRolePriorityBonus(Unit* unit);

    /**
     * Get incoming damage for a unit from AnticipatoryThreat system
     */
    uint32 GetIncomingDamage(Unit* unit);

    /**
     * Get health trend penalty (added to score if health stable, subtracted if dropping fast)
     */
    float GetHealthTrendPenalty(Unit* unit);

    /**
     * Update health trend tracking for a unit
     */
    void UpdateHealthTrend(Unit* unit);

    // Health trend tracking per unit (static to persist across calculations)
    static std::unordered_map<uint64, HealthTrendData> s_healthTrends;
};

class PartyMemberToProtect : public PartyMemberValue
{
public:
    PartyMemberToProtect(PlayerbotAI* botAI, std::string const name = "party member to protect")
        : PartyMemberValue(botAI, name)
    {
    }

protected:
    Unit* Calculate() override;
};

#endif
