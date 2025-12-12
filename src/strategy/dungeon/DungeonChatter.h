/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_DUNGEONCHATTER_H
#define _PLAYERBOT_DUNGEONCHATTER_H

#include "Common.h"
#include <string>
#include <vector>
#include <unordered_map>

class Player;

/**
 * Chatter categories for different dungeon situations
 */
enum class ChatterCategory : uint8
{
    ENTERING_DUNGEON = 0,   // When first entering
    BEFORE_PULL,            // Before pulling trash/boss
    AFTER_KILL,             // After killing mobs
    BOSS_PULL,              // Before boss fight
    BOSS_KILL,              // After killing a boss
    WIPE,                   // After a wipe
    LOW_HEALTH,             // When health gets low
    LOW_MANA,               // Healer/caster low mana
    WAITING,                // While waiting for group
    RANDOM,                 // Random chatter during downtime
    LOOT,                   // When looting
    STUCK,                  // When getting stuck
    DEATH,                  // When dying
    RESURRECT,              // When resurrected
    MAX_CATEGORY
};

/**
 * DungeonChatter - Manages funny bot dialogue during dungeons
 *
 * Bots will occasionally say humorous lines to make dungeons more entertaining.
 * Lines are categorized by situation and selected randomly with cooldowns to prevent spam.
 */
class DungeonChatter
{
public:
    static DungeonChatter* instance();

    // Initialize the chatter database
    void Initialize();

    // Get a random chatter line for the situation
    std::string GetChatter(ChatterCategory category, Player* bot);

    // Check if bot should say something (based on cooldown and chance)
    bool ShouldChatter(Player* bot, ChatterCategory category);

    // Record that bot just chatted
    void RecordChatter(Player* bot);

    // Configuration
    void SetChatterChance(uint8 chance) { m_chatterChance = chance; }
    void SetMinCooldownMs(uint32 ms) { m_minCooldownMs = ms; }
    void SetMaxCooldownMs(uint32 ms) { m_maxCooldownMs = ms; }

private:
    DungeonChatter();
    ~DungeonChatter() = default;

    void LoadChatterLines();

    // Chatter lines by category
    std::unordered_map<uint8, std::vector<std::string>> m_chatterLines;

    // Last chatter time per bot
    std::unordered_map<uint64, uint32> m_lastChatterTime;

    // Configuration
    uint8 m_chatterChance = 15;          // 15% chance to chatter
    uint32 m_minCooldownMs = 30000;      // 30 seconds minimum between chatters
    uint32 m_maxCooldownMs = 120000;     // 2 minutes maximum cooldown

    bool m_initialized = false;
};

#define sDungeonChatter DungeonChatter::instance()

#endif  // _PLAYERBOT_DUNGEONCHATTER_H
