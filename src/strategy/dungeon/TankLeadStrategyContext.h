/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _PLAYERBOT_TANKLEADSTRATEGYCONTEXT_H
#define _PLAYERBOT_TANKLEADSTRATEGYCONTEXT_H

#include "NamedObjectContext.h"
#include "TankLeadStrategy.h"

class TankLeadStrategyContext : public NamedObjectContext<Strategy>
{
public:
    TankLeadStrategyContext() : NamedObjectContext<Strategy>(false, true)
    {
        creators["tank lead"] = &TankLeadStrategyContext::tank_lead;
        creators["tank lead nc"] = &TankLeadStrategyContext::tank_lead_nc;
        creators["dungeon progress"] = &TankLeadStrategyContext::dungeon_progress;
    }

private:
    static Strategy* tank_lead(PlayerbotAI* ai) { return new TankLeadStrategy(ai); }
    static Strategy* tank_lead_nc(PlayerbotAI* ai) { return new TankLeadNonCombatStrategy(ai); }
    static Strategy* dungeon_progress(PlayerbotAI* ai) { return new DungeonProgressStrategy(ai); }
};

#endif
