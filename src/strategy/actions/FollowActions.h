/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_FOLLOWACTIONS_H
#define _PLAYERBOT_FOLLOWACTIONS_H

#include "MovementActions.h"

class PlayerbotAI;

class FollowAction : public MovementAction
{
public:
    FollowAction(PlayerbotAI* botAI, std::string const name = "follow") : MovementAction(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool CanDeadFollow(Unit* target);
};

class FleeToGroupLeaderAction : public FollowAction
{
public:
    FleeToGroupLeaderAction(PlayerbotAI* botAI) : FollowAction(botAI, "flee to group leader") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

/**
 * ResumeFollowAfterTeleportAction - Restores follow strategy after teleport
 *
 * This action:
 * 1. Removes the 'stay' strategy that was added during teleport
 * 2. Ensures the 'follow' strategy is active
 * 3. Clears any stale movement state
 */
class ResumeFollowAfterTeleportAction : public Action
{
public:
    ResumeFollowAfterTeleportAction(PlayerbotAI* botAI) : Action(botAI, "resume follow after teleport") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
