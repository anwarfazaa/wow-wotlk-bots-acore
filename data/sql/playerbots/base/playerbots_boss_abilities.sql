DROP TABLE IF EXISTS `playerbots_boss_abilities`;
CREATE TABLE `playerbots_boss_abilities` (
    `id`                INT          NOT NULL AUTO_INCREMENT,
    `boss_entry`        INT          UNSIGNED NOT NULL COMMENT 'Boss NPC entry',
    `boss_name`         VARCHAR(64)  NOT NULL COMMENT 'Human-readable boss name',
    `spell_id`          INT          UNSIGNED NOT NULL COMMENT 'Ability spell ID',
    `ability_name`      VARCHAR(64)  NOT NULL COMMENT 'Ability name',
    `cooldown_ms`       INT          UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Cooldown in milliseconds',
    `cast_time_ms`      INT          UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Cast time in milliseconds',
    `base_damage`       INT          UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Estimated base damage',
    `is_aoe`            TINYINT(1)   UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Is this an AOE ability',
    `aoe_radius`        FLOAT        NOT NULL DEFAULT 0 COMMENT 'AOE radius in yards',
    `requires_movement` TINYINT(1)   UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Requires repositioning',
    `safe_move_dist`    FLOAT        NOT NULL DEFAULT 0 COMMENT 'Distance to move for safety',
    `severity`          TINYINT(3)   UNSIGNED NOT NULL DEFAULT 50 COMMENT 'Danger level 0-100',
    `mechanic_type`     VARCHAR(32)  DEFAULT NULL COMMENT 'spread/stack/interrupt/dispel/move_away/move_to',
    `description`       VARCHAR(255) DEFAULT NULL COMMENT 'Notes about this ability',

    PRIMARY KEY (`id`),
    INDEX `idx_boss` (`boss_entry`),
    INDEX `idx_spell` (`spell_id`),
    INDEX `idx_mechanic` (`mechanic_type`)
)
ENGINE=MyISAM
DEFAULT CHARSET=utf8
ROW_FORMAT=FIXED
COMMENT='Boss ability data for anticipatory threat system';

-- ============================================================================
-- Utgarde Keep Bosses
-- ============================================================================

-- Prince Keleseth (entry: 23953)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(23953, 'Prince Keleseth', 48400, 'Frost Tomb',        15000, 0,    0,     0, 0,    0, 0,  70, 'dispel',    'Encases random player in ice - dispel or break'),
(23953, 'Prince Keleseth', 59397, 'Shadow Bolt',       2000,  2000, 5000,  0, 0,    0, 0,  30, 'interrupt', 'Shadow damage to tank - interruptible'),
(23953, 'Prince Keleseth', 48878, 'Skeleton Adds',     20000, 0,    0,     0, 0,    0, 0,  40, NULL,        'Summons skeleton adds - tank pick up');

-- Skarvald the Constructor (entry: 24200)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(24200, 'Skarvald the Constructor', 48261, 'Stone Strike',   8000,  0,    4000,  0, 0,    0, 0,  50, NULL,       'Physical hit on tank'),
(24200, 'Skarvald the Constructor', 48263, 'Charge',         12000, 0,    2500,  0, 0,    0, 0,  40, NULL,       'Charges random player');

-- Dalronn the Controller (entry: 24201)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(24201, 'Dalronn the Controller', 43649, 'Shadow Bolt',     3000,  2500, 4500,  0, 0,    0, 0,  35, 'interrupt', 'Shadow bolt - interruptible'),
(24201, 'Dalronn the Controller', 43650, 'Debilitate',      15000, 0,    0,     0, 0,    0, 0,  60, 'dispel',    'Reduces damage and movement - dispel');

-- Ingvar the Plunderer (entry: 23954)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(23954, 'Ingvar the Plunderer', 42708, 'Smash',             12000, 3000, 8000,  1, 8,    1, 10, 85, 'move_away', 'Frontal cone - avoid'),
(23954, 'Ingvar the Plunderer', 42723, 'Dark Smash',        12000, 3000, 12000, 1, 8,    1, 10, 90, 'move_away', 'Phase 2 frontal cone - avoid'),
(23954, 'Ingvar the Plunderer', 42729, 'Dreadful Roar',     18000, 2000, 3000,  1, 40,   0, 0,  60, 'interrupt', 'AOE silence - interrupt if possible'),
(23954, 'Ingvar the Plunderer', 42669, 'Staggering Roar',   15000, 0,    2500,  1, 30,   0, 0,  50, NULL,        'AOE knockback and damage');

-- ============================================================================
-- The Nexus Bosses
-- ============================================================================

-- Grand Magus Telestra (entry: 26731)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(26731, 'Grand Magus Telestra', 47765, 'Firebomb',         8000,  0,    4500,  1, 5,    1, 8,  55, 'move_away', 'Fire damage at location'),
(26731, 'Grand Magus Telestra', 47772, 'Ice Nova',         12000, 0,    5000,  1, 15,   0, 0,  60, NULL,        'AOE frost damage'),
(26731, 'Grand Magus Telestra', 47731, 'Critter',          0,     2000, 0,     0, 0,    0, 0,  40, 'dispel',    'Polymorph - dispel'),
(26731, 'Grand Magus Telestra', 47748, 'Split',            0,     0,    0,     0, 0,    0, 0,  70, NULL,        'Splits into 3 copies - kill all');

