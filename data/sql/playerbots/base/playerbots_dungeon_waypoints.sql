DROP TABLE IF EXISTS `playerbots_dungeon_waypoints`;
CREATE TABLE `playerbots_dungeon_waypoints` (
    `id`              INT          NOT NULL AUTO_INCREMENT,
    `map_id`          MEDIUMINT(8) UNSIGNED NOT NULL COMMENT 'Dungeon map ID',
    `dungeon_name`    VARCHAR(64)  NOT NULL COMMENT 'Human-readable dungeon name',
    `waypoint_index`  SMALLINT(5)  UNSIGNED NOT NULL COMMENT 'Order in navigation path',
    `x`               FLOAT        NOT NULL COMMENT 'X position',
    `y`               FLOAT        NOT NULL COMMENT 'Y position',
    `z`               FLOAT        NOT NULL COMMENT 'Z position',
    `orientation`     FLOAT        NOT NULL DEFAULT 0 COMMENT 'Facing direction',
    `waypoint_type`   TINYINT(1)   UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=path, 1=boss, 2=trash_pack, 3=safe_spot, 4=event',
    `boss_entry`      INT          UNSIGNED DEFAULT 0 COMMENT 'Boss NPC entry if waypoint_type=1',
    `trash_pack_id`   SMALLINT(5)  UNSIGNED DEFAULT 0 COMMENT 'Trash group identifier',
    `requires_clear`  TINYINT(1)   UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Must clear before proceeding',
    `safe_radius`     FLOAT        NOT NULL DEFAULT 5.0 COMMENT 'Safe standing radius around waypoint',
    `wait_for_group`  TINYINT(1)   UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Tank waits for group here',
    `is_optional`     TINYINT(1)   UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Optional waypoint (skippable)',
    `description`     VARCHAR(255) DEFAULT NULL COMMENT 'Notes about this waypoint',

    PRIMARY KEY (`id`),
    INDEX `idx_map_waypoint` (`map_id`, `waypoint_index`),
    INDEX `idx_map_type` (`map_id`, `waypoint_type`),
    INDEX `idx_boss` (`boss_entry`)
)
ENGINE=MyISAM
DEFAULT CHARSET=utf8
ROW_FORMAT=FIXED
COMMENT='Dungeon navigation waypoints for tank lead strategy';

-- ============================================================================
-- Utgarde Keep (map_id = 574)
-- ============================================================================

INSERT INTO `playerbots_dungeon_waypoints`
(`map_id`, `dungeon_name`, `waypoint_index`, `x`, `y`, `z`, `orientation`, `waypoint_type`, `boss_entry`, `trash_pack_id`, `requires_clear`, `safe_radius`, `wait_for_group`, `is_optional`, `description`) VALUES

-- Entrance and first hallway
(574, 'Utgarde Keep', 1,  156.80, -88.80, 109.46, 1.57, 0, 0, 0, 0, 8.0, 1, 0, 'Dungeon entrance - wait for group'),
(574, 'Utgarde Keep', 2,  157.30, -57.00, 109.46, 1.57, 2, 0, 1, 1, 5.0, 0, 0, 'First trash pack - 2 Dragonflayer Ironhelms'),
(574, 'Utgarde Keep', 3,  162.50, -40.80, 109.46, 1.40, 0, 0, 0, 0, 5.0, 0, 0, 'Hallway turn'),
(574, 'Utgarde Keep', 4,  181.00, -29.50, 109.46, 0.80, 2, 0, 2, 1, 5.0, 0, 0, 'Second trash pack'),

-- First boss area
(574, 'Utgarde Keep', 5,  197.50, -14.60, 109.46, 0.78, 3, 0, 0, 0, 10.0, 1, 0, 'Pre-boss safe spot'),
(574, 'Utgarde Keep', 6,  210.80, -5.80,  109.46, 0.60, 2, 0, 3, 1, 5.0, 0, 0, 'Boss room trash'),
(574, 'Utgarde Keep', 7,  230.70, 13.20,  109.46, 0.78, 1, 23953, 0, 1, 8.0, 1, 0, 'Prince Keleseth - first boss'),

-- After first boss
(574, 'Utgarde Keep', 8,  260.50, 41.80,  109.46, 0.78, 0, 0, 0, 0, 5.0, 0, 0, 'Post-Keleseth path'),
(574, 'Utgarde Keep', 9,  281.20, 68.50,  109.46, 1.20, 2, 0, 4, 1, 5.0, 0, 0, 'Forge room trash'),
(574, 'Utgarde Keep', 10, 300.00, 95.20,  109.46, 1.57, 0, 0, 0, 0, 5.0, 0, 0, 'Forge room hallway'),

