/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "AnticipatoryThreatValue.h"
#include "DatabaseEnv.h"
#include "DungeonNavigator.h"
#include "DynamicObject.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

// ============================================================================
// AnticipatoryThreatManager Implementation
// ============================================================================

void AnticipatoryThreatManager::Initialize()
{
    if (m_initialized)
        return;

    LOG_INFO("playerbots", "AnticipatoryThreatManager: Loading boss abilities...");
    LoadFromDB();
    m_initialized = true;

    LOG_INFO("playerbots", "AnticipatoryThreatManager: Loaded {} bosses with {} abilities",
             GetBossCount(), GetTotalAbilityCount());
}

void AnticipatoryThreatManager::Reload()
{
    LOG_INFO("playerbots", "AnticipatoryThreatManager: Reloading boss abilities...");

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_bossAbilities.clear();
        m_spellIndex.clear();
    }

    LoadFromDB();

    LOG_INFO("playerbots", "AnticipatoryThreatManager: Reloaded {} bosses with {} abilities",
             GetBossCount(), GetTotalAbilityCount());
}

void AnticipatoryThreatManager::LoadFromDB()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    QueryResult result = PlayerbotsDatabase.Query(
        "SELECT boss_entry, boss_name, spell_id, ability_name, cooldown_ms, cast_time_ms, "
        "base_damage, is_aoe, aoe_radius, requires_movement, safe_move_dist, severity, "
        "mechanic_type, description "
        "FROM playerbots_boss_abilities "
        "ORDER BY boss_entry, severity DESC");

    if (!result)
    {
        LOG_WARN("playerbots", "AnticipatoryThreatManager: No boss abilities found in database");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        BossAbilityData ability;
        ability.bossEntry = fields[0].Get<uint32>();
        ability.bossName = fields[1].Get<std::string>();
        ability.spellId = fields[2].Get<uint32>();
        ability.abilityName = fields[3].Get<std::string>();
        ability.cooldownMs = fields[4].Get<uint32>();
        ability.castTimeMs = fields[5].Get<uint32>();
        ability.baseDamage = fields[6].Get<uint32>();
        ability.isAoe = fields[7].Get<bool>();
        ability.aoeRadius = fields[8].Get<float>();
        ability.requiresMovement = fields[9].Get<bool>();
        ability.safeMoveDistance = fields[10].Get<float>();
        ability.severity = fields[11].Get<uint8>();

        std::string mechanicStr = fields[12].Get<std::string>();
        ability.mechanic = ParseMechanicType(mechanicStr);

        ability.description = fields[13].Get<std::string>();

        m_bossAbilities[ability.bossEntry].push_back(ability);

    } while (result->NextRow());

    // Build spell index after all abilities loaded - store (bossEntry, index) pairs
    // to avoid pointer invalidation issues when vectors reallocate
    for (auto& [bossEntry, abilities] : m_bossAbilities)
    {
        for (size_t i = 0; i < abilities.size(); ++i)
        {
            m_spellIndex[abilities[i].spellId] = std::make_pair(bossEntry, i);
        }
    }
}

std::vector<const BossAbilityData*> AnticipatoryThreatManager::GetBossAbilities(uint32 bossEntry) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<const BossAbilityData*> result;

    auto it = m_bossAbilities.find(bossEntry);
    if (it != m_bossAbilities.end())
    {
        for (const auto& ability : it->second)
        {
            result.push_back(&ability);
        }
    }

    return result;
}

const BossAbilityData* AnticipatoryThreatManager::GetAbilityData(uint32 bossEntry, uint32 spellId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto bossIt = m_bossAbilities.find(bossEntry);
    if (bossIt == m_bossAbilities.end())
        return nullptr;

    for (const auto& ability : bossIt->second)
    {
        if (ability.spellId == spellId)
            return &ability;
    }

    return nullptr;
}

const BossAbilityData* AnticipatoryThreatManager::GetAbilityBySpell(uint32 spellId) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_spellIndex.find(spellId);
    if (it == m_spellIndex.end())
        return nullptr;

    // Safely look up using (bossEntry, index) pair
    uint32 bossEntry = it->second.first;
    size_t index = it->second.second;

    auto bossIt = m_bossAbilities.find(bossEntry);
    if (bossIt == m_bossAbilities.end() || index >= bossIt->second.size())
        return nullptr;

    return &bossIt->second[index];
}

bool AnticipatoryThreatManager::IsDangerousAbility(uint32 spellId) const
{
    const BossAbilityData* ability = GetAbilityBySpell(spellId);
    return ability && ability->IsDangerous();
}

