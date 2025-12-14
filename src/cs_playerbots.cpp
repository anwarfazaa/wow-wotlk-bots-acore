/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BattleGroundTactics.h"
#include "Chat.h"
#include "GuildTaskMgr.h"
#include "PerformanceMonitor.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"
#include "PathfindingBotManager.h"

using namespace Acore::ChatCommands;

class playerbots_commandscript : public CommandScript
{
public:
    playerbots_commandscript() : CommandScript("playerbots_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable playerbotsDebugCommandTable = {
            {"bg", HandleDebugBGCommand, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable playerbotsPathfindCommandTable = {
            {"start", HandlePathfindStartCommand, SEC_GAMEMASTER, Console::No},
            {"stop", HandlePathfindStopCommand, SEC_GAMEMASTER, Console::No},
            {"status", HandlePathfindStatusCommand, SEC_GAMEMASTER, Console::No},
            {"promote", HandlePathfindPromoteCommand, SEC_GAMEMASTER, Console::No},
            {"clear", HandlePathfindClearCommand, SEC_GAMEMASTER, Console::No},
        };

        static ChatCommandTable playerbotsAccountCommandTable = {
            {"setKey", HandleSetSecurityKeyCommand, SEC_PLAYER, Console::No},
            {"link", HandleLinkAccountCommand, SEC_PLAYER, Console::No},
            {"linkedAccounts", HandleViewLinkedAccountsCommand, SEC_PLAYER, Console::No},
            {"unlink", HandleUnlinkAccountCommand, SEC_PLAYER, Console::No},
        };

        static ChatCommandTable playerbotsCommandTable = {
            {"bot", HandlePlayerbotCommand, SEC_PLAYER, Console::No},
            {"gtask", HandleGuildTaskCommand, SEC_GAMEMASTER, Console::Yes},
            {"pmon", HandlePerfMonCommand, SEC_GAMEMASTER, Console::Yes},
            {"rndbot", HandleRandomPlayerbotCommand, SEC_GAMEMASTER, Console::Yes},
            {"debug", playerbotsDebugCommandTable},
            {"account", playerbotsAccountCommandTable},
            {"pathfind", playerbotsPathfindCommandTable},
        };

        static ChatCommandTable commandTable = {
            {"playerbots", playerbotsCommandTable},
        };

        return commandTable;
    }

    static bool HandlePlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return PlayerbotMgr::HandlePlayerbotMgrCommand(handler, args);
    }

    static bool HandleRandomPlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return RandomPlayerbotMgr::HandlePlayerbotConsoleCommand(handler, args);
    }

    static bool HandleGuildTaskCommand(ChatHandler* handler, char const* args)
    {
        return GuildTaskMgr::HandleConsoleCommand(handler, args);
    }

    static bool HandlePerfMonCommand(ChatHandler* handler, char const* args)
    {
        if (!strcmp(args, "reset"))
        {
            sPerformanceMonitor->Reset();
            return true;
        }

        if (!strcmp(args, "tick"))
        {
            sPerformanceMonitor->PrintStats(true, false);
            return true;
        }

        if (!strcmp(args, "stack"))
        {
            sPerformanceMonitor->PrintStats(false, true);
            return true;
        }

        if (!strcmp(args, "toggle"))
        {
            sPlayerbotAIConfig->perfMonEnabled = !sPlayerbotAIConfig->perfMonEnabled;
            if (sPlayerbotAIConfig->perfMonEnabled)
                LOG_INFO("playerbots", "Performance monitor enabled");
            else
                LOG_INFO("playerbots", "Performance monitor disabled");
            return true;
        }

        sPerformanceMonitor->PrintStats();
        return true;
    }

    static bool HandleDebugBGCommand(ChatHandler* handler, char const* args)
    {
        return BGTactics::HandleConsoleCommand(handler, args);
    }

    static bool HandleSetSecurityKeyCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->PSendSysMessage("Usage: .playerbots account setKey <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        std::string key = args;

        PlayerbotMgr* mgr = sPlayerbotsMgr->GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleSetSecurityKeyCommand(player, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleLinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        char* key = strtok(nullptr, " ");

        if (!accountName || !key)
        {
            handler->PSendSysMessage("Usage: .playerbots account link <accountName> <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = sPlayerbotsMgr->GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleLinkAccountCommand(player, accountName, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleViewLinkedAccountsCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = sPlayerbotsMgr->GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleViewLinkedAccountsCommand(player);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleUnlinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        if (!accountName)
        {
            handler->PSendSysMessage("Usage: .playerbots account unlink <accountName>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = sPlayerbotsMgr->GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleUnlinkAccountCommand(player, accountName);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    // Pathfinding Bot GM Commands
    static bool HandlePathfindStartCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!args || !*args)
        {
            handler->PSendSysMessage("Usage: .playerbots pathfind start <mapId>");
            handler->PSendSysMessage("Example: .playerbots pathfind start 574 (Utgarde Keep)");
            return false;
        }

        uint32 mapId = atoi(args);
        if (mapId == 0)
        {
            handler->PSendSysMessage("Invalid map ID.");
            return false;
        }

        if (sPathfindingBot->StartPathfinding(player, mapId))
        {
            handler->PSendSysMessage("Pathfinding started for map %u (%s).",
                mapId, sPathfindingBot->GetDungeonName(mapId).c_str());
            return true;
        }
        else
        {
            handler->PSendSysMessage("Failed to start pathfinding. Make sure you're using a bot character.");
            return false;
        }
    }

    static bool HandlePathfindStopCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (sPathfindingBot->IsActive(player))
        {
            sPathfindingBot->StopPathfinding(player);
            handler->PSendSysMessage("Pathfinding stopped.");
            return true;
        }
        else
        {
            handler->PSendSysMessage("Pathfinding is not active for this character.");
            return false;
        }
    }

    static bool HandlePathfindStatusCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sPathfindingBot->IsActive(player))
        {
            handler->PSendSysMessage("Pathfinding is not active for this character.");
            return false;
        }

        std::string status = sPathfindingBot->GetStatusString(player);
        handler->PSendSysMessage("Pathfinding Status:");
        handler->PSendSysMessage("%s", status.c_str());
        handler->PSendSysMessage("Iteration: %u, Exploration: %.1f%%",
            sPathfindingBot->GetCurrentIteration(player),
            sPathfindingBot->GetExplorationPercent(player));

        if (sPathfindingBot->IsConverged(player))
            handler->PSendSysMessage("Status: CONVERGED - routes are stable.");

        return true;
    }

    static bool HandlePathfindPromoteCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->PSendSysMessage("Usage: .playerbots pathfind promote <mapId>");
            return false;
        }

        uint32 mapId = atoi(args);
        if (mapId == 0)
        {
            handler->PSendSysMessage("Invalid map ID.");
            return false;
        }

        sPathfindingBot->PromoteWaypointCandidates(mapId);
        handler->PSendSysMessage("Waypoint candidates promoted to main table for map %u.", mapId);
        return true;
    }

    static bool HandlePathfindClearCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->PSendSysMessage("Usage: .playerbots pathfind clear <mapId>");
            return false;
        }

        uint32 mapId = atoi(args);
        if (mapId == 0)
        {
            handler->PSendSysMessage("Invalid map ID.");
            return false;
        }

        sPathfindingBot->ClearLearnedData(mapId);
        handler->PSendSysMessage("Learned pathfinding data cleared for map %u.", mapId);
        return true;
    }
};

void AddSC_playerbots_commandscript() { new playerbots_commandscript(); }
