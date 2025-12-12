/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "TankLeadStrategy.h"
#include "TankLeadActions.h"
#include "TankLeadTriggers.h"
#include "Action.h"

// ============================================================================
// TankLeadStrategy
// ============================================================================

TankLeadStrategy::TankLeadStrategy(PlayerbotAI* ai) : Strategy(ai)
{
}

void TankLeadStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // When no dungeon path exists, ask human to lead and follow them
    triggers.push_back(new TriggerNode(
        "no dungeon path",
        NextAction::array(0,
            new NextAction("request human lead", ACTION_MOVE + 2),
            new NextAction("follow human leader", ACTION_MOVE + 1),
            nullptr)));

    // Move to next waypoint when ready
    triggers.push_back(new TriggerNode(
        "should move to next waypoint",
        NextAction::array(0,
            new NextAction("announce movement", ACTION_MOVE + 5),
            new NextAction("move to next waypoint", ACTION_MOVE + 4),
            nullptr)));

    // Wait for group when needed
    triggers.push_back(new TriggerNode(
        "wait for group",
        NextAction::array(0,
            new NextAction("wait for group", ACTION_MOVE + 6),
            nullptr)));

    // Wait for mana break
    triggers.push_back(new TriggerNode(
        "healer needs mana break",
        NextAction::array(0,
            new NextAction("wait for mana break", ACTION_MOVE + 7),
            nullptr)));

    // Handle trash ahead
    triggers.push_back(new TriggerNode(
        "trash pack ahead",
        NextAction::array(0,
            new NextAction("announce pull", ACTION_MOVE + 3),
            nullptr)));

    // Handle boss ahead
    triggers.push_back(new TriggerNode(
        "boss ahead",
        NextAction::array(0,
            new NextAction("announce boss", ACTION_MOVE + 8),
            nullptr)));

    // Group member far behind - wait
    triggers.push_back(new TriggerNode(
        "group member far behind",
        NextAction::array(0,
            new NextAction("wait for group", ACTION_MOVE + 9),
            nullptr)));

    // Group member dead - wait
    triggers.push_back(new TriggerNode(
        "group member dead dungeon",
        NextAction::array(0,
            new NextAction("wait for group", ACTION_MOVE + 10),
            nullptr)));

    // Pull ready at waypoint
    triggers.push_back(new TriggerNode(
        "pull ready",
        NextAction::array(0,
            new NextAction("pull trash", ACTION_RAID + 1),
            nullptr)));
}

void TankLeadStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // No special multipliers needed
}

// ============================================================================
// TankLeadNonCombatStrategy
// ============================================================================

TankLeadNonCombatStrategy::TankLeadNonCombatStrategy(PlayerbotAI* ai) : Strategy(ai)
{
}

void TankLeadNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Initialize progress on dungeon entry
    triggers.push_back(new TriggerNode(
        "tank lead enabled",
        NextAction::array(0,
            new NextAction("initialize dungeon progress", ACTION_NORMAL + 5),
            nullptr)));

    // Update progress periodically
    triggers.push_back(new TriggerNode(
        "at dungeon waypoint",
        NextAction::array(0,
            new NextAction("update dungeon progress", ACTION_NORMAL + 1),
            nullptr)));

    // Check group ready state
    triggers.push_back(new TriggerNode(
        "group not ready",
        NextAction::array(0,
            new NextAction("check group ready", ACTION_NORMAL + 2),
            nullptr)));

    // Mark clears after combat
    triggers.push_back(new TriggerNode(
        "waypoint requires clear",
        NextAction::array(0,
            new NextAction("mark trash cleared", ACTION_NORMAL + 3),
            new NextAction("mark boss killed", ACTION_NORMAL + 3),
            nullptr)));

    // Safe spot reached
    triggers.push_back(new TriggerNode(
        "safe spot reached",
        NextAction::array(0,
            new NextAction("sync group progress", ACTION_NORMAL + 1),
            nullptr)));

    // Dungeon complete
    triggers.push_back(new TriggerNode(
        "dungeon complete",
        NextAction::array(0, nullptr)));  // Could add celebration action
}

// ============================================================================
// DungeonProgressStrategy
// ============================================================================

DungeonProgressStrategy::DungeonProgressStrategy(PlayerbotAI* ai) : Strategy(ai)
{
}

void DungeonProgressStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // All roles should track ready state
    triggers.push_back(new TriggerNode(
        "group not ready",
        NextAction::array(0,
            new NextAction("check group ready", ACTION_NORMAL + 1),
            nullptr)));

    // Sync progress for followers
    triggers.push_back(new TriggerNode(
        "at dungeon waypoint",
        NextAction::array(0,
            new NextAction("sync group progress", ACTION_NORMAL),
            nullptr)));

    // =========================================================================
    // Dungeon Chatter - Bots say funny lines during dungeons
    // =========================================================================

    // Random chatter during downtime
    triggers.push_back(new TriggerNode(
        "dungeon chatter",
        NextAction::array(0,
            new NextAction("dungeon chatter", ACTION_IDLE),
            nullptr)));

    // Chatter when entering a dungeon
    triggers.push_back(new TriggerNode(
        "dungeon enter chatter",
        NextAction::array(0,
            new NextAction("dungeon enter chatter", ACTION_IDLE),
            nullptr)));

    // Chatter after combat ends
    triggers.push_back(new TriggerNode(
        "after combat chatter",
        NextAction::array(0,
            new NextAction("after combat chatter", ACTION_IDLE),
            nullptr)));

    // Chatter when health is low
    triggers.push_back(new TriggerNode(
        "low health chatter",
        NextAction::array(0,
            new NextAction("low health chatter", ACTION_IDLE),
            nullptr)));

    // Chatter when mana is low
    triggers.push_back(new TriggerNode(
        "low mana chatter",
        NextAction::array(0,
            new NextAction("low mana chatter", ACTION_IDLE),
            nullptr)));

    // Chatter when dying
    triggers.push_back(new TriggerNode(
        "death chatter",
        NextAction::array(0,
            new NextAction("death chatter", ACTION_IDLE),
            nullptr)));

    // Chatter when resurrected
    triggers.push_back(new TriggerNode(
        "resurrect chatter",
        NextAction::array(0,
            new NextAction("resurrect chatter", ACTION_IDLE),
            nullptr)));
}
