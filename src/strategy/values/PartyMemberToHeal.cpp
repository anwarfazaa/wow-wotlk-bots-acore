/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "PartyMemberToHeal.h"

#include "AnticipatoryThreatValue.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "Spell.h"
#include "SpellAuraEffects.h"

// Static member initialization
std::unordered_map<uint64, HealthTrendData> PartyMemberToHeal::s_healthTrends;

class IsTargetOfHealingSpell : public SpellEntryPredicate
{
public:
    bool Check(SpellInfo const* spellInfo) override
    {
        for (uint8 i = 0; i < 3; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL_MAX_HEALTH ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL_MECHANICAL)
                return true;
        }

        return false;
    }
};

inline bool compareByHealth(Unit const* u1, Unit const* u2) { return u1->GetHealthPct() < u2->GetHealthPct(); }

Unit* PartyMemberToHeal::Calculate()
{
    IsTargetOfHealingSpell predicate;

    Group* group = bot->GetGroup();
    if (!group)
        return bot;

    bool isRaid = bot->GetGroup()->isRaidGroup();

    Unit* bestTarget = nullptr;
    float bestPriority = 200.0f;  // Lower is better, start high

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player || player->IsGameMaster() || !player->IsAlive())
            continue;

        // Update health trend tracking
        UpdateHealthTrend(player);

        // Calculate priority score
        float priority = CalculateHealPriority(player, isRaid);

        // Skip if already being healed (unless priority is very high)
        if (priority > 30.0f && IsTargetOfSpellCast(player, predicate))
            continue;

        // Check validity
        if (priority < bestPriority && Check(player))
        {
            bestPriority = priority;
            bestTarget = player;
        }

        // Also check pets
        Pet* pet = player->GetPet();
        if (pet && pet->IsAlive())
        {
            UpdateHealthTrend(pet);
            float petPriority = CalculateHealPriority(pet, isRaid);
            // Pets get lower priority than players (+15 penalty)
            petPriority += 15.0f;

            if (petPriority < bestPriority && Check(pet))
            {
                bestPriority = petPriority;
                bestTarget = pet;
            }
        }

        // Check charmed units
        Unit* charm = player->GetCharm();
        if (charm && charm->IsAlive())
        {
            float charmPriority = CalculateHealPriority(charm, isRaid);
            charmPriority += 20.0f;  // Lower priority than pets

            if (charmPriority < bestPriority && Check(charm))
            {
                bestPriority = charmPriority;
                bestTarget = charm;
            }
        }
    }

    return bestTarget;
}

float PartyMemberToHeal::CalculateHealPriority(Unit* unit, bool isRaid)
{
    if (!unit)
        return 200.0f;

    float currentHealth = unit->GetHealthPct();
    uint32 maxHealth = unit->GetMaxHealth();

    // Base priority is current health percentage
    float priority = currentHealth;

    // =========================================================================
    // Factor 1: Incoming Damage Prediction
    // =========================================================================
    uint32 incomingDamage = GetIncomingDamage(unit);
    if (incomingDamage > 0 && maxHealth > 0)
    {
        // Calculate predicted health after incoming damage
        float predictedHealthPct = ((unit->GetHealth() - incomingDamage) /
                                     static_cast<float>(maxHealth)) * 100.0f;

        // If predicted health is worse than current, use that for priority
        if (predictedHealthPct < currentHealth)
        {
            // Blend current and predicted (60% predicted, 40% current)
            priority = (predictedHealthPct * 0.6f) + (currentHealth * 0.4f);

            // Emergency: if predicted death, huge priority boost
            if (predictedHealthPct <= 0.0f)
            {
                priority -= 30.0f;
            }
            else if (predictedHealthPct < 20.0f)
            {
                priority -= 15.0f;
            }
        }
    }

    // =========================================================================
    // Factor 2: Role Priority
    // =========================================================================
    priority += GetRolePriorityBonus(unit);

    // =========================================================================
    // Factor 3: Health Trend (rapidly dropping health)
    // =========================================================================
    priority += GetHealthTrendPenalty(unit);

    // =========================================================================
    // Factor 4: Distance Penalty
    // =========================================================================
    float distance = bot->GetDistance2d(unit);
    if (distance > sPlayerbotAIConfig->healDistance)
    {
        // Out of optimal range - significant penalty
        priority += 25.0f;
    }
    else
    {
        // Small distance penalty within range (0-5 based on distance)
        priority += (distance / sPlayerbotAIConfig->healDistance) * 5.0f;
    }

    // =========================================================================
    // Factor 5: Critical Health Threshold Boost
    // =========================================================================
    if (currentHealth < sPlayerbotAIConfig->criticalHealth)
    {
        priority -= 25.0f;  // Critical health - big boost
    }
    else if (currentHealth < sPlayerbotAIConfig->lowHealth)
    {
        priority -= 10.0f;  // Low health - moderate boost
    }

    return priority;
}

