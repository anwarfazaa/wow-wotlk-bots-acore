/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_TANKLEADSTRATEGY_H
#define _PLAYERBOT_TANKLEADSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

/**
 * TankLeadStrategy - Enables tank to lead group through dungeons
 *
 * This strategy allows the tank bot to:
 * - Navigate through dungeon waypoints
 * - Wait for group at appropriate points
 * - Announce pulls and boss encounters
 * - Track dungeon progress
 * - Coordinate with group ready states
 */
class TankLeadStrategy : public Strategy
{
public:
    TankLeadStrategy(PlayerbotAI* ai);

    std::string getName() override { return "tank lead"; }
    uint32 GetType() const override { return STRATEGY_TYPE_TANK; }

protected:
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

/**
 * TankLeadNonCombatStrategy - Non-combat tank lead behaviors
 */
class TankLeadNonCombatStrategy : public Strategy
{
public:
    TankLeadNonCombatStrategy(PlayerbotAI* ai);

    std::string getName() override { return "tank lead nc"; }
    uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }

protected:
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

/**
 * DungeonProgressStrategy - Track dungeon progress for all roles
 */
class DungeonProgressStrategy : public Strategy
{
public:
    DungeonProgressStrategy(PlayerbotAI* ai);

    std::string getName() override { return "dungeon progress"; }
    uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }

protected:
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