bool AnticipatoryThreatManager::HasBossData(uint32 bossEntry) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_bossAbilities.find(bossEntry) != m_bossAbilities.end();
}

ThreatMechanicType AnticipatoryThreatManager::ParseMechanicType(const std::string& str)
{
    if (str == "spread") return ThreatMechanicType::SPREAD;
    if (str == "stack") return ThreatMechanicType::STACK;
    if (str == "interrupt") return ThreatMechanicType::INTERRUPT;
    if (str == "dispel") return ThreatMechanicType::DISPEL;
    if (str == "move_away") return ThreatMechanicType::MOVE_AWAY;
    if (str == "move_to") return ThreatMechanicType::MOVE_TO;
    if (str == "avoid_ground") return ThreatMechanicType::AVOID_GROUND;
    if (str == "face_away") return ThreatMechanicType::FACE_AWAY;
    if (str == "stop_casting") return ThreatMechanicType::STOP_CASTING;
    if (str == "tank_swap") return ThreatMechanicType::TANK_SWAP;
    return ThreatMechanicType::NONE;
}

const char* AnticipatoryThreatManager::MechanicTypeToString(ThreatMechanicType type)
{
    switch (type)
    {
        case ThreatMechanicType::SPREAD: return "spread";
        case ThreatMechanicType::STACK: return "stack";
        case ThreatMechanicType::INTERRUPT: return "interrupt";
        case ThreatMechanicType::DISPEL: return "dispel";
        case ThreatMechanicType::MOVE_AWAY: return "move_away";
        case ThreatMechanicType::MOVE_TO: return "move_to";
        case ThreatMechanicType::AVOID_GROUND: return "avoid_ground";
        case ThreatMechanicType::FACE_AWAY: return "face_away";
        case ThreatMechanicType::STOP_CASTING: return "stop_casting";
        case ThreatMechanicType::TANK_SWAP: return "tank_swap";
        default: return "none";
    }
}

size_t AnticipatoryThreatManager::GetBossCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_bossAbilities.size();
}

size_t AnticipatoryThreatManager::GetTotalAbilityCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    size_t count = 0;
    for (const auto& [bossEntry, abilities] : m_bossAbilities)
    {
        count += abilities.size();
    }
    return count;
}

// ============================================================================
// IncomingDamageValue Implementation
// ============================================================================

uint32 IncomingDamageValue::Calculate()
{
    uint32 damage = 0;
    damage += CalculateFromActiveCasts();
    damage += CalculateFromAuras();
    return damage;
}

uint32 IncomingDamageValue::CalculateFromActiveCasts()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return 0;

    uint32 totalDamage = 0;

    // Check enemies targeting us that are casting
    for (auto& ref : bot->getHostileRefMgr())
    {
        Unit* enemy = ref.GetSource()->GetOwner();
        if (!enemy || !enemy->IsAlive())
            continue;

        Spell* spell = enemy->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = enemy->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (spell)
        {
            // Check if we're the target
            ObjectGuid targetGuid = spell->m_targets.GetUnitTargetGUID();
            if (targetGuid == bot->GetGUID() || targetGuid.IsEmpty())
            {
                // Look up in our database
                const BossAbilityData* ability = sAnticipatoryThreat->GetAbilityBySpell(spell->GetSpellInfo()->Id);
                if (ability)
                {
                    totalDamage += ability->baseDamage;
                }
                else
                {
                    // Estimate from spell info
                    // This is a rough estimate based on spell effects
                    const SpellInfo* info = spell->GetSpellInfo();
                    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    {
                        if (info->Effects[i].Effect == SPELL_EFFECT_SCHOOL_DAMAGE ||
                            info->Effects[i].Effect == SPELL_EFFECT_WEAPON_DAMAGE)
                        {
                            totalDamage += info->Effects[i].CalcValue(enemy);
                        }
                    }
                }
            }
        }
    }

    return totalDamage;
}

uint32 IncomingDamageValue::CalculateFromAuras()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return 0;

    uint32 totalDamage = 0;

    // Check periodic damage auras
    Unit::AuraApplicationMap const& auras = bot->GetAppliedAuras();
    for (auto const& [spellId, auraApp] : auras)
    {
        Aura* aura = auraApp->GetBase();
        if (!aura)
            continue;

        // Check for periodic damage effects
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            AuraEffect* effect = aura->GetEffect(i);
            if (!effect)
                continue;

            if (effect->GetAuraType() == SPELL_AURA_PERIODIC_DAMAGE ||
                effect->GetAuraType() == SPELL_AURA_PERIODIC_LEECH)
            {
                // Estimate damage from remaining ticks
                int32 damage = effect->GetAmount();
                uint32 remaining = aura->GetDuration();
                uint32 amplitude = effect->GetAmplitude();

                if (amplitude > 0)
                {
                    uint32 ticksRemaining = remaining / amplitude;
                    totalDamage += damage * ticksRemaining;
                }
            }
        }
    }

    return totalDamage;
}

