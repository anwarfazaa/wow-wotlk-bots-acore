/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_DPSTARGETVALUE_H
#define _PLAYERBOT_DPSTARGETVALUE_H

#include "RtiTargetValue.h"

class PlayerbotAI;

/**
 * DpsTargetValue - Intelligent DPS target selection
 *
 * Selection priority:
 * 1. Raid target icons (skull, X, etc.)
 * 2. Group coordinator focus target
 * 3. Low HP targets (execute range for quick kills)
 * 4. Targets near death threshold based on group DPS
 * 5. Range and cleave efficiency considerations
 *
 * Features:
 * - Smart target switching to quickly eliminate threats
 * - Hysteresis to prevent excessive target swapping
 * - Cleave awareness for multi-target efficiency
 * - Role-specific considerations (caster, melee, combo)
 */
class DpsTargetValue : public RtiTargetValue
{
public:
    DpsTargetValue(PlayerbotAI* botAI, std::string const type = "rti", std::string const name = "dps target")
        : RtiTargetValue(botAI, type, name)
    {
    }

    Unit* Calculate() override;

private:
    /**
     * Check if target is in execute range (<20% health) and worth switching to
     */
    bool ShouldSwitchToLowHealthTarget(Unit* currentTarget, Unit* lowHealthTarget, float groupDps);

    /**
     * Calculate target switch score (higher = more valuable to switch)
     */
    float CalculateSwitchScore(Unit* target, Unit* currentTarget, float groupDps);
};

class DpsAoeTargetValue : public RtiTargetValue
{
public:
    DpsAoeTargetValue(PlayerbotAI* botAI, std::string const type = "rti", std::string const name = "dps aoe target")
        : RtiTargetValue(botAI, type, name)
    {
    }

    Unit* Calculate() override;
};

#endif
