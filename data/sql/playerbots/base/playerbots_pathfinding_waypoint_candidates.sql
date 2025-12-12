-- --------------------------------------------------------
-- Pathfinding Waypoint Candidates Table
-- Stores learned waypoint positions before promotion to main waypoints table
-- --------------------------------------------------------

DROP TABLE IF EXISTS `playerbots_pathfinding_waypoint_candidates`;
CREATE TABLE `playerbots_pathfinding_waypoint_candidates` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `map_id` MEDIUMINT UNSIGNED NOT NULL COMMENT 'Dungeon map ID',
    `waypoint_index` SMALLINT UNSIGNED NOT NULL COMMENT 'Order in optimized path',
    `x` FLOAT NOT NULL DEFAULT 0,
    `y` FLOAT NOT NULL DEFAULT 0,
    `z` FLOAT NOT NULL DEFAULT 0,
    `orientation` FLOAT NOT NULL DEFAULT 0 COMMENT 'Facing direction',
    `waypoint_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=PATH, 1=BOSS, 2=TRASH_PACK, 3=SAFE_SPOT, 4=EVENT',
    `boss_entry` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boss NPC entry if type=BOSS',
    `trash_pack_id` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Trash pack ID if type=TRASH_PACK',
    `safe_radius` FLOAT NOT NULL DEFAULT 5.0 COMMENT 'Safe standing radius',
    `confidence` FLOAT NOT NULL DEFAULT 0 COMMENT '0-1 confidence based on iteration consistency',
    `times_visited` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'How many iterations visited this position',
    `avg_combat_duration_ms` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Average time spent in combat here',
    `promoted` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '1 if promoted to main waypoints table',
    `promoted_waypoint_id` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ID in main waypoints table if promoted',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    INDEX `idx_map_waypoint` (`map_id`, `waypoint_index`),
    INDEX `idx_confidence` (`map_id`, `confidence` DESC),
    INDEX `idx_promoted` (`promoted`),
    UNIQUE KEY `uk_map_position` (`map_id`, `waypoint_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Learned waypoint candidates from pathfinding';

-- --------------------------------------------------------
-- Pathfinding Stuck Locations Table
-- Records positions where bots got stuck to avoid in future
-- --------------------------------------------------------

DROP TABLE IF EXISTS `playerbots_pathfinding_stuck_locations`;
CREATE TABLE `playerbots_pathfinding_stuck_locations` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `map_id` MEDIUMINT UNSIGNED NOT NULL COMMENT 'Dungeon map ID',
    `x` FLOAT NOT NULL DEFAULT 0,
    `y` FLOAT NOT NULL DEFAULT 0,
    `z` FLOAT NOT NULL DEFAULT 0,
    `stuck_count` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Number of times bots got stuck here',
    `recovery_success_count` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Successful recoveries',
    `recovery_method` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Most successful recovery method',
    `avoid_radius` FLOAT NOT NULL DEFAULT 3.0 COMMENT 'Radius to avoid around this position',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    INDEX `idx_map` (`map_id`),
    INDEX `idx_position` (`map_id`, `x`, `y`, `z`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Known stuck locations to avoid';
