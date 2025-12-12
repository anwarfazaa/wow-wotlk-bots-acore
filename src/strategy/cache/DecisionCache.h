/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_DECISIONCACHE_H
#define _PLAYERBOT_DECISIONCACHE_H

#include <any>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "Common.h"
#include "ObjectGuid.h"
#include "Timer.h"

/**
 * GameStateHash - Represents a snapshot of game state for cache invalidation
 * Used to determine if cached decisions are still valid
 */
struct GameStateHash
{
    uint8 targetHealthPct;      // Target health percentage (0-100)
    uint8 botHealthPct;         // Bot health percentage (0-100)
    uint8 botManaPct;           // Bot mana/power percentage (0-100)
    uint8 targetCount;          // Number of targets in combat
    uint8 groupMemberCount;     // Number of group members
    bool inCombat;              // Combat state
    bool targetExists;          // Has valid target
    bool isMoving;              // Bot is moving
    uint32 mapId;               // Current map

    GameStateHash()
        : targetHealthPct(0), botHealthPct(0), botManaPct(0), targetCount(0),
          groupMemberCount(0), inCombat(false), targetExists(false), isMoving(false), mapId(0)
    {}

    size_t Hash() const
    {
        size_t h = 0;
        h ^= std::hash<uint8>()(targetHealthPct) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint8>()(botHealthPct) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint8>()(botManaPct) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint8>()(targetCount) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint8>()(groupMemberCount) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>()(inCombat) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>()(targetExists) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>()(isMoving) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32>()(mapId) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }

    bool operator==(const GameStateHash& other) const
    {
        return targetHealthPct == other.targetHealthPct &&
               botHealthPct == other.botHealthPct &&
               botManaPct == other.botManaPct &&
               targetCount == other.targetCount &&
               groupMemberCount == other.groupMemberCount &&
               inCombat == other.inCombat &&
               targetExists == other.targetExists &&
               isMoving == other.isMoving &&
               mapId == other.mapId;
    }

    bool operator!=(const GameStateHash& other) const
    {
        return !(*this == other);
    }
};

/**
 * CachedDecision - Holds a cached value with state hash and timestamp
 */
template<typename T>
struct CachedDecision
{
    T value;
    GameStateHash stateHash;
    uint32 cacheTime;

    CachedDecision() : cacheTime(0) {}
    CachedDecision(const T& val, const GameStateHash& hash)
        : value(val), stateHash(hash), cacheTime(getMSTime()) {}

    bool IsValid(const GameStateHash& currentState, uint32 maxAgeMs) const
    {
        if (cacheTime == 0)
            return false;

        uint32 age = getMSTimeDiff(cacheTime, getMSTime());
        if (age > maxAgeMs)
            return false;

        return stateHash == currentState;
    }

    uint32 GetAge() const
    {
        return getMSTimeDiff(cacheTime, getMSTime());
    }
};

/**
 * DecisionCache - Per-bot cache for expensive calculations
 * Thread-safe for concurrent access
 */
class DecisionCache
{
public:
    DecisionCache() : m_lastPruneTime(getMSTime()) {}

    /**
     * Try to get a cached value
     * @param key Cache key (typically action/value name)
     * @param state Current game state for validation
     * @param maxAgeMs Maximum age in milliseconds
     * @param outValue Output value if found
     * @return true if valid cached value found
     */
    template<typename T>
    bool TryGetCached(const std::string& key, const GameStateHash& state,
                       uint32 maxAgeMs, T& outValue)
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);

        auto it = m_cache.find(key);
        if (it == m_cache.end())
            return false;

        try
        {
            const CachedDecision<T>* cached = std::any_cast<CachedDecision<T>>(&it->second);
            if (!cached)
                return false;

            if (!cached->IsValid(state, maxAgeMs))
                return false;

            outValue = cached->value;
            return true;
        }
        catch (const std::bad_any_cast&)
        {
            return false;
        }
    }

    /**
     * Store a value in the cache
     */
    template<typename T>
    void SetCached(const std::string& key, const GameStateHash& state, const T& value)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[key] = CachedDecision<T>(value, state);
    }

    /**
     * Clear entire cache
     */
    void Clear()
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache.clear();
    }

    /**
     * Invalidate a specific cache entry
     */
    void Invalidate(const std::string& key)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache.erase(key);
    }

    /**
     * Invalidate entries matching a prefix
     */
    void InvalidatePrefix(const std::string& prefix)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);

        for (auto it = m_cache.begin(); it != m_cache.end();)
        {
            if (it->first.rfind(prefix, 0) == 0)
                it = m_cache.erase(it);
            else
                ++it;
        }
    }

    /**
     * Get number of cached entries
     */
    size_t Size() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_cache.size();
    }

    /**
     * Periodic maintenance - remove old entries
     */
    void Update(uint32 maxAgeMs = 5000)
    {
        uint32 now = getMSTime();
        if (getMSTimeDiff(m_lastPruneTime, now) < 1000)
            return;

        m_lastPruneTime = now;

        // Simple size-based pruning (cache entries don't store their own age generically)
        if (Size() > 100)
        {
            Clear();  // Simple strategy: clear if too large
        }
    }

