/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_THREATSTRATEGY_H
#define _PLAYERBOT_THREATSTRATEGY_H

#include "Strategy.h"
#include <unordered_map>

class PlayerbotAI;

class ThreatMultiplier : public Multiplier
{
public:
    ThreatMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "threat") {}

    float GetValue(Action* action) override;
};

/**
 * PullDelayMultiplier - Delays DPS after tank pull to let tank establish threat
 *
 * Behavior:
 * - First 1.5 seconds: 0% DPS (let tank get initial hit)
 * - 1.5-3 seconds: 30% DPS (light attacks only)
 * - 3-5 seconds: 70% DPS (moderate attacks)
 * - After 5 seconds: 100% DPS (full damage)
 *
 * This prevents DPS from pulling aggro immediately after tank engages.
 * Only applies to non-tank roles and threat-generating actions.
 */
class PullDelayMultiplier : public Multiplier
{
public:
    PullDelayMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "pull delay") {}

    float GetValue(Action* action) override;

private:
    // Track combat start time per bot
    static std::unordered_map<uint64, uint32> s_combatStartTimes;
};

class ThreatStrategy : public Strategy
{
public:
    ThreatStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "threat"; }
};

class FocusMultiplier : public Multiplier
{
public:
    FocusMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "focus") {}

    float GetValue(Action* action) override;
};

class FocusStrategy : public Strategy
{
public:
    FocusStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "focus"; }
};

#endif
