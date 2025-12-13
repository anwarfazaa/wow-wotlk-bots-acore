/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "IntentBroadcaster.h"

#include <algorithm>
#include <mutex>

// =========================================================================
// Key Generation
// =========================================================================

uint64 IntentBroadcaster::MakeBroadcasterKey(ObjectGuid broadcasterGuid, BotIntent type) const
{
    // Combine low part of GUID with intent type for a unique key
    // This ensures each bot can only have one intent of each type active
    return (static_cast<uint64>(broadcasterGuid.GetCounter()) << 8) | static_cast<uint8>(type);
}

// =========================================================================
// Index Management
// =========================================================================

void IntentBroadcaster::UpdateIndices(uint64 key, const BotIntentData& intent)
{
    // Index by target
    if (!intent.targetGuid.IsEmpty())
    {
        uint64 targetKey = intent.targetGuid.GetCounter();
        auto& targetList = m_targetIndex[targetKey];
        if (std::find(targetList.begin(), targetList.end(), key) == targetList.end())
            targetList.push_back(key);
    }

    // Index by type
    auto& typeList = m_typeIndex[static_cast<uint8>(intent.intentType)];
    if (std::find(typeList.begin(), typeList.end(), key) == typeList.end())
        typeList.push_back(key);

    // Index by group
    if (intent.groupId != 0)
    {
        auto& groupList = m_groupIndex[intent.groupId];
        if (std::find(groupList.begin(), groupList.end(), key) == groupList.end())
            groupList.push_back(key);
    }
}

void IntentBroadcaster::RemoveFromIndices(uint64 key, const BotIntentData& intent)
{
    // Remove from target index
    if (!intent.targetGuid.IsEmpty())
    {
        uint64 targetKey = intent.targetGuid.GetCounter();
        auto targetIt = m_targetIndex.find(targetKey);
        if (targetIt != m_targetIndex.end())
        {
            auto& vec = targetIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), key), vec.end());
            if (vec.empty())
                m_targetIndex.erase(targetIt);
        }
    }

    // Remove from type index
    auto typeIt = m_typeIndex.find(static_cast<uint8>(intent.intentType));
    if (typeIt != m_typeIndex.end())
    {
        auto& vec = typeIt->second;
        vec.erase(std::remove(vec.begin(), vec.end(), key), vec.end());
        if (vec.empty())
            m_typeIndex.erase(typeIt);
    }

    // Remove from group index
    if (intent.groupId != 0)
    {
        auto groupIt = m_groupIndex.find(intent.groupId);
        if (groupIt != m_groupIndex.end())
        {
            auto& vec = groupIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), key), vec.end());
            if (vec.empty())
                m_groupIndex.erase(groupIt);
        }
    }
}

// =========================================================================
// Broadcasting Intents
// =========================================================================

bool IntentBroadcaster::BroadcastIntent(const BotIntentData& intent)
{
    if (!intent.IsValid())
        return false;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    uint64 key = MakeBroadcasterKey(intent.broadcasterGuid, intent.intentType);

    // Remove old intent from indices if exists
    auto existingIt = m_intents.find(key);
    if (existingIt != m_intents.end())
    {
        RemoveFromIndices(key, existingIt->second);
    }

    // Store new intent
    m_intents[key] = intent;
    UpdateIndices(key, intent);

    return true;
}

bool IntentBroadcaster::BroadcastIntent(ObjectGuid broadcasterGuid, BotIntent type,
                                         ObjectGuid targetGuid, uint32 spellId,
                                         uint32 durationMs, uint8 priority, uint32 groupId)
{
    BotIntentData intent;
    intent.intentType = type;
    intent.broadcasterGuid = broadcasterGuid;
    intent.targetGuid = targetGuid;
    intent.spellId = spellId;
    intent.broadcastTime = getMSTime();
    intent.durationMs = durationMs;
    intent.priority = priority;
    intent.groupId = groupId;

    return BroadcastIntent(intent);
}

bool IntentBroadcaster::BroadcastInterruptIntent(ObjectGuid broadcasterGuid, ObjectGuid targetGuid,
                                                   uint32 spellId, uint32 durationMs)
{
    return BroadcastIntent(broadcasterGuid, BotIntent::WILL_INTERRUPT, targetGuid, spellId, durationMs, 1, 0);
}

