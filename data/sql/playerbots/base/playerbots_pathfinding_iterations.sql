-- --------------------------------------------------------
-- Pathfinding Bot Iterations Table
-- Stores learning data from autonomous dungeon exploration runs
-- --------------------------------------------------------

DROP TABLE IF EXISTS `playerbots_pathfinding_iterations`;
CREATE TABLE `playerbots_pathfinding_iterations` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `map_id` MEDIUMINT UNSIGNED NOT NULL COMMENT 'Dungeon map ID',
    `bot_guid` BIGINT UNSIGNED NOT NULL COMMENT 'Bot that performed this iteration',
    `iteration` SMALLINT UNSIGNED NOT NULL COMMENT 'Iteration number for this dungeon',
    `duration_ms` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total run duration in milliseconds',
    `death_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Number of deaths during run',
    `stuck_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Number of stuck events',
    `total_distance` FLOAT NOT NULL DEFAULT 0 COMMENT 'Total path distance traveled',
    `score` FLOAT NOT NULL DEFAULT 0 COMMENT 'Calculated efficiency score (higher = better)',
    `path_json` MEDIUMTEXT COMMENT 'JSON serialized path positions',
    `bosses_killed` VARCHAR(255) DEFAULT '' COMMENT 'Comma-separated boss entry IDs killed',
    `exploration_pct` FLOAT NOT NULL DEFAULT 0 COMMENT 'Percentage of dungeon explored',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    INDEX `idx_map_iteration` (`map_id`, `iteration`),
    INDEX `idx_bot_guid` (`bot_guid`),
    INDEX `idx_score` (`map_id`, `score` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Pathfinding bot learning iterations';

-- --------------------------------------------------------
-- Pathfinding Best Routes Table
-- Stores the best converged route for each dungeon
-- --------------------------------------------------------

DROP TABLE IF EXISTS `playerbots_pathfinding_best_routes`;
CREATE TABLE `playerbots_pathfinding_best_routes` (
    `map_id` MEDIUMINT UNSIGNED NOT NULL COMMENT 'Dungeon map ID',
    `dungeon_name` VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'Human-readable dungeon name',
    `best_iteration_id` INT UNSIGNED NOT NULL COMMENT 'Reference to best iteration',
    `total_iterations` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total iterations run',
    `converged` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '1 if route has converged',
    `best_score` FLOAT NOT NULL DEFAULT 0 COMMENT 'Best achieved score',
    `avg_duration_ms` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Average run duration',
    `path_json` MEDIUMTEXT COMMENT 'Best path JSON (denormalized for quick access)',
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Best converged routes per dungeon';
