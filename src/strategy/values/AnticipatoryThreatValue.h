/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_ANTICIPATORYTHREATVALUE_H
#define _PLAYERBOT_ANTICIPATORYTHREATVALUE_H

#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "Value.h"
#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"

class Unit;
class Spell;

/**
 * ThreatMechanicType - Types of boss mechanics that require response
 */
enum class ThreatMechanicType
{
    NONE = 0,
    SPREAD,             // Players need to spread out
    STACK,              // Players need to stack together
    INTERRUPT,          // Spell needs interrupting
    DISPEL,             // Debuff needs dispelling
    MOVE_AWAY,          // Move away from boss/location
    MOVE_TO,            // Move to boss/location (e.g., Loken Lightning Nova)
    AVOID_GROUND,       // Avoid ground effect
    FACE_AWAY,          // Turn away from boss
    STOP_CASTING,       // Stop all casting
    TANK_SWAP           // Tanks need to swap
};

/**
 * BossAbilityData - Loaded from database, cached per boss
 */
struct BossAbilityData
{
    uint32 bossEntry = 0;
    std::string bossName;
    uint32 spellId = 0;
    std::string abilityName;
    uint32 cooldownMs = 0;
    uint32 castTimeMs = 0;
    uint32 baseDamage = 0;
    bool isAoe = false;
    float aoeRadius = 0.0f;
    bool requiresMovement = false;
    float safeMoveDistance = 0.0f;
    uint8 severity = 50;            // 0-100
    ThreatMechanicType mechanic = ThreatMechanicType::NONE;
    std::string description;

    bool IsDangerous() const { return severity >= 60; }
    bool IsCritical() const { return severity >= 80; }
    bool NeedsResponse() const { return mechanic != ThreatMechanicType::NONE; }
};

/**
 * ActiveThreat - Currently active threat (boss casting, ground effect, etc.)
 */
struct ActiveThreat
{
    ObjectGuid sourceGuid;          // Who is creating the threat
    uint32 spellId = 0;             // Spell being cast
    const BossAbilityData* ability = nullptr;  // Ability data (if known)
    Position sourcePosition;        // Where the threat originates
    Position targetPosition;        // Target location (for ground effects)
    uint32 castStartTime = 0;       // When cast started
    uint32 estimatedImpactTime = 0; // When damage will land
    uint32 estimatedDamage = 0;     // Expected damage
    bool isTargetingBot = false;    // Is this bot the target
    ThreatMechanicType mechanic = ThreatMechanicType::NONE;

    uint32 GetRemainingCastTime() const
    {
        if (estimatedImpactTime == 0)
            return 0;
        uint32 now = getMSTime();
        if (now >= estimatedImpactTime)
            return 0;
        return estimatedImpactTime - now;
    }

    bool IsImminent() const { return GetRemainingCastTime() < 2000; }
    bool IsUrgent() const { return GetRemainingCastTime() < 1000; }
};

/**
 * ThreatPrediction - Predicted incoming damage/mechanic
 */
struct ThreatPrediction
{
    float expectedDamage = 0.0f;
    float expectedDamagePercent = 0.0f;  // As percent of bot's health
    ThreatMechanicType suggestedResponse = ThreatMechanicType::NONE;
    Position safePosition;               // Where to move if needed
    bool shouldPreposition = false;
    bool shouldInterrupt = false;
    bool shouldDispel = false;
    uint32 urgencyMs = 0;               // How soon to respond

    bool RequiresAction() const
    {
        return suggestedResponse != ThreatMechanicType::NONE ||
               shouldPreposition || shouldInterrupt || shouldDispel;
    }
};

/**
 * AnticipatoryThreatManager - Global singleton for boss ability data
 */
