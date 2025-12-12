/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "ThreatStrategy.h"

#include "GenericSpellActions.h"
#include "Group.h"
#include "GroupAICoordinator.h"
#include "Map.h"
#include "Playerbots.h"

// Static member initialization
std::unordered_map<uint64, uint32> PullDelayMultiplier::s_combatStartTimes;

/**
 * Calculate a graduated threat multiplier based on current threat percentage.
 * Instead of a binary cliff (1.0 or 0.0), this provides a smooth reduction curve:
 * - 0-50% threat: full DPS (1.0)
 * - 50-70% threat: reduced DPS (0.7)
 * - 70-85% threat: heavily reduced DPS (0.3)
 * - 85-95% threat: minimal DPS (0.1)
 * - 95%+ threat: stop DPS (0.0)
 *
 * This prevents the jarring stop/start behavior and lets bots manage threat more naturally.
 */
static float CalculateGraduatedThreatMultiplier(uint8 threatPct)
{
    if (threatPct < 50)
        return 1.0f;       // Safe zone - full DPS
    else if (threatPct < 70)
        return 0.7f;       // Caution zone - reduce DPS
    else if (threatPct < 85)
        return 0.3f;       // Danger zone - heavily reduce DPS
    else if (threatPct < 95)
        return 0.1f;       // Critical zone - minimal actions only
    else
        return 0.0f;       // Stop zone - no threat-generating actions
}

/**
 * Calculate threat multiplier for AOE actions.
 * AOE is more dangerous for threat since it affects all mobs.
 * Use a more conservative curve that kicks in earlier.
 */
static float CalculateAoeThreatMultiplier(uint8 threatPct)
{
    if (threatPct < 30)
        return 1.0f;       // Safe zone for AOE
    else if (threatPct < 50)
        return 0.5f;       // Start reducing AOE earlier
    else if (threatPct < 70)
        return 0.2f;       // Heavy AOE reduction
    else
        return 0.0f;       // No AOE at high threat
}

float ThreatMultiplier::GetValue(Action* action)
{
    if (AI_VALUE(bool, "neglect threat"))
    {
        return 1.0f;
    }

    if (!action || action->getThreatType() == Action::ActionThreatType::None)
        return 1.0f;

    if (!AI_VALUE(bool, "group"))
        return 1.0f;

    // Check if this is an AOE action - use stricter threat management
    if (action->getThreatType() == Action::ActionThreatType::Aoe)
    {
        uint8 aoeThreat = AI_VALUE2(uint8, "threat", "aoe");
        return CalculateAoeThreatMultiplier(aoeThreat);
    }

    // Single target threat management with graduated curve
    uint8 threat = AI_VALUE2(uint8, "threat", "current target");
    return CalculateGraduatedThreatMultiplier(threat);
}

void ThreatStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new ThreatMultiplier(botAI));
    multipliers.push_back(new PullDelayMultiplier(botAI));
}

// ============================================================================
// PullDelayMultiplier Implementation
// ============================================================================

/**
 * Constants for pull delay timing (in milliseconds)
 */
namespace PullDelay
{
    constexpr uint32 PHASE_1_END = 1500;    // First 1.5s: no DPS
    constexpr uint32 PHASE_2_END = 3000;    // 1.5-3s: light DPS (30%)
    constexpr uint32 PHASE_3_END = 5000;    // 3-5s: moderate DPS (70%)
    // After PHASE_3_END: full DPS (100%)

    constexpr float PHASE_1_MULT = 0.0f;    // No threat-generating actions
    constexpr float PHASE_2_MULT = 0.3f;    // Light attacks
    constexpr float PHASE_3_MULT = 0.7f;    // Moderate attacks
    constexpr float PHASE_4_MULT = 1.0f;    // Full damage

    // Cleanup stale entries after 60 seconds
    constexpr uint32 STALE_ENTRY_MS = 60000;
}

float PullDelayMultiplier::GetValue(Action* action)
{
    // Only applies to threat-generating actions
    if (!action || action->getThreatType() == Action::ActionThreatType::None)
        return 1.0f;

    Player* bot = botAI->GetBot();
    if (!bot)
        return 1.0f;

    // Tanks don't delay - they need to establish threat immediately
    if (botAI->ContainsStrategy(STRATEGY_TYPE_TANK))
        return 1.0f;

    // Must be in combat
    if (!bot->IsInCombat())
    {
        // Clear our combat start time when out of combat
        uint64 guid = bot->GetGUID().GetRawValue();
        s_combatStartTimes.erase(guid);
        return 1.0f;
    }

    // Must be in a group to apply delay (solo play doesn't need it)
    Group* group = bot->GetGroup();
    if (!group)
        return 1.0f;

    uint64 guid = bot->GetGUID().GetRawValue();
    uint32 now = getMSTime();

    // Check if we have a recorded combat start time
    auto it = s_combatStartTimes.find(guid);
    if (it == s_combatStartTimes.end())
    {
        // First time seeing this bot in combat - record start time
        s_combatStartTimes[guid] = now;

        // Periodic cleanup of stale entries
        static uint32 lastCleanup = 0;
        if (now - lastCleanup > PullDelay::STALE_ENTRY_MS)
        {
            lastCleanup = now;
            for (auto iter = s_combatStartTimes.begin(); iter != s_combatStartTimes.end(); )
            {
                if (now - iter->second > PullDelay::STALE_ENTRY_MS)
                    iter = s_combatStartTimes.erase(iter);
                else
                    ++iter;
            }
        }

        return PullDelay::PHASE_1_MULT;  // Start in no-DPS phase
    }

    uint32 combatDuration = now - it->second;

    // Determine which phase we're in
    if (combatDuration < PullDelay::PHASE_1_END)
    {
        // Phase 1: No DPS - let tank get first hit and establish initial threat
        return PullDelay::PHASE_1_MULT;
    }
    else if (combatDuration < PullDelay::PHASE_2_END)
    {
        // Phase 2: Light DPS - auto-attacks and low-threat abilities
        return PullDelay::PHASE_2_MULT;
    }
    else if (combatDuration < PullDelay::PHASE_3_END)
    {
        // Phase 3: Moderate DPS - most abilities but no big cooldowns
        return PullDelay::PHASE_3_MULT;
    }
    else
    {
        // Phase 4: Full DPS - tank should have solid threat by now
        return PullDelay::PHASE_4_MULT;
    }
}

float FocusMultiplier::GetValue(Action* action)
{
    if (!action)
    {
        return 1.0f;
    }
    if (action->getThreatType() == Action::ActionThreatType::Aoe && !dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 0.0f;
    }
    if (dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 0.0f;
    }
    return 1.0f;
}

void FocusStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FocusMultiplier(botAI));
}