// ============================================================================
// ActiveThreatsValue Implementation
// ============================================================================

std::vector<ActiveThreat> ActiveThreatsValue::Calculate()
{
    std::vector<ActiveThreat> threats;

    ScanEnemyCasts(threats);
    ScanGroundEffects(threats);

    // Sort by urgency (soonest first)
    std::sort(threats.begin(), threats.end(),
        [](const ActiveThreat& a, const ActiveThreat& b) {
            return a.GetRemainingCastTime() < b.GetRemainingCastTime();
        });

    return threats;
}

void ActiveThreatsValue::ScanEnemyCasts(std::vector<ActiveThreat>& threats)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();

    for (auto& ref : bot->getHostileRefMgr())
    {
        Unit* enemy = ref.GetSource()->GetOwner();
        if (!enemy || !enemy->IsAlive())
            continue;

        // Check all spell slots
        for (uint8 spellType = 0; spellType < CURRENT_MAX_SPELL; ++spellType)
        {
            Spell* spell = enemy->GetCurrentSpell(static_cast<CurrentSpellTypes>(spellType));
            if (!spell)
                continue;

            const SpellInfo* info = spell->GetSpellInfo();
            if (!info)
                continue;

            // Look up ability data
            const BossAbilityData* ability = sAnticipatoryThreat->GetAbilityBySpell(info->Id);

            ActiveThreat threat;
            threat.sourceGuid = enemy->GetGUID();
            threat.spellId = info->Id;
            threat.ability = ability;
            threat.sourcePosition.Relocate(enemy->GetPositionX(), enemy->GetPositionY(),
                                           enemy->GetPositionZ(), enemy->GetOrientation());

            // Calculate timing
            int32 castTime = spell->GetCastTime();
            int32 elapsed = spell->GetTimer();
            threat.castStartTime = now - elapsed;
            threat.estimatedImpactTime = now + (castTime - elapsed);

            // Check if targeting bot
            ObjectGuid targetGuid = spell->m_targets.GetUnitTargetGUID();
            threat.isTargetingBot = (targetGuid == bot->GetGUID());

            if (ability)
            {
                threat.estimatedDamage = ability->baseDamage;
                threat.mechanic = ability->mechanic;
            }

            threats.push_back(threat);
        }
    }
}

void ActiveThreatsValue::ScanGroundEffects(std::vector<ActiveThreat>& threats)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return;

    Map* map = bot->GetMap();
    if (!map)
        return;

    // Known dangerous ground effect spell IDs
    static const std::unordered_set<uint32> dangerousGroundEffects = {
        // Death Knight
        43265,  // Death and Decay
        52212,  // Death and Decay (Rank 2)
        // Mage
        2120,   // Flamestrike
        10,     // Blizzard
        // Warlock
        5740,   // Rain of Fire
        1949,   // Hellfire
        // Boss abilities - common dangerous ground effects
        28547,  // Chill (Sapphiron)
        28531,  // Frost Aura (Sapphiron)
        29371,  // Eruption (Heigan)
        28433,  // Slime Spray (Grobbulus)
        28240,  // Poison Cloud (Grobbulus)
        69024,  // Freezing Ground (HoR)
        69789,  // Ooze Flood (Rotface)
        71215,  // Malleable Goo (Putricide)
        72295,  // Malleable Goo (Putricide H)
        69508,  // Slime Spray (Rotface)
        71224,  // Mutated Infection (Putricide)
        70852,  // Ooze Eruption (Festergut)
        70341,  // Slime Puddle (Festergut)
        70672,  // Gaseous Blight (Festergut)
        70360,  // Defile (Lich King)
        72762,  // Defile (Lich King H)
        69146,  // Coldflame (Lord Marrowgar)
    };

    uint32 now = getMSTime();
    float searchRadius = 50.0f;

    // Search for DynamicObjects near the bot
    std::list<DynamicObject*> dynObjs;
    bot->GetMap()->GetDynamicObjectListInGrid(dynObjs, bot->GetPositionX(), bot->GetPositionY());

    for (DynamicObject* dynObj : dynObjs)
    {
        if (!dynObj)
            continue;

        uint32 spellId = dynObj->GetSpellId();
        float radius = dynObj->GetRadius();
        float distToBot = bot->GetDistance(dynObj);

        // Check if this is a known dangerous effect or if bot is close to it
        bool isDangerous = dangerousGroundEffects.count(spellId) > 0;
        bool isClose = distToBot < (radius + 5.0f);  // Within danger zone + safety margin

        if (!isDangerous && !isClose)
            continue;

        // Create threat entry
        ActiveThreat threat;
        threat.sourceGuid = dynObj->GetCasterGUID();
        threat.spellId = spellId;
        threat.ability = sAnticipatoryThreat->GetAbilityBySpell(spellId);
        threat.sourcePosition.Relocate(dynObj->GetPositionX(), dynObj->GetPositionY(), dynObj->GetPositionZ());
        threat.targetPosition = threat.sourcePosition;
        threat.castStartTime = now;
        threat.estimatedImpactTime = now;  // Ground effects are immediate
        threat.isTargetingBot = isClose;
        threat.mechanic = ThreatMechanicType::AVOID_GROUND;

        // Estimate damage if we have ability data
        if (threat.ability)
        {
            threat.estimatedDamage = threat.ability->baseDamage;
        }
        else
        {
            // Default estimate for unknown ground effects
            threat.estimatedDamage = bot->GetMaxHealth() / 5;  // Assume ~20% health per tick
        }

        threats.push_back(threat);
    }
}