bool IntentBroadcaster::BroadcastHealingNeed(ObjectGuid broadcasterGuid, uint8 healthPct,
                                               uint8 priority, uint32 groupId)
{
    BotIntentData intent;
    intent.intentType = BotIntent::NEED_HEAL;
    intent.broadcasterGuid = broadcasterGuid;
    intent.targetGuid = broadcasterGuid;  // Self-targeting for heals
    intent.broadcastTime = getMSTime();
    intent.durationMs = 5000;  // 5 second validity
    intent.priority = priority;
    intent.groupId = groupId;
    // Store health percent in spellId field (repurposed)
    intent.spellId = healthPct;

    return BroadcastIntent(intent);
}

bool IntentBroadcaster::BroadcastDispelNeed(ObjectGuid broadcasterGuid, uint32 auraSpellId, uint32 groupId)
{
    return BroadcastIntent(broadcasterGuid, BotIntent::NEED_DISPEL, broadcasterGuid, auraSpellId, 5000, 1, groupId);
}

bool IntentBroadcaster::BroadcastMovingToPosition(ObjectGuid broadcasterGuid, const Position& pos, uint32 durationMs)
{
    BotIntentData intent;
    intent.intentType = BotIntent::MOVING_TO_POSITION;
    intent.broadcasterGuid = broadcasterGuid;
    intent.targetPosition = pos;
    intent.broadcastTime = getMSTime();
    intent.durationMs = durationMs;
    intent.priority = 1;

    return BroadcastIntent(intent);
}

bool IntentBroadcaster::BroadcastPulling(ObjectGuid broadcasterGuid, ObjectGuid targetGuid,
                                          uint32 groupId, uint32 durationMs)
{
    return BroadcastIntent(broadcasterGuid, BotIntent::PULLING, targetGuid, 0, durationMs, 5, groupId);
}

bool IntentBroadcaster::BroadcastCooldownUsage(ObjectGuid broadcasterGuid, uint32 spellId, uint32 durationMs)
{
    // If durationMs is 0, use a default based on typical cooldown visibility
    uint32 duration = durationMs > 0 ? durationMs : 10000;
    return BroadcastIntent(broadcasterGuid, BotIntent::USING_COOLDOWN, ObjectGuid::Empty, spellId, duration, 1, 0);
}

bool IntentBroadcaster::BroadcastCrowdControl(ObjectGuid broadcasterGuid, ObjectGuid targetGuid,
                                                uint32 spellId, uint32 durationMs)
{
    return BroadcastIntent(broadcasterGuid, BotIntent::CROWD_CONTROLLING, targetGuid, spellId, durationMs, 3, 0);
}

bool IntentBroadcaster::BroadcastTaunting(ObjectGuid broadcasterGuid, ObjectGuid targetGuid, uint32 groupId)
{
    return BroadcastIntent(broadcasterGuid, BotIntent::TAUNTING, targetGuid, 0, 3000, 4, groupId);
}

bool IntentBroadcaster::BroadcastManaBreak(ObjectGuid broadcasterGuid, uint32 groupId, uint32 durationMs)
{
    return BroadcastIntent(broadcasterGuid, BotIntent::MANA_BREAK, ObjectGuid::Empty, 0, durationMs, 2, groupId);
}

// =========================================================================
// Revoking Intents
// =========================================================================

void IntentBroadcaster::RevokeIntent(ObjectGuid broadcasterGuid, BotIntent type)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    uint64 key = MakeBroadcasterKey(broadcasterGuid, type);
    auto it = m_intents.find(key);
    if (it != m_intents.end())
    {
        RemoveFromIndices(key, it->second);
        m_intents.erase(it);
    }
}

void IntentBroadcaster::RevokeAllIntents(ObjectGuid broadcasterGuid)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    // Remove all intents from this broadcaster
    for (uint8 i = 0; i < static_cast<uint8>(BotIntent::MAX_INTENT); ++i)
    {
        uint64 key = MakeBroadcasterKey(broadcasterGuid, static_cast<BotIntent>(i));
        auto it = m_intents.find(key);
        if (it != m_intents.end())
        {
            RemoveFromIndices(key, it->second);
            m_intents.erase(it);
        }
    }
}