-- Second boss area
(574, 'Utgarde Keep', 11, 310.50, 138.80, 109.46, 1.57, 3, 0, 0, 0, 10.0, 1, 0, 'Pre-Skarvald safe spot'),
(574, 'Utgarde Keep', 12, 341.30, 175.50, 112.80, 1.57, 2, 0, 5, 1, 5.0, 0, 0, 'Skarvald room trash'),
(574, 'Utgarde Keep', 13, 343.00, 208.00, 112.80, 1.57, 1, 24200, 0, 1, 8.0, 1, 0, 'Skarvald the Constructor & Dalronn the Controller'),

-- Path to final boss
(574, 'Utgarde Keep', 14, 357.00, 240.00, 112.80, 1.20, 0, 0, 0, 0, 5.0, 0, 0, 'Stairs to upper level'),
(574, 'Utgarde Keep', 15, 380.50, 272.50, 118.50, 0.90, 2, 0, 6, 1, 5.0, 0, 0, 'Upper hallway trash'),
(574, 'Utgarde Keep', 16, 405.00, 290.00, 124.20, 0.60, 0, 0, 0, 0, 5.0, 0, 0, 'Upper hallway'),
(574, 'Utgarde Keep', 17, 438.00, 310.00, 134.50, 0.40, 2, 0, 7, 1, 5.0, 0, 0, 'Pre-Ingvar trash'),

-- Final boss
(574, 'Utgarde Keep', 18, 460.00, 325.00, 134.50, 0.20, 3, 0, 0, 0, 10.0, 1, 0, 'Pre-Ingvar safe spot'),
(574, 'Utgarde Keep', 19, 489.50, 341.00, 134.50, 0.10, 1, 23954, 0, 1, 10.0, 1, 0, 'Ingvar the Plunderer - final boss'),

-- ============================================================================
-- The Nexus (map_id = 576)
-- ============================================================================

-- Entrance area
(576, 'The Nexus', 1,  155.90, 13.90,  -16.10, 3.14, 0, 0, 0, 0, 8.0, 1, 0, 'Dungeon entrance'),
(576, 'The Nexus', 2,  128.50, 14.20,  -16.10, 3.14, 2, 0, 1, 1, 5.0, 0, 0, 'First trash pack'),
(576, 'The Nexus', 3,  95.80,  14.50,  -16.10, 3.14, 0, 0, 0, 0, 5.0, 0, 0, 'Central hub approach'),

-- Central hub and paths
(576, 'The Nexus', 4,  35.50,  14.80,  -16.10, 3.14, 3, 0, 0, 0, 10.0, 1, 0, 'Central hub - wait point'),

-- Path to Commander Kolurg/Stoutbeard
(576, 'The Nexus', 5,  5.20,   -35.60, -16.10, 4.71, 2, 0, 2, 1, 5.0, 0, 0, 'Horde Commander path trash'),
(576, 'The Nexus', 6,  -15.80, -68.90, -16.10, 4.71, 1, 26798, 0, 1, 8.0, 1, 0, 'Commander Kolurg/Stoutbeard'),

-- Path to Grand Magus Telestra
(576, 'The Nexus', 7,  35.50,  14.80,  -16.10, 3.14, 0, 0, 0, 0, 5.0, 0, 0, 'Return to hub'),
(576, 'The Nexus', 8,  -25.00, 48.50,  -16.10, 2.36, 2, 0, 3, 1, 5.0, 0, 0, 'Telestra path trash'),
(576, 'The Nexus', 9,  -65.20, 88.00,  -16.10, 2.36, 3, 0, 0, 0, 8.0, 1, 0, 'Pre-Telestra safe spot'),
(576, 'The Nexus', 10, -105.00, 128.50, -16.10, 2.36, 1, 26731, 0, 1, 10.0, 1, 0, 'Grand Magus Telestra'),

-- Path to Anomalus
(576, 'The Nexus', 11, 35.50,  14.80,  -16.10, 3.14, 0, 0, 0, 0, 5.0, 0, 0, 'Return to hub'),
(576, 'The Nexus', 12, 75.00,  -40.00, -16.10, 5.50, 2, 0, 4, 1, 5.0, 0, 0, 'Anomalus path trash'),
(576, 'The Nexus', 13, 115.00, -85.00, -16.10, 5.50, 3, 0, 0, 0, 8.0, 1, 0, 'Pre-Anomalus safe spot'),
(576, 'The Nexus', 14, 155.50, -130.20, -16.10, 5.50, 1, 26763, 0, 1, 10.0, 1, 0, 'Anomalus'),