private:
    std::unordered_map<std::string, std::any> m_cache;
    mutable std::shared_mutex m_mutex;
    uint32 m_lastPruneTime;
};

/**
 * LazyValue - Cached value with dirty flag for lazy recalculation
 * Use for expensive computations that should only run when dependencies change
 */
template<typename T>
class LazyValue
{
public:
    using Calculator = std::function<T()>;

    LazyValue() : m_isDirty(true), m_hasValue(false) {}

    explicit LazyValue(Calculator calc)
        : m_calculator(calc), m_isDirty(true), m_hasValue(false) {}

    /**
     * Get the value, recalculating if dirty
     */
    const T& Get()
    {
        if (m_isDirty && m_calculator)
        {
            m_value = m_calculator();
            m_isDirty = false;
            m_hasValue = true;
        }
        return m_value;
    }

    /**
     * Get without recalculating (may return stale data)
     */
    const T& GetCurrent() const
    {
        return m_value;
    }

    /**
     * Check if value needs recalculation
     */
    bool IsDirty() const { return m_isDirty; }

    /**
     * Check if a value has ever been calculated
     */
    bool HasValue() const { return m_hasValue; }

    /**
     * Mark value as needing recalculation
     */
    void MarkDirty() { m_isDirty = true; }

    /**
     * Force a specific value (marks as not dirty)
     */
    void Set(const T& value)
    {
        m_value = value;
        m_isDirty = false;
        m_hasValue = true;
    }

    /**
     * Update the calculator function
     */
    void SetCalculator(Calculator calc)
    {
        m_calculator = calc;
        m_isDirty = true;
    }

    /**
     * Reset to initial state
     */
    void Reset()
    {
        m_value = T();
        m_isDirty = true;
        m_hasValue = false;
    }

private:
    T m_value;
    Calculator m_calculator;
    bool m_isDirty;
    bool m_hasValue;
};

/**
 * DependencyTracker - Track dependencies between values for automatic invalidation
 */
class DependencyTracker
{
public:
    /**
     * Register that 'dependent' depends on 'dependency'
     */
    void AddDependency(const std::string& dependent, const std::string& dependency)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dependencies[dependency].push_back(dependent);
    }

    /**
     * Get all values that depend on the given value
     */
    std::vector<std::string> GetDependents(const std::string& dependency) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_dependencies.find(dependency);
        if (it == m_dependencies.end())
            return {};

        return it->second;
    }

    /**
     * Clear all dependencies for a value
     */
    void ClearDependencies(const std::string& dependent)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& [dep, dependents] : m_dependencies)
        {
            dependents.erase(
                std::remove(dependents.begin(), dependents.end(), dependent),
                dependents.end());
        }
    }

    /**
     * Clear all tracked dependencies
     */
    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dependencies.clear();
    }

private:
    // dependency -> list of dependents
    std::unordered_map<std::string, std::vector<std::string>> m_dependencies;
    mutable std::mutex m_mutex;
};

/**
 * Helper to compute GameStateHash from bot
 */
class Player;
class PlayerbotAI;

class GameStateHasher
{
public:
    static GameStateHash ComputeHash(PlayerbotAI* ai);
    static GameStateHash ComputeHash(Player* bot);
};

#endif