// ============================================================================
// ThreatPredictionValue Implementation
// ============================================================================

ThreatPrediction ThreatPredictionValue::Calculate()
{
    ThreatPrediction prediction;

    std::vector<ActiveThreat> threats = AI_VALUE(std::vector<ActiveThreat>, "active threats");
    if (threats.empty())
        return prediction;

    AnalyzeThreats(threats, prediction);

    return prediction;
}

void ThreatPredictionValue::AnalyzeThreats(const std::vector<ActiveThreat>& threats,
                                            ThreatPrediction& prediction)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return;

    uint32 botMaxHealth = bot->GetMaxHealth();
    float totalExpectedDamage = 0.0f;

    for (const auto& threat : threats)
    {
        totalExpectedDamage += threat.estimatedDamage;

        // Determine response based on mechanic
        if (threat.ability && threat.ability->NeedsResponse())
        {
            switch (threat.mechanic)
            {
                case ThreatMechanicType::INTERRUPT:
                    prediction.shouldInterrupt = true;
                    break;

                case ThreatMechanicType::DISPEL:
                    prediction.shouldDispel = true;
                    break;

                case ThreatMechanicType::MOVE_AWAY:
                case ThreatMechanicType::SPREAD:
                case ThreatMechanicType::AVOID_GROUND:
                    prediction.shouldPreposition = true;
                    prediction.suggestedResponse = threat.mechanic;
                    prediction.safePosition = CalculateSafePosition(threat);
                    break;

                case ThreatMechanicType::MOVE_TO:
                case ThreatMechanicType::STACK:
                    prediction.shouldPreposition = true;
                    prediction.suggestedResponse = threat.mechanic;
                    prediction.safePosition = threat.sourcePosition;  // Move to source
                    break;

                default:
                    break;
            }

            // Track most urgent response needed
            if (threat.GetRemainingCastTime() < prediction.urgencyMs || prediction.urgencyMs == 0)
            {
                prediction.urgencyMs = threat.GetRemainingCastTime();
            }
        }
    }

    prediction.expectedDamage = totalExpectedDamage;
    prediction.expectedDamagePercent = (botMaxHealth > 0)
        ? (totalExpectedDamage / static_cast<float>(botMaxHealth)) * 100.0f
        : 0.0f;
}

Position ThreatPredictionValue::CalculateSafePosition(const ActiveThreat& threat)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return Position();

    float safeDistance = 10.0f;  // Default safe distance
    if (threat.ability)
    {
        safeDistance = threat.ability->safeMoveDistance;
        if (safeDistance <= 0)
            safeDistance = threat.ability->aoeRadius + 5.0f;
    }

    // Calculate position away from threat source
    float dx = bot->GetPositionX() - threat.sourcePosition.GetPositionX();
    float dy = bot->GetPositionY() - threat.sourcePosition.GetPositionY();
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 0.1f)
    {
        // Bot is at source, pick a random direction to avoid degenerate case
        float angle = frand(0, 2.0f * static_cast<float>(M_PI));
        dx = std::cos(angle);
        dy = std::sin(angle);
        dist = 1.0f;
    }

    // Normalize and scale to safe distance from threat source
    dx = (dx / dist) * safeDistance;
    dy = (dy / dist) * safeDistance;

    float newX = threat.sourcePosition.GetPositionX() + dx;
    float newY = threat.sourcePosition.GetPositionY() + dy;

    // Get proper ground Z coordinate at the new position
    float newZ = bot->GetPositionZ();
    Map* map = bot->GetMap();
    if (map)
    {
        float groundZ = map->GetHeight(bot->GetPhaseMask(), newX, newY, newZ + 5.0f, true, 20.0f);
        if (groundZ > INVALID_HEIGHT)
        {
            newZ = groundZ;
        }
    }

    Position safePos;
    safePos.Relocate(newX, newY, newZ);

    return safePos;
}