void IntentBroadcaster::RevokeIntentsForTarget(ObjectGuid targetGuid, BotIntent type)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    uint64 targetKey = targetGuid.GetCounter();
    auto targetIt = m_targetIndex.find(targetKey);
    if (targetIt == m_targetIndex.end())
        return;

    // Copy keys to avoid iterator invalidation
    std::vector<uint64> keysToRemove;
    for (uint64 key : targetIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && intentIt->second.intentType == type)
        {
            keysToRemove.push_back(key);
        }
    }

    for (uint64 key : keysToRemove)
    {
        auto it = m_intents.find(key);
        if (it != m_intents.end())
        {
            RemoveFromIndices(key, it->second);
            m_intents.erase(it);
        }
    }
}

void IntentBroadcaster::RevokeIntentsForGroup(uint32 groupId)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    auto groupIt = m_groupIndex.find(groupId);
    if (groupIt == m_groupIndex.end())
        return;

    // Copy keys to avoid iterator invalidation
    std::vector<uint64> keysToRemove = groupIt->second;

    for (uint64 key : keysToRemove)
    {
        auto it = m_intents.find(key);
        if (it != m_intents.end())
        {
            RemoveFromIndices(key, it->second);
            m_intents.erase(it);
        }
    }
}

// =========================================================================
// Querying Intents
// =========================================================================

IntentQueryResult IntentBroadcaster::GetIntent(ObjectGuid broadcasterGuid, BotIntent type) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    uint64 key = MakeBroadcasterKey(broadcasterGuid, type);
    auto it = m_intents.find(key);
    if (it == m_intents.end())
        return IntentQueryResult();

    if (it->second.IsExpired())
        return IntentQueryResult();

    return IntentQueryResult(it->second);
}

std::vector<BotIntentData> IntentBroadcaster::GetIntentsForTarget(ObjectGuid targetGuid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<BotIntentData> results;
    uint64 targetKey = targetGuid.GetCounter();

    auto targetIt = m_targetIndex.find(targetKey);
    if (targetIt == m_targetIndex.end())
        return results;

    for (uint64 key : targetIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && !intentIt->second.IsExpired())
        {
            results.push_back(intentIt->second);
        }
    }

    return results;
}

std::vector<BotIntentData> IntentBroadcaster::GetIntentsByType(BotIntent type) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<BotIntentData> results;

    auto typeIt = m_typeIndex.find(static_cast<uint8>(type));
    if (typeIt == m_typeIndex.end())
        return results;

    for (uint64 key : typeIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && !intentIt->second.IsExpired())
        {
            results.push_back(intentIt->second);
        }
    }

    return results;
}

std::vector<BotIntentData> IntentBroadcaster::GetIntentsByGroup(uint32 groupId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<BotIntentData> results;

    auto groupIt = m_groupIndex.find(groupId);
    if (groupIt == m_groupIndex.end())
        return results;

    for (uint64 key : groupIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && !intentIt->second.IsExpired())
        {
            results.push_back(intentIt->second);
        }
    }

    return results;
}

std::vector<BotIntentData> IntentBroadcaster::GetHealingRequests(uint32 groupId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<BotIntentData> results;

    auto typeIt = m_typeIndex.find(static_cast<uint8>(BotIntent::NEED_HEAL));
    if (typeIt == m_typeIndex.end())
        return results;

    for (uint64 key : typeIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && !intentIt->second.IsExpired())
        {
            // Filter by group if specified
            if (groupId == 0 || intentIt->second.groupId == groupId)
            {
                results.push_back(intentIt->second);
            }
        }
    }

    // Sort by priority (highest first)
    std::sort(results.begin(), results.end(),
        [](const BotIntentData& a, const BotIntentData& b) {
            return a.priority > b.priority;
        });

    return results;
}

// =========================================================================
// Checking for Claimed Intents
// =========================================================================

bool IntentBroadcaster::IsIntentClaimed(BotIntent type, ObjectGuid targetGuid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    uint64 targetKey = targetGuid.GetCounter();
    auto targetIt = m_targetIndex.find(targetKey);
    if (targetIt == m_targetIndex.end())
        return false;

    for (uint64 key : targetIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() &&
            intentIt->second.intentType == type &&
            !intentIt->second.IsExpired())
        {
            return true;
        }
    }

    return false;
}

bool IntentBroadcaster::IsInterruptClaimed(ObjectGuid targetGuid, uint32 spellId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    uint64 targetKey = targetGuid.GetCounter();
    auto targetIt = m_targetIndex.find(targetKey);
    if (targetIt == m_targetIndex.end())
        return false;

    for (uint64 key : targetIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() &&
            intentIt->second.intentType == BotIntent::WILL_INTERRUPT &&
            !intentIt->second.IsExpired())
        {
            // If spellId is 0, match any interrupt on this target
            // Otherwise, match specific spell
            if (spellId == 0 || intentIt->second.spellId == spellId)
            {
                return true;
            }
        }
    }

    return false;
}

