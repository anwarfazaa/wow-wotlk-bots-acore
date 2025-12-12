/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_INTENTBROADCASTER_H
#define _PLAYERBOT_INTENTBROADCASTER_H

#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "Timer.h"

/**
 * BotIntent - Types of intentions bots can broadcast
 */
enum class BotIntent : uint8
{
    NONE = 0,
    WILL_INTERRUPT,         // Bot intends to interrupt a cast
    NEED_HEAL,              // Bot requests healing
    NEED_DISPEL,            // Bot needs a dispel
    MOVING_TO_POSITION,     // Bot is moving to a specific position
    PULLING,                // Tank is about to pull
    USING_COOLDOWN,         // Bot is using a significant cooldown
    CROWD_CONTROLLING,      // Bot is about to CC a target
    TAUNTING,               // Tank is taunting
    BATTLE_REZ,             // Druid/DK is battle rezzing
    DISPELLING,             // Bot is dispelling a target
    POTION_USAGE,           // Bot is using a potion
    MANA_BREAK,             // Healer needs mana break
    READY_CHECK,            // Ready check response
    FOCUS_TARGET,           // Requesting others to focus this target

    MAX_INTENT
};

/**
 * Convert BotIntent to string for debugging
 */
inline const char* BotIntentToString(BotIntent intent)
{
    switch (intent)
    {
        case BotIntent::NONE: return "NONE";
        case BotIntent::WILL_INTERRUPT: return "WILL_INTERRUPT";
        case BotIntent::NEED_HEAL: return "NEED_HEAL";
        case BotIntent::NEED_DISPEL: return "NEED_DISPEL";
        case BotIntent::MOVING_TO_POSITION: return "MOVING_TO_POSITION";
        case BotIntent::PULLING: return "PULLING";
        case BotIntent::USING_COOLDOWN: return "USING_COOLDOWN";
        case BotIntent::CROWD_CONTROLLING: return "CROWD_CONTROLLING";
        case BotIntent::TAUNTING: return "TAUNTING";
        case BotIntent::BATTLE_REZ: return "BATTLE_REZ";
        case BotIntent::DISPELLING: return "DISPELLING";
        case BotIntent::POTION_USAGE: return "POTION_USAGE";
        case BotIntent::MANA_BREAK: return "MANA_BREAK";
        case BotIntent::READY_CHECK: return "READY_CHECK";
        case BotIntent::FOCUS_TARGET: return "FOCUS_TARGET";
        default: return "UNKNOWN";
    }
}

/**
 * BotIntentData - Full data for a broadcast intent
 */
struct BotIntentData
{
    BotIntent intentType = BotIntent::NONE;
    ObjectGuid broadcasterGuid;         // Who broadcast this intent
    ObjectGuid targetGuid;              // Target of the action (if applicable)
    uint32 spellId = 0;                 // Related spell (if applicable)
    Position targetPosition;            // Position (for movement intents)
    uint32 broadcastTime = 0;           // When this was broadcast (getMSTime)
    uint32 durationMs = 3000;           // How long this intent is valid
    uint8 priority = 1;                 // Higher = more important (for healing)
    uint32 groupId = 0;                 // Group this intent belongs to

    bool IsExpired() const
    {
        return getMSTimeDiff(broadcastTime, getMSTime()) > durationMs;
    }

    bool IsValid() const
    {
        return intentType != BotIntent::NONE && !broadcasterGuid.IsEmpty() && broadcastTime > 0;
    }

    uint32 GetRemainingTime() const
    {
        uint32 elapsed = getMSTimeDiff(broadcastTime, getMSTime());
        return elapsed >= durationMs ? 0 : durationMs - elapsed;
    }
};

/**
 * IntentQueryResult - Result of intent queries
 */
struct IntentQueryResult
{
    bool found = false;
    BotIntentData intent;

    IntentQueryResult() = default;
    explicit IntentQueryResult(const BotIntentData& data) : found(true), intent(data) {}

    operator bool() const { return found; }
};

/**
 * IntentBroadcaster - Global singleton for bot intent communication
 *
 * This system allows bots to announce their intentions and query what
 * other bots are planning to do, preventing duplicate actions like
 * two bots trying to interrupt the same cast.
 */
class IntentBroadcaster
{
public:
    static IntentBroadcaster* instance()
    {
        static IntentBroadcaster instance;
        return &instance;
    }

    // =========================================================================
    // Broadcasting intents
    // =========================================================================

    /**
     * Broadcast a general intent
     */
    bool BroadcastIntent(const BotIntentData& intent);

    /**
     * Broadcast a simple intent
     */
    bool BroadcastIntent(ObjectGuid broadcasterGuid, BotIntent type,
                         ObjectGuid targetGuid = ObjectGuid::Empty,
                         uint32 spellId = 0, uint32 durationMs = 3000,
                         uint8 priority = 1, uint32 groupId = 0);