-- Anomalus (entry: 26763)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(26763, 'Anomalus', 47751, 'Spark',              10000, 0,    3500,  0, 0,    0, 0,  45, NULL,        'Arcane damage bolt'),
(26763, 'Anomalus', 47756, 'Create Rift',        25000, 0,    0,     0, 0,    0, 0,  60, NULL,        'Creates chaotic rift - kill adds'),
(26763, 'Anomalus', 57049, 'Charge Rifts',       0,     0,    0,     0, 0,    1, 15, 70, 'move_away', 'Charges rifts - avoid standing near');

-- Ormorok the Tree-Shaper (entry: 26794)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(26794, 'Ormorok the Tree-Shaper', 47958, 'Crystal Spikes', 12000, 0,    6500,  1, 3,    1, 5,  75, 'move_away', 'Ground spikes - move away'),
(26794, 'Ormorok the Tree-Shaper', 48017, 'Trample',        15000, 0,    4000,  1, 10,   0, 0,  55, NULL,        'AOE physical damage'),
(26794, 'Ormorok the Tree-Shaper', 61564, 'Spell Reflection', 20000, 0,  0,     0, 0,    0, 0,  50, NULL,        'Reflects spells - stop casting');

-- Keristrasza (entry: 26723)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(26723, 'Keristrasza', 48094, 'Intense Cold',       0,     0,    2000,  1, 100,  0, 0,  80, NULL,          'Stacking frost damage aura - KEEP MOVING'),
(26723, 'Keristrasza', 48096, 'Crystal Chains',     12000, 0,    0,     0, 0,    0, 0,  60, 'dispel',      'Roots player - dispel or jump'),
(26723, 'Keristrasza', 48179, 'Crystalize',         18000, 3000, 7500,  1, 20,   0, 0,  70, 'interrupt',   'AOE frost damage - interrupt'),
(26723, 'Keristrasza', 50155, 'Tail Sweep',         8000,  0,    4000,  1, 10,   1, 0,  45, 'move_away',   'Tail swipe - dont stand behind');

-- ============================================================================
-- Halls of Lightning Bosses
-- ============================================================================

-- General Bjarngrim (entry: 28586)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(28586, 'General Bjarngrim', 52026, 'Mortal Strike',   8000,  0,    6000,  0, 0,    0, 0,  55, NULL,       'Reduces healing - heavy tank damage'),
(28586, 'General Bjarngrim', 52027, 'Whirlwind',       12000, 0,    4500,  1, 8,    1, 10, 70, 'move_away', 'AOE whirlwind - melee move out'),
(28586, 'General Bjarngrim', 52028, 'Knock Away',      15000, 0,    3000,  1, 5,    0, 0,  40, NULL,       'Knockback on tank'),
(28586, 'General Bjarngrim', 52098, 'Charge',          20000, 0,    2500,  0, 0,    0, 0,  35, NULL,       'Charges random target');

-- Volkhan (entry: 28587)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(28587, 'Volkhan', 52237, 'Heat',           0,     0,    1500,  0, 0,    0, 0,  40, NULL,       'Stacking fire damage aura'),
(28587, 'Volkhan', 52238, 'Temper',         15000, 0,    0,     0, 0,    0, 0,  50, NULL,       'Creates Molten Golem add'),
(28587, 'Volkhan', 59529, 'Shattering Stomp', 25000, 0,  8000,  1, 100,  0, 0,  85, NULL,       'Destroys golems for massive AOE - kill golems first');

-- Ionar (entry: 28546)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(28546, 'Ionar', 52658, 'Static Overload', 8000,  0,    4000,  1, 6,    1, 10, 65, 'spread',     'Chain lightning - spread out'),
(28546, 'Ionar', 52661, 'Ball Lightning',  12000, 0,    5500,  0, 0,    0, 0,  55, NULL,        'Lightning ball damage'),
(28546, 'Ionar', 52770, 'Disperse',        30000, 0,    0,     0, 0,    1, 20, 80, 'move_away', 'Splits into sparks - avoid sparks');

-- Loken (entry: 28923)
INSERT INTO `playerbots_boss_abilities`
(`boss_entry`, `boss_name`, `spell_id`, `ability_name`, `cooldown_ms`, `cast_time_ms`, `base_damage`, `is_aoe`, `aoe_radius`, `requires_movement`, `safe_move_dist`, `severity`, `mechanic_type`, `description`) VALUES
(28923, 'Loken', 59835, 'Lightning Nova',  15000, 5000, 15000, 1, 100,  0, 0,  95, 'move_to',   'MASSIVE damage far away - STACK ON BOSS'),
(28923, 'Loken', 52960, 'Pulsing Shockwave', 0,   0,    2500,  1, 100,  0, 0,  70, NULL,        'Constant aura - damage increases with distance'),
(28923, 'Loken', 59837, 'Arc Lightning',   10000, 0,    5000,  0, 0,    0, 0,  50, NULL,        'Chain lightning on random targets');