// ============================================================================
// ShouldPrepositionValue Implementation
// ============================================================================

bool ShouldPrepositionValue::Calculate()
{
    ThreatPrediction prediction = AI_VALUE(ThreatPrediction, "threat prediction");
    return prediction.shouldPreposition && prediction.urgencyMs < 3000;
}

// ============================================================================
// PreemptiveHealTargetsValue Implementation
// ============================================================================

std::vector<ObjectGuid> PreemptiveHealTargetsValue::Calculate()
{
    std::vector<ObjectGuid> targets;

    Player* bot = botAI->GetBot();
    if (!bot)
        return targets;

    Group* group = bot->GetGroup();
    if (!group)
        return targets;

    // Get active threats
    std::vector<ActiveThreat> threats = AI_VALUE(std::vector<ActiveThreat>, "active threats");

    // Find group members who will take significant damage
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        float expectedDamage = 0.0f;

        for (const auto& threat : threats)
        {
            if (threat.ability && threat.ability->isAoe)
            {
                // Check if member is in AOE range
                float dist = member->GetDistance(threat.sourcePosition.GetPositionX(),
                                                  threat.sourcePosition.GetPositionY(),
                                                  threat.sourcePosition.GetPositionZ());
                if (dist <= threat.ability->aoeRadius)
                {
                    expectedDamage += threat.estimatedDamage;
                }
            }
        }

        // Add to list if expected damage would bring them low
        float expectedHealthPct = ((member->GetHealth() - expectedDamage) /
                                    static_cast<float>(member->GetMaxHealth())) * 100.0f;

        if (expectedHealthPct < 50.0f && expectedDamage > 0)
        {
            targets.push_back(member->GetGUID());
        }
    }

    // Sort by expected final health (lowest first)
    std::sort(targets.begin(), targets.end(),
        [group, &threats](ObjectGuid a, ObjectGuid b) {
            Player* pA = ObjectAccessor::FindPlayer(a);
            Player* pB = ObjectAccessor::FindPlayer(b);
            if (!pA || !pB) return false;
            return pA->GetHealthPct() < pB->GetHealthPct();
        });

    return targets;
}

// ============================================================================
// HighestThreatValue Implementation
// ============================================================================

ActiveThreat HighestThreatValue::Calculate()
{
    std::vector<ActiveThreat> threats = AI_VALUE(std::vector<ActiveThreat>, "active threats");

    if (threats.empty())
        return ActiveThreat();

    // Find highest severity threat
    auto it = std::max_element(threats.begin(), threats.end(),
        [](const ActiveThreat& a, const ActiveThreat& b) {
            uint8 severityA = a.ability ? a.ability->severity : 0;
            uint8 severityB = b.ability ? b.ability->severity : 0;
            return severityA < severityB;
        });

    return *it;
}

// ============================================================================
// SafePositionValue Implementation
// ============================================================================

Position SafePositionValue::Calculate()
{
    ThreatPrediction prediction = AI_VALUE(ThreatPrediction, "threat prediction");

    if (prediction.shouldPreposition)
        return prediction.safePosition;

    // Return current position if no movement needed
    Player* bot = botAI->GetBot();
    if (bot)
    {
        Position pos;
        pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
        return pos;
    }

    return Position();
}

// ============================================================================
// AtDungeonWaypointValue Implementation
// ============================================================================

bool AtDungeonWaypointValue::Calculate()
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    uint32 mapId = bot->GetMapId();
    if (!sDungeonNavigator->HasDungeonPath(mapId))
        return false;

    Group* group = bot->GetGroup();
    uint32 groupId = group ? group->GetGUID().GetCounter() : bot->GetGUID().GetCounter();

    GroupProgress* progress = sDungeonNavigator->GetGroupProgress(groupId, mapId);
    if (!progress)
        return false;

    Position pos;
    pos.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    return sDungeonNavigator->IsAtWaypoint(mapId, pos, progress->currentWaypointIndex);
}