    // Specialized broadcast methods for common intents
    bool BroadcastInterruptIntent(ObjectGuid broadcasterGuid, ObjectGuid targetGuid,
                                   uint32 spellId, uint32 durationMs = 2000);
    bool BroadcastHealingNeed(ObjectGuid broadcasterGuid, uint8 healthPct,
                               uint8 priority = 1, uint32 groupId = 0);
    bool BroadcastDispelNeed(ObjectGuid broadcasterGuid, uint32 auraSpellId,
                              uint32 groupId = 0);
    bool BroadcastMovingToPosition(ObjectGuid broadcasterGuid, const Position& pos,
                                    uint32 durationMs = 5000);
    bool BroadcastPulling(ObjectGuid broadcasterGuid, ObjectGuid targetGuid,
                          uint32 groupId = 0, uint32 durationMs = 5000);
    bool BroadcastCooldownUsage(ObjectGuid broadcasterGuid, uint32 spellId,
                                 uint32 durationMs = 0);
    bool BroadcastCrowdControl(ObjectGuid broadcasterGuid, ObjectGuid targetGuid,
                                uint32 spellId, uint32 durationMs = 30000);
    bool BroadcastTaunting(ObjectGuid broadcasterGuid, ObjectGuid targetGuid,
                            uint32 groupId = 0);
    bool BroadcastManaBreak(ObjectGuid broadcasterGuid, uint32 groupId = 0,
                             uint32 durationMs = 10000);

    // =========================================================================
    // Revoking intents
    // =========================================================================

    void RevokeIntent(ObjectGuid broadcasterGuid, BotIntent type);
    void RevokeAllIntents(ObjectGuid broadcasterGuid);
    void RevokeIntentsForTarget(ObjectGuid targetGuid, BotIntent type);
    void RevokeIntentsForGroup(uint32 groupId);

    // =========================================================================
    // Querying intents
    // =========================================================================

    /**
     * Get a specific intent from a broadcaster
     */
    IntentQueryResult GetIntent(ObjectGuid broadcasterGuid, BotIntent type) const;

    /**
     * Get all intents targeting a specific unit
     */
    std::vector<BotIntentData> GetIntentsForTarget(ObjectGuid targetGuid) const;

    /**
     * Get all intents of a specific type
     */
    std::vector<BotIntentData> GetIntentsByType(BotIntent type) const;

    /**
     * Get all intents for a group
     */
    std::vector<BotIntentData> GetIntentsByGroup(uint32 groupId) const;

    /**
     * Get all active heal requests, sorted by priority
     */
    std::vector<BotIntentData> GetHealingRequests(uint32 groupId = 0) const;

    // =========================================================================
    // Checking for claimed intents
    // =========================================================================

    /**
     * Check if an intent type is claimed for a target
     */
    bool IsIntentClaimed(BotIntent type, ObjectGuid targetGuid) const;

    /**
     * Check if an interrupt is claimed for a specific spell on a target
     */
    bool IsInterruptClaimed(ObjectGuid targetGuid, uint32 spellId) const;

    /**
     * Check if a crowd control intent exists for a target
     */
    bool IsCrowdControlClaimed(ObjectGuid targetGuid) const;

    /**
     * Check if a player is broadcasting a specific intent type
     */
    bool IsPlayerBroadcastingIntent(ObjectGuid broadcasterGuid, BotIntent type) const;

    /**
     * Get who has claimed an intent for a target
     */
    ObjectGuid GetIntentClaimer(BotIntent type, ObjectGuid targetGuid) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    uint32 CountActiveIntents(BotIntent type) const;
    uint32 CountHealingRequests(uint32 groupId = 0) const;
    uint32 CountActiveInterrupts() const;

    /**
     * Get the highest priority heal request in a group
     */
    IntentQueryResult GetHighestPriorityHealRequest(uint32 groupId = 0) const;

    // =========================================================================
    // Maintenance
    // =========================================================================

    void PruneExpiredIntents();
    void Update(uint32 diff);
    void Clear();

    /**
     * Get total number of active intents
     */
    size_t GetTotalIntentCount() const;

private:
    IntentBroadcaster() : m_lastPruneTime(0) {}
    ~IntentBroadcaster() = default;
    IntentBroadcaster(const IntentBroadcaster&) = delete;
    IntentBroadcaster& operator=(const IntentBroadcaster&) = delete;

    // Key generation for efficient lookups
    uint64 MakeBroadcasterKey(ObjectGuid broadcasterGuid, BotIntent type) const;

    // Internal helper to add/update indices
    void UpdateIndices(uint64 key, const BotIntentData& intent);
    void RemoveFromIndices(uint64 key, const BotIntentData& intent);

    // Main storage: composite key (broadcaster + type) -> IntentData
    std::unordered_map<uint64, BotIntentData> m_intents;

    // Index by target for quick lookups: targetGuid -> list of keys
    std::unordered_map<uint64, std::vector<uint64>> m_targetIndex;

    // Index by type: intentType -> list of keys
    std::unordered_map<uint8, std::vector<uint64>> m_typeIndex;

    // Index by group: groupId -> list of keys
    std::unordered_map<uint32, std::vector<uint64>> m_groupIndex;

    mutable std::shared_mutex m_mutex;

    uint32 m_lastPruneTime;
    static constexpr uint32 PRUNE_INTERVAL_MS = 1000;
};

#define sIntentBroadcaster IntentBroadcaster::instance()

#endif
