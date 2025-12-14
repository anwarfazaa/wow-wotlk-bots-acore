# Enhanced Playerbots Features

This document describes the enhanced features added to this fork of [mod-playerbots](https://github.com/mod-playerbots/mod-playerbots).

## Table of Contents

- [Quick Start](#quick-start)
- [Overview](#overview)
- [New Systems](#new-systems)
  - [Dungeon Navigator](#dungeon-navigator)
  - [Tank Lead Strategy](#tank-lead-strategy)
  - [Dungeon Chatter](#dungeon-chatter)
  - [Pathfinding Bot](#pathfinding-bot)
  - [Follow After Teleport Fix](#follow-after-teleport-fix)
- [Configuration](#configuration)
- [Database Tables](#database-tables)
- [Credits](#credits)

---

## Quick Start

### 1. Run Database Setup

After compiling, run these SQL files against your `acore_playerbots` database:

```bash
cd <azerothcore>/modules/mod-playerbots/data/sql/playerbots/base/
mysql -u root -p acore_playerbots < playerbots_dungeon_waypoints.sql
mysql -u root -p acore_playerbots < playerbots_boss_abilities.sql
mysql -u root -p acore_playerbots < playerbots_pathfinding_iterations.sql
mysql -u root -p acore_playerbots < playerbots_pathfinding_waypoint_candidates.sql
```

### 2. Summon Bots Into Dungeon

If bots are outside the dungeon (e.g., after LFG queue), use the `summon` command to bring them inside:

```
/w BotName summon
```

Or summon all bots at once (party chat):
```
/p summon
```

### 3. Enable Tank Lead (In-Game Commands)

Whisper these commands to your tank bot to enable waypoint-based dungeon navigation:

```
/w TankBot co +tank lead
/w TankBot nc +tank lead nc
/w TankBot nc +dungeon progress
```

For DPS/healer bots, enable progress tracking:
```
/w HealerBot nc +dungeon progress
```

### 4. Supported Dungeons

The following dungeons have pre-defined waypoints (more can be added via the Pathfinding Bot system):

| Map ID | Dungeon Name |
|--------|--------------|
| 574 | Utgarde Keep |
| 576 | The Nexus |
| 602 | Halls of Lightning |

---

## Overview

This fork extends the original mod-playerbots with several new systems designed to improve bot behavior in dungeons and group content:

- **Intelligent dungeon navigation** using waypoint-based pathfinding
- **Tank-led dungeon runs** where tank bots lead the group through dungeons
- **Humorous dungeon chatter** for a more immersive experience
- **Autonomous pathfinding bot** for learning dungeon routes
- **Improved follow behavior** after teleporting to dungeons

---

## New Systems

### Dungeon Navigator

The Dungeon Navigator system provides waypoint-based navigation for bots in dungeons.

**Features:**
- Pre-defined waypoints for dungeon paths
- Boss and trash pack location tracking
- Safe spot identification for resting
- Progress tracking per group
- Automatic path following

**Files:**
- `src/strategy/dungeon/DungeonNavigator.h`
- `src/strategy/dungeon/DungeonNavigator.cpp`

---

### Tank Lead Strategy

Allows tank bots to lead groups through dungeons autonomously.

**Features:**
- Tank bots navigate using dungeon waypoints
- Wait for group members before proceeding
- Announce pulls and boss encounters
- Mana break detection for healers
- Human player handoff when waypoints aren't available

**Triggers:**
| Trigger | Description |
|---------|-------------|
| `tank lead enabled` | Check if tank lead strategy should be active |
| `at dungeon waypoint` | Tank is at current waypoint |
| `should move to next waypoint` | Tank should proceed to next waypoint |
| `group not ready` | Group members not ready to proceed |
| `healer needs mana break` | Healer mana below threshold |
| `boss ahead` | Boss encounter at next waypoint |
| `trash pack ahead` | Trash pack detected ahead |
| `no dungeon path` | No waypoints available for current dungeon |

**Actions:**
| Action | Description |
|--------|-------------|
| `move to next waypoint` | Navigate to next dungeon waypoint |
| `wait for group` | Wait for group members to catch up |
| `announce pull` | Announce upcoming pull |
| `announce boss` | Announce boss encounter |
| `request human lead` | Ask human player to lead (no waypoints) |
| `follow human leader` | Follow human player through dungeon |

**Player Commands (whisper to bot or say in party):**
```
co +tank lead        - Enable tank lead combat strategy (tank navigates dungeon)
co -tank lead        - Disable tank lead combat strategy
nc +tank lead nc     - Enable tank lead non-combat strategy
nc -tank lead nc     - Disable tank lead non-combat strategy
nc +dungeon progress - Enable dungeon progress tracking (for all roles)
nc -dungeon progress - Disable dungeon progress tracking
```

**Example Usage:**
```
# Enable full tank lead behavior on your tank bot
/w TankBot co +tank lead
/w TankBot nc +tank lead nc
/w TankBot nc +dungeon progress

# Enable progress tracking for DPS/healer bots
/w HealerBot nc +dungeon progress
/w DPSBot nc +dungeon progress
```

**Files:**
- `src/strategy/dungeon/TankLeadStrategy.h`
- `src/strategy/dungeon/TankLeadStrategy.cpp`
- `src/strategy/dungeon/TankLeadActions.h`
- `src/strategy/dungeon/TankLeadActions.cpp`
- `src/strategy/dungeon/TankLeadTriggers.h`
- `src/strategy/dungeon/TankLeadTriggers.cpp`
- `src/strategy/dungeon/TankLeadActionContext.h`
- `src/strategy/dungeon/TankLeadTriggerContext.h`

---

### Dungeon Chatter

Adds humorous dialogue for bots during dungeon runs, making the experience more immersive and entertaining.

**Chatter Categories:**
| Category | When Triggered |
|----------|----------------|
| `ENTERING_DUNGEON` | When first entering a dungeon |
| `BEFORE_PULL` | Before pulling trash/boss |
| `AFTER_KILL` | After killing mobs |
| `BOSS_PULL` | Before boss fights |
| `BOSS_KILL` | After killing a boss |
| `WIPE` | After a wipe |
| `LOW_HEALTH` | When health gets low |
| `LOW_MANA` | When healer/caster mana is low |
| `WAITING` | While waiting for group |
| `RANDOM` | Random chatter during downtime |
| `LOOT` | When looting |
| `STUCK` | When getting stuck |
| `DEATH` | When dying |
| `RESURRECT` | When resurrected |

**Configuration:**
- Default chance: 15%
- Minimum cooldown: 30 seconds
- Maximum cooldown: 2 minutes

**Files:**
- `src/strategy/dungeon/DungeonChatter.h`
- `src/strategy/dungeon/DungeonChatter.cpp`

---

### Pathfinding Bot

An autonomous bot system that explores dungeons solo, learns optimal routes through iterative runs, and generates waypoint data for the Dungeon Navigator.

**Features:**
- Frontier-based exploration algorithm
- Multiple iteration learning with convergence detection
- Stuck recovery with jump mechanics
- Automatic waypoint generation from learned paths
- Boss and trash pack location recording

**States:**
```
IDLE -> ENTERING -> EXPLORING -> COMBAT -> BOSS_ENCOUNTER
                       |            |
                       v            v
               STUCK_RECOVERY  ANALYZING -> COMPLETE
                                   |
                                   v
                              RESETTING (next iteration)
```

**GM Commands:**
```
.playerbots pathfind start <mapId>    - Start pathfinding a dungeon
.playerbots pathfind stop             - Stop current pathfinding
.playerbots pathfind status           - Show current state/progress
.playerbots pathfind promote <mapId>  - Promote waypoint candidates to main table
.playerbots pathfind clear <mapId>    - Clear learned data for dungeon
```

**Example:**
```
.playerbots pathfind start 574    (Start exploring Utgarde Keep)
.playerbots pathfind status       (Check progress)
.playerbots pathfind promote 574  (Promote learned routes to waypoint table)
```

**Files:**
- `src/strategy/pathfinding/PathfindingBotManager.h`
- `src/strategy/pathfinding/PathfindingBotManager.cpp`
- `src/strategy/pathfinding/ExplorationEngine.h`
- `src/strategy/pathfinding/ExplorationEngine.cpp`
- `src/strategy/pathfinding/PathLearner.h`
- `src/strategy/pathfinding/PathLearner.cpp`
- `src/strategy/pathfinding/StuckRecoverySystem.h`
- `src/strategy/pathfinding/StuckRecoverySystem.cpp`
- `src/strategy/pathfinding/WaypointGenerator.h`
- `src/strategy/pathfinding/WaypointGenerator.cpp`

---

### Follow After Teleport Fix

Fixes an issue where bots would stop following their leader after teleporting to dungeons.

**Problem:**
The original `TeleportAction` set `-follow,+stay` when teleporting via game objects, permanently disabling follow behavior.

**Solution:**
- Removed the problematic strategy change from `TeleportAction`
- Added `ResumeFollowAfterTeleportTrigger` to detect when bots should resume following
- Added `ResumeFollowAfterTeleportAction` to restore follow behavior
- Clears stale movement state to prevent priority conflicts

**Files Modified:**
- `src/strategy/actions/TeleportAction.cpp`
- `src/strategy/actions/FollowActions.h`
- `src/strategy/actions/FollowActions.cpp`
- `src/strategy/triggers/GenericTriggers.h`
- `src/strategy/triggers/GenericTriggers.cpp`
- `src/strategy/generic/NonCombatStrategy.cpp`

---

## Configuration

Add the following options to your `playerbots.conf` file:

```ini
###################################
# DUNGEON NAVIGATION SETTINGS     #
###################################

# Enable dungeon navigator system
# Default: 1
AiPlayerbot.DungeonNavigator.Enabled = 1

###################################
# TANK LEAD SETTINGS              #
###################################

# Enable tank lead strategy for dungeons
# Default: 1
AiPlayerbot.TankLead.Enabled = 1

# Distance at which tank waits for group members (yards)
# Default: 40
AiPlayerbot.TankLead.WaitForGroupDistance = 40

# Mana percentage threshold for healer mana breaks
# Default: 30
AiPlayerbot.TankLead.ManaBreakThreshold = 30

###################################
# PATHFINDING BOT SETTINGS        #
###################################

# Enable pathfinding bot system
# Default: 1
AiPlayerbot.PathfindingBot.Enabled = 1

# Maximum iterations for route learning
# Default: 10
AiPlayerbot.PathfindingBot.MaxIterations = 10

# Time in ms before considering bot stuck
# Default: 10000
AiPlayerbot.PathfindingBot.StuckThresholdMs = 10000

# Iterations without improvement before convergence
# Default: 3
AiPlayerbot.PathfindingBot.ConvergenceIterations = 3

# Score improvement threshold for convergence (0.02 = 2%)
# Default: 0.02
AiPlayerbot.PathfindingBot.ConvergenceThreshold = 0.02

# Automatically promote learned waypoints to main table
# Default: 0
AiPlayerbot.PathfindingBot.AutoPromoteWaypoints = 0

# Minimum confidence score for waypoint promotion (0-1)
# Default: 0.8
AiPlayerbot.PathfindingBot.MinConfidenceForPromotion = 0.8
```

---

## Database Setup

### Required SQL Files

After compiling, you **must** run these SQL files to create the required database tables.
Run them against your `acore_playerbots` database:

```bash
# Navigate to the module's SQL directory
cd ~/exp/azerothcore/modules/mod-playerbots/data/sql/playerbots/base/

# Run all required SQL files
mysql -u root -p acore_playerbots < playerbots_dungeon_waypoints.sql
mysql -u root -p acore_playerbots < playerbots_boss_abilities.sql
mysql -u root -p acore_playerbots < playerbots_pathfinding_iterations.sql
mysql -u root -p acore_playerbots < playerbots_pathfinding_waypoint_candidates.sql
```

Or run them all at once:
```bash
cd ~/exp/azerothcore/modules/mod-playerbots/data/sql/playerbots/base/
for f in playerbots_dungeon_waypoints.sql playerbots_boss_abilities.sql playerbots_pathfinding_iterations.sql playerbots_pathfinding_waypoint_candidates.sql; do
  mysql -u root -p acore_playerbots < "$f"
done
```

### Tables Created

| Table | Description |
|-------|-------------|
| `playerbots_dungeon_waypoints` | Predefined waypoints for dungeon navigation (includes sample data for UK, Nexus, HoL) |
| `playerbots_boss_abilities` | Boss ability data for anticipatory threat system |
| `playerbots_pathfinding_iterations` | Learning data from pathfinding bot runs |
| `playerbots_pathfinding_waypoint_candidates` | Waypoint candidates generated by pathfinding bot |
| `playerbots_pathfinding_best_routes` | Best routes discovered for each dungeon |
| `playerbots_pathfinding_stuck_locations` | Known stuck locations to avoid |

---

## Credits

This fork is based on the original [mod-playerbots](https://github.com/mod-playerbots/mod-playerbots) project.

**Original Project:**
- Repository: https://github.com/mod-playerbots/mod-playerbots
- Based on [IKE3's Playerbots](https://github.com/ike3/mangosbot)
- Special thanks to [@ZhengPeiRu21](https://github.com/ZhengPeiRu21) and [@celguar](https://github.com/celguar)

**Enhanced Features by:**
- Dungeon Navigator System
- Tank Lead Strategy
- Dungeon Chatter System
- Pathfinding Bot System
- Follow After Teleport Fix

For the original installation instructions and documentation, please refer to the [main README](README.md) and the [Playerbots Wiki](https://github.com/mod-playerbots/mod-playerbots/wiki).