bool IntentBroadcaster::IsCrowdControlClaimed(ObjectGuid targetGuid) const
{
    return IsIntentClaimed(BotIntent::CROWD_CONTROLLING, targetGuid);
}

bool IntentBroadcaster::IsPlayerBroadcastingIntent(ObjectGuid broadcasterGuid, BotIntent type) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    uint64 key = MakeBroadcasterKey(broadcasterGuid, type);
    auto it = m_intents.find(key);
    if (it == m_intents.end())
        return false;

    return !it->second.IsExpired();
}

ObjectGuid IntentBroadcaster::GetIntentClaimer(BotIntent type, ObjectGuid targetGuid) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    uint64 targetKey = targetGuid.GetCounter();
    auto targetIt = m_targetIndex.find(targetKey);
    if (targetIt == m_targetIndex.end())
        return ObjectGuid::Empty;

    for (uint64 key : targetIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() &&
            intentIt->second.intentType == type &&
            !intentIt->second.IsExpired())
        {
            return intentIt->second.broadcasterGuid;
        }
    }

    return ObjectGuid::Empty;
}

// =========================================================================
// Statistics
// =========================================================================

uint32 IntentBroadcaster::CountActiveIntents(BotIntent type) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto typeIt = m_typeIndex.find(static_cast<uint8>(type));
    if (typeIt == m_typeIndex.end())
        return 0;

    uint32 count = 0;
    for (uint64 key : typeIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && !intentIt->second.IsExpired())
        {
            ++count;
        }
    }

    return count;
}

uint32 IntentBroadcaster::CountHealingRequests(uint32 groupId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto typeIt = m_typeIndex.find(static_cast<uint8>(BotIntent::NEED_HEAL));
    if (typeIt == m_typeIndex.end())
        return 0;

    uint32 count = 0;
    for (uint64 key : typeIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && !intentIt->second.IsExpired())
        {
            if (groupId == 0 || intentIt->second.groupId == groupId)
            {
                ++count;
            }
        }
    }

    return count;
}

uint32 IntentBroadcaster::CountActiveInterrupts() const
{
    return CountActiveIntents(BotIntent::WILL_INTERRUPT);
}

IntentQueryResult IntentBroadcaster::GetHighestPriorityHealRequest(uint32 groupId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto typeIt = m_typeIndex.find(static_cast<uint8>(BotIntent::NEED_HEAL));
    if (typeIt == m_typeIndex.end())
        return IntentQueryResult();

    BotIntentData* highest = nullptr;
    uint8 highestPriority = 0;

    for (uint64 key : typeIt->second)
    {
        auto intentIt = m_intents.find(key);
        if (intentIt != m_intents.end() && !intentIt->second.IsExpired())
        {
            if (groupId == 0 || intentIt->second.groupId == groupId)
            {
                if (intentIt->second.priority > highestPriority)
                {
                    highestPriority = intentIt->second.priority;
                    highest = const_cast<BotIntentData*>(&intentIt->second);
                }
            }
        }
    }

    if (highest)
        return IntentQueryResult(*highest);

    return IntentQueryResult();
}

// =========================================================================
// Maintenance
// =========================================================================

void IntentBroadcaster::PruneExpiredIntents()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    std::vector<uint64> keysToRemove;

    for (const auto& [key, intent] : m_intents)
    {
        if (intent.IsExpired())
        {
            keysToRemove.push_back(key);
        }
    }

    for (uint64 key : keysToRemove)
    {
        auto it = m_intents.find(key);
        if (it != m_intents.end())
        {
            RemoveFromIndices(key, it->second);
            m_intents.erase(it);
        }
    }
}

void IntentBroadcaster::Update(uint32 diff)
{
    uint32 now = getMSTime();
    if (getMSTimeDiff(m_lastPruneTime, now) >= PRUNE_INTERVAL_MS)
    {
        m_lastPruneTime = now;
        PruneExpiredIntents();
    }
}

void IntentBroadcaster::Clear()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    m_intents.clear();
    m_targetIndex.clear();
    m_typeIndex.clear();
    m_groupIndex.clear();
}

size_t IntentBroadcaster::GetTotalIntentCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_intents.size();
}