-- Path to Ormorok the Tree-Shaper
(576, 'The Nexus', 15, 35.50,  14.80,  -16.10, 3.14, 0, 0, 0, 0, 5.0, 0, 0, 'Return to hub'),
(576, 'The Nexus', 16, 10.00,  65.00,  -16.10, 1.57, 2, 0, 5, 1, 5.0, 0, 0, 'Ormorok path trash'),
(576, 'The Nexus', 17, -20.00, 110.00, -16.10, 1.57, 3, 0, 0, 0, 8.0, 1, 0, 'Pre-Ormorok safe spot'),
(576, 'The Nexus', 18, -50.00, 155.00, -16.10, 1.57, 1, 26794, 0, 1, 10.0, 1, 0, 'Ormorok the Tree-Shaper'),

-- Path to Keristrasza
(576, 'The Nexus', 19, 35.50,  14.80,  -16.10, 0.00, 0, 0, 0, 0, 5.0, 0, 0, 'Return to hub'),
(576, 'The Nexus', 20, 65.00,  45.00,  -16.10, 0.78, 2, 0, 6, 1, 5.0, 0, 0, 'Keristrasza path trash'),
(576, 'The Nexus', 21, 95.00,  75.00,  -16.10, 0.78, 3, 0, 0, 0, 8.0, 1, 0, 'Pre-Keristrasza safe spot'),
(576, 'The Nexus', 22, 125.00, 105.00, -16.10, 0.78, 1, 26723, 0, 1, 12.0, 1, 0, 'Keristrasza - final boss'),

-- ============================================================================
-- Halls of Lightning (map_id = 602)
-- ============================================================================

-- Entrance and first section
(602, 'Halls of Lightning', 1,  1331.50, 92.00,  431.50, 0.00, 0, 0, 0, 0, 8.0, 1, 0, 'Dungeon entrance'),
(602, 'Halls of Lightning', 2,  1358.00, 92.00,  431.50, 0.00, 2, 0, 1, 1, 5.0, 0, 0, 'First hallway trash'),
(602, 'Halls of Lightning', 3,  1390.00, 92.00,  431.50, 0.00, 0, 0, 0, 0, 5.0, 0, 0, 'First hallway'),

-- General Bjarngrim area
(602, 'Halls of Lightning', 4,  1420.00, 92.00,  431.50, 0.00, 3, 0, 0, 0, 10.0, 1, 0, 'Pre-Bjarngrim safe spot'),
(602, 'Halls of Lightning', 5,  1455.00, 92.00,  431.50, 0.00, 2, 0, 2, 1, 5.0, 0, 0, 'Bjarngrim platform trash'),
(602, 'Halls of Lightning', 6,  1490.00, 100.00, 431.50, 0.20, 1, 28586, 0, 1, 10.0, 1, 0, 'General Bjarngrim'),

-- Path to Volkhan
(602, 'Halls of Lightning', 7,  1520.00, 115.00, 431.50, 0.50, 0, 0, 0, 0, 5.0, 0, 0, 'Post-Bjarngrim path'),
(602, 'Halls of Lightning', 8,  1550.00, 140.00, 431.50, 0.78, 2, 0, 3, 1, 5.0, 0, 0, 'Forge area trash'),
(602, 'Halls of Lightning', 9,  1580.00, 170.00, 431.50, 0.78, 3, 0, 0, 0, 10.0, 1, 0, 'Pre-Volkhan safe spot'),
(602, 'Halls of Lightning', 10, 1610.00, 200.00, 431.50, 0.78, 1, 28587, 0, 1, 10.0, 1, 0, 'Volkhan'),

-- Path to Ionar
(602, 'Halls of Lightning', 11, 1640.00, 230.00, 431.50, 0.78, 0, 0, 0, 0, 5.0, 0, 0, 'Post-Volkhan path'),
(602, 'Halls of Lightning', 12, 1670.00, 265.00, 431.50, 1.00, 2, 0, 4, 1, 5.0, 0, 0, 'Lightning tunnel trash'),
(602, 'Halls of Lightning', 13, 1700.00, 300.00, 431.50, 1.20, 3, 0, 0, 0, 10.0, 1, 0, 'Pre-Ionar safe spot'),
(602, 'Halls of Lightning', 14, 1730.00, 335.00, 431.50, 1.40, 1, 28546, 0, 1, 12.0, 1, 0, 'Ionar'),

-- Path to Loken
(602, 'Halls of Lightning', 15, 1760.00, 370.00, 431.50, 1.57, 0, 0, 0, 0, 5.0, 0, 0, 'Post-Ionar path'),
(602, 'Halls of Lightning', 16, 1785.00, 410.00, 431.50, 1.57, 2, 0, 5, 1, 5.0, 0, 0, 'Loken hallway trash'),
(602, 'Halls of Lightning', 17, 1810.00, 450.00, 431.50, 1.57, 3, 0, 0, 0, 12.0, 1, 0, 'Pre-Loken safe spot'),
(602, 'Halls of Lightning', 18, 1835.00, 490.00, 431.50, 1.57, 1, 28923, 0, 1, 15.0, 1, 0, 'Loken - final boss');
