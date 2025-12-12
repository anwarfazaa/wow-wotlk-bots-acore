# Enhanced Playerbots Features

This document describes the enhanced features added to this fork of [mod-playerbots](https://github.com/mod-playerbots/mod-playerbots).

## Table of Contents

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
.playerbot pathfind start <mapId>    - Start pathfinding a dungeon
.playerbot pathfind stop             - Stop current pathfinding
.playerbot pathfind status           - Show current state/progress
.playerbot pathfind promote <mapId>  - Promote waypoint candidates
.playerbot pathfind clear <mapId>    - Clear learned data for dungeon
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

## Database Tables

### playerbots_dungeon_waypoints

Stores predefined waypoints for dungeon navigation.

```sql
CREATE TABLE `playerbots_dungeon_waypoints` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `map_id` MEDIUMINT NOT NULL,
    `dungeon_name` VARCHAR(64),
    `waypoint_index` SMALLINT NOT NULL,
    `x` FLOAT NOT NULL,
    `y` FLOAT NOT NULL,
    `z` FLOAT NOT NULL,
    `orientation` FLOAT DEFAULT 0,
    `boss_id` INT DEFAULT 0,
    `trash_pack_id` SMALLINT DEFAULT 0,
    `requires_clear` TINYINT DEFAULT 0,
    `safe_radius` FLOAT DEFAULT 5.0,
    `wait_for_group` TINYINT DEFAULT 0,
    `is_safe_spot` TINYINT DEFAULT 0,
    `description` VARCHAR(255),
    INDEX `idx_map_waypoint` (`map_id`, `waypoint_index`)
);
```

### playerbots_dungeon_progress

Tracks group progress through dungeons.

```sql
CREATE TABLE `playerbots_dungeon_progress` (
    `group_id` INT NOT NULL,
    `map_id` MEDIUMINT NOT NULL,
    `current_waypoint` SMALLINT DEFAULT 0,
    `bosses_killed` VARCHAR(255) DEFAULT '',
    `trash_cleared` VARCHAR(255) DEFAULT '',
    `last_updated` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`group_id`, `map_id`)
);
```

### playerbots_pathfinding_iterations

Stores learning data from pathfinding bot runs.

```sql
CREATE TABLE `playerbots_pathfinding_iterations` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `map_id` MEDIUMINT NOT NULL,
    `bot_guid` BIGINT NOT NULL,
    `iteration` SMALLINT NOT NULL,
    `duration_ms` INT,
    `death_count` SMALLINT DEFAULT 0,
    `stuck_count` SMALLINT DEFAULT 0,
    `total_distance` FLOAT,
    `score` FLOAT,
    `path_json` MEDIUMTEXT,
    `bosses_killed` VARCHAR(255),
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX `idx_map_iteration` (`map_id`, `iteration`)
);
```

### playerbots_pathfinding_waypoint_candidates

Stores waypoint candidates generated by the pathfinding bot.

```sql
CREATE TABLE `playerbots_pathfinding_waypoint_candidates` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `map_id` MEDIUMINT NOT NULL,
    `waypoint_index` SMALLINT NOT NULL,
    `x` FLOAT NOT NULL,
    `y` FLOAT NOT NULL,
    `z` FLOAT NOT NULL,
    `orientation` FLOAT DEFAULT 0,
    `waypoint_type` TINYINT DEFAULT 0,
    `boss_entry` INT DEFAULT 0,
    `safe_radius` FLOAT DEFAULT 5.0,
    `confidence` FLOAT DEFAULT 0,
    `times_visited` INT DEFAULT 0,
    `promoted` TINYINT DEFAULT 0,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX `idx_map_waypoint` (`map_id`, `waypoint_index`)
);
```

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