float PartyMemberToHeal::GetRolePriorityBonus(Unit* unit)
{
    // Check if it's a player
    Player* player = unit->ToPlayer();
    if (!player)
        return 0.0f;  // Non-players (pets) get no role bonus

    // Tank: highest priority (-20)
    if (botAI->IsTank(player))
        return -20.0f;

    // Healer: second priority (-10) - keeping healers alive is critical
    if (botAI->IsHeal(player))
        return -10.0f;

    // DPS: baseline (0)
    return 0.0f;
}

uint32 PartyMemberToHeal::GetIncomingDamage(Unit* unit)
{
    if (!unit)
        return 0;

    // Use the AnticipatoryThreat system to get incoming damage
    uint32 totalDamage = 0;

    // Check for active casts targeting this unit
    for (auto& ref : unit->getHostileRefMgr())
    {
        Unit* enemy = ref.GetSource()->GetOwner();
        if (!enemy || !enemy->IsAlive())
            continue;

        // Check current spell
        Spell* spell = enemy->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = enemy->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (spell)
        {
            ObjectGuid targetGuid = spell->m_targets.GetUnitTargetGUID();

            // Check if targeting this unit or is AOE
            bool isTargeting = (targetGuid == unit->GetGUID());
            bool isAoe = spell->GetSpellInfo()->IsAffectingArea();

            if (isTargeting || (isAoe && targetGuid.IsEmpty()))
            {
                // Look up in boss ability database
                const BossAbilityData* ability = sAnticipatoryThreat->GetAbilityBySpell(spell->GetSpellInfo()->Id);
                if (ability)
                {
                    totalDamage += ability->baseDamage;
                }
                else
                {
                    // Estimate from spell info
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

    // Also consider periodic damage auras
    Unit::AuraApplicationMap const& auras = unit->GetAppliedAuras();
    for (auto const& [spellId, auraApp] : auras)
    {
        Aura* aura = auraApp->GetBase();
        if (!aura)
            continue;

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            AuraEffect* effect = aura->GetEffect(i);
            if (!effect)
                continue;

            if (effect->GetAuraType() == SPELL_AURA_PERIODIC_DAMAGE ||
                effect->GetAuraType() == SPELL_AURA_PERIODIC_LEECH)
            {
                // Consider next 3 seconds of periodic damage
                int32 damage = effect->GetAmount();
                uint32 amplitude = effect->GetAmplitude();
                if (amplitude > 0)
                {
                    uint32 ticksIn3Sec = (3000 / amplitude) + 1;
                    totalDamage += damage * ticksIn3Sec;
                }
            }
        }
    }

    return totalDamage;
}

float PartyMemberToHeal::GetHealthTrendPenalty(Unit* unit)
{
    if (!unit)
        return 0.0f;

    uint64 guid = unit->GetGUID().GetRawValue();
    auto it = s_healthTrends.find(guid);

    if (it == s_healthTrends.end())
        return 0.0f;

    const HealthTrendData& trend = it->second;

    // Need at least 2 samples for meaningful trend
    if (trend.sampleCount < 2)
        return 0.0f;

    // Health change rate is negative when taking damage
    // Convert to penalty: fast damage intake = negative penalty (priority boost)
    // healthChangeRate is in HP/second, convert to % of max health
    float maxHealth = static_cast<float>(unit->GetMaxHealth());
    if (maxHealth <= 0)
        return 0.0f;

    float changeRatePct = (trend.healthChangeRate / maxHealth) * 100.0f;

    // Rapid health loss (< -10% per second) = priority boost up to -15
    // Stable health = no bonus
    // Health recovering = small penalty (they're being healed already)
    if (changeRatePct < -20.0f)
        return -15.0f;  // Very fast damage intake
    else if (changeRatePct < -10.0f)
        return -10.0f;  // Fast damage intake
    else if (changeRatePct < -5.0f)
        return -5.0f;   // Moderate damage intake
    else if (changeRatePct > 5.0f)
        return 5.0f;    // Being healed, lower priority
    else
        return 0.0f;    // Stable
}

void PartyMemberToHeal::UpdateHealthTrend(Unit* unit)
{
    if (!unit)
        return;

    uint64 guid = unit->GetGUID().GetRawValue();
    uint32 now = getMSTime();
    uint32 currentHealth = unit->GetHealth();
    uint32 maxHealth = unit->GetMaxHealth();

    HealthTrendData& trend = s_healthTrends[guid];

    // Calculate time delta
    uint32 timeDelta = now - trend.lastUpdateTime;

    // Only update if enough time has passed (100ms minimum to avoid noise)
    if (timeDelta >= 100 && trend.lastUpdateTime > 0)
    {
        // Calculate health change rate (HP per second)
        int32 healthDelta = static_cast<int32>(currentHealth) - static_cast<int32>(trend.lastHealth);
        float newRate = (static_cast<float>(healthDelta) / static_cast<float>(timeDelta)) * 1000.0f;

        // Exponential moving average to smooth out spikes
        // Weight new samples more heavily when we have few samples
        float alpha = (trend.sampleCount < 5) ? 0.5f : 0.3f;
        trend.healthChangeRate = (alpha * newRate) + ((1.0f - alpha) * trend.healthChangeRate);

        // Track damage intake (only negative health changes)
        if (healthDelta < 0)
        {
            float damageRate = -newRate;
            trend.avgDamageIntake = (alpha * damageRate) + ((1.0f - alpha) * trend.avgDamageIntake);
        }

        if (trend.sampleCount < 255)
            trend.sampleCount++;
    }

    // Update stored values
    trend.lastHealth = currentHealth;
    trend.lastMaxHealth = maxHealth;
    trend.lastUpdateTime = now;

    // Clean up stale entries (not seen in 30 seconds)
    // Do this occasionally to avoid memory buildup
    static uint32 lastCleanup = 0;
    if (now - lastCleanup > 30000)
    {
        lastCleanup = now;
        for (auto it = s_healthTrends.begin(); it != s_healthTrends.end(); )
        {
            if (now - it->second.lastUpdateTime > 30000)
                it = s_healthTrends.erase(it);
            else
                ++it;
        }
    }
}

bool PartyMemberToHeal::Check(Unit* player)
{
    // return player && player != bot && player->GetMapId() == bot->GetMapId() && player->IsInWorld() &&
    //     sServerFacade->GetDistance2d(bot, player) < (player->IsPlayer() && botAI->IsTank((Player*)player) ? 50.0f
    //     : 40.0f);
    return player->GetMapId() == bot->GetMapId() && !player->IsCharmed() &&
           bot->GetDistance2d(player) < sPlayerbotAIConfig->healDistance * 2 && bot->IsWithinLOSInMap(player);
}

Unit* PartyMemberToProtect::Calculate()
{
    return nullptr;
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    std::vector<Unit*> needProtect;

    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (GuidVector::iterator i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit)
            continue;

        Unit* pVictim = unit->GetVictim();
        if (!pVictim || !pVictim->IsPlayer())
            continue;

        if (pVictim == bot)
            continue;

        float attackDistance = 30.0f;
        if (sServerFacade->GetDistance2d(pVictim, unit) > attackDistance)
            continue;

        if (botAI->IsTank((Player*)pVictim) && pVictim->GetHealthPct() > 10)
            continue;
        else if (pVictim->GetHealthPct() > 30)
            continue;

        if (find(needProtect.begin(), needProtect.end(), pVictim) == needProtect.end())
            needProtect.push_back(pVictim);
    }

    if (needProtect.empty())
        return nullptr;

    sort(needProtect.begin(), needProtect.end(), compareByHealth);

    return needProtect[0];
}