class AnticipatoryThreatManager
{
public:
    static AnticipatoryThreatManager* instance()
    {
        static AnticipatoryThreatManager instance;
        return &instance;
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    void Initialize();
    void Reload();
    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Boss Ability Queries
    // =========================================================================

    /**
     * Get all known abilities for a boss
     */
    std::vector<const BossAbilityData*> GetBossAbilities(uint32 bossEntry) const;

    /**
     * Get specific ability data by spell ID
     */
    const BossAbilityData* GetAbilityData(uint32 bossEntry, uint32 spellId) const;

    /**
     * Get ability by spell ID (any boss)
     */
    const BossAbilityData* GetAbilityBySpell(uint32 spellId) const;

    /**
     * Check if a spell is a known dangerous ability
     */
    bool IsDangerousAbility(uint32 spellId) const;

    /**
     * Check if we have data for a boss
     */
    bool HasBossData(uint32 bossEntry) const;

    // =========================================================================
    // Mechanic Type Helpers
    // =========================================================================

    static ThreatMechanicType ParseMechanicType(const std::string& str);
    static const char* MechanicTypeToString(ThreatMechanicType type);

    // =========================================================================
    // Statistics
    // =========================================================================

    size_t GetBossCount() const;
    size_t GetTotalAbilityCount() const;

private:
    AnticipatoryThreatManager() : m_initialized(false) {}
    ~AnticipatoryThreatManager() = default;
    AnticipatoryThreatManager(const AnticipatoryThreatManager&) = delete;
    AnticipatoryThreatManager& operator=(const AnticipatoryThreatManager&) = delete;

    void LoadFromDB();

    // bossEntry -> list of abilities
    std::unordered_map<uint32, std::vector<BossAbilityData>> m_bossAbilities;
    // spellId -> (bossEntry, index in abilities vector) for safe lookup after reallocation
    std::unordered_map<uint32, std::pair<uint32, size_t>> m_spellIndex;

    mutable std::shared_mutex m_mutex;
    bool m_initialized;
};

#define sAnticipatoryThreat AnticipatoryThreatManager::instance()

// ============================================================================
// Value Classes
// ============================================================================

/**
 * IncomingDamageValue - Calculate expected incoming damage
 */
class IncomingDamageValue : public Uint32CalculatedValue
{
public:
    IncomingDamageValue(PlayerbotAI* ai) : Uint32CalculatedValue(ai, "incoming damage", 1) {}
    uint32 Calculate() override;

private:
    uint32 CalculateFromActiveCasts();
    uint32 CalculateFromAuras();
};

/**
 * ActiveThreatsValue - Get list of active threats
 */
class ActiveThreatsValue : public CalculatedValue<std::vector<ActiveThreat>>
{
public:
    ActiveThreatsValue(PlayerbotAI* ai) : CalculatedValue<std::vector<ActiveThreat>>(ai, "active threats", 1) {}
    std::vector<ActiveThreat> Calculate() override;

private:
    void ScanEnemyCasts(std::vector<ActiveThreat>& threats);
    void ScanGroundEffects(std::vector<ActiveThreat>& threats);
};

/**
 * ThreatPredictionValue - Get threat prediction for bot
 */
class ThreatPredictionValue : public CalculatedValue<ThreatPrediction>
{
public:
    ThreatPredictionValue(PlayerbotAI* ai) : CalculatedValue<ThreatPrediction>(ai, "threat prediction", 1) {}
    ThreatPrediction Calculate() override;

private:
    void AnalyzeThreats(const std::vector<ActiveThreat>& threats, ThreatPrediction& prediction);
    Position CalculateSafePosition(const ActiveThreat& threat);
};

/**
 * ShouldPrepositionValue - Should bot move preemptively
 */
class ShouldPrepositionValue : public BoolCalculatedValue
{
public:
    ShouldPrepositionValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "should preposition", 1) {}
    bool Calculate() override;
};

/**
 * PreemptiveHealTargetsValue - Get targets needing preemptive healing
 */
class PreemptiveHealTargetsValue : public CalculatedValue<std::vector<ObjectGuid>>
{
public:
    PreemptiveHealTargetsValue(PlayerbotAI* ai)
        : CalculatedValue<std::vector<ObjectGuid>>(ai, "preemptive heal targets", 2) {}
    std::vector<ObjectGuid> Calculate() override;
};

/**
 * HighestThreatValue - Get the most dangerous current threat
 */
class HighestThreatValue : public CalculatedValue<ActiveThreat>
{
public:
    HighestThreatValue(PlayerbotAI* ai) : CalculatedValue<ActiveThreat>(ai, "highest threat", 1) {}
    ActiveThreat Calculate() override;
};

/**
 * SafePositionValue - Get safe position for current threats
 */
class SafePositionValue : public CalculatedValue<Position>
{
public:
    SafePositionValue(PlayerbotAI* ai) : CalculatedValue<Position>(ai, "safe position", 1) {}
    Position Calculate() override;
};

/**
 * AtDungeonWaypointValue - Check if bot is at current dungeon waypoint
 */
class AtDungeonWaypointValue : public BoolCalculatedValue
{
public:
    AtDungeonWaypointValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "at dungeon waypoint", 2) {}
    bool Calculate() override;
};

#endif
