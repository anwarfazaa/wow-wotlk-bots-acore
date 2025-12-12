/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "GenericTriggers.h"

#include <string>

#include "BattlegroundWS.h"
#include "CreatureAI.h"
#include "GameTime.h"
#include "IntentBroadcaster.h"
#include "ItemVisitors.h"
#include "LastSpellCastValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Player.h"
#include "PositionValue.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "ThreatMgr.h"
#include "Timer.h"

bool LowManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig->lowMana;
}

bool MediumManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig->mediumMana;
}

bool NoPetTrigger::IsActive()
{
    return (bot->GetMinionGUID().IsEmpty()) && (!AI_VALUE(Unit*, "pet target")) && (!bot->GetGuardianPet()) &&
           (!bot->GetFirstControlled()) && (!AI_VALUE2(bool, "mounted", "self target"));
}

bool HasPetTrigger::IsActive()
{
    return (AI_VALUE(Unit*, "pet target")) && !AI_VALUE2(bool, "mounted", "self target");
    ;
}

bool PetAttackTrigger::IsActive()
{
    Guardian* pet = bot->GetGuardianPet();
    if (!pet)
    {
        return false;
    }
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
    {
        return false;
    }
    if (pet->GetVictim() == target && pet->GetCharmInfo()->IsCommandAttack())
    {
        return false;
    }
    if (bot->GetMap()->IsDungeon() && bot->GetGroup() && !target->IsInCombat())
    {
        return false;
    }
    return true;
}

bool HighManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") && AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig->highMana;
}

bool AlmostFullManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") && AI_VALUE2(uint8, "mana", "self target") > 85;
}

bool EnoughManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") && AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig->highMana;
}

bool RageAvailable::IsActive() { return AI_VALUE2(uint8, "rage", "self target") >= amount; }

bool EnergyAvailable::IsActive() { return AI_VALUE2(uint8, "energy", "self target") >= amount; }

bool ComboPointsAvailableTrigger::IsActive() { return AI_VALUE2(uint8, "combo", "current target") >= amount; }

bool ComboPointsNotFullTrigger::IsActive() { return AI_VALUE2(uint8, "combo", "current target") < amount; }

bool TargetWithComboPointsLowerHealTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !target->IsInWorld())
    {
        return false;
    }
    return ComboPointsAvailableTrigger::IsActive() &&
           (target->GetHealth() / AI_VALUE(float, "estimated group dps")) <= lifeTime;
}

bool LoseAggroTrigger::IsActive() { return !AI_VALUE2(bool, "has aggro", "current target"); }

bool HasAggroTrigger::IsActive() { return AI_VALUE2(bool, "has aggro", "current target"); }

bool PanicTrigger::IsActive()
{
    return AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig->criticalHealth &&
           (!AI_VALUE2(bool, "has mana", "self target") ||
            AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig->lowMana);
}

bool OutNumberedTrigger::IsActive()
{
    if (bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsRaid()))
        return false;

    if (bot->GetGroup() && bot->GetGroup()->isRaidGroup())
        return false;

    int32 botLevel = bot->GetLevel();
    uint32 friendPower = 200;
    uint32 foePower = 0;
    for (auto& attacker : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
    {
        Creature* creature = botAI->GetCreature(attacker);
        if (!creature)
            continue;

        int32 dLevel = creature->GetLevel() - botLevel;
        if (dLevel > -10)
            foePower = std::max(100 + 10 * dLevel, dLevel * 200);
    }

    if (!foePower)
        return false;

    for (auto& helper : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
    {
        Unit* player = botAI->GetUnit(helper);
        if (!player || player == bot)
            continue;

        int32 dLevel = player->GetLevel() - botLevel;

        if (dLevel > -10 && bot->GetDistance(player) < 10.0f)
            friendPower += std::max(200 + 20 * dLevel, dLevel * 200);
    }

    return friendPower < foePower;
}

bool BuffTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;
    if (!SpellTrigger::IsActive())
        return false;
    Aura* aura = botAI->GetAura(spell, target, checkIsOwner, checkDuration);
    if (!aura)
        return true;
    if (beforeDuration && aura->GetDuration() < static_cast<int32>(beforeDuration))
        return true;
    return false;
}

// ============================================================================
// SmartBuffRefreshTrigger Implementation
// ============================================================================

namespace BuffRefresh
{
    // Duration thresholds (in ms)
    constexpr uint32 SHORT_BUFF_THRESHOLD = 30000;      // 30 seconds
    constexpr uint32 MEDIUM_BUFF_THRESHOLD = 300000;    // 5 minutes

    // Refresh percentages (as decimal)
    constexpr float SHORT_BUFF_REFRESH_PCT = 0.20f;     // 20% remaining
    constexpr float MEDIUM_BUFF_REFRESH_PCT = 0.15f;    // 15% remaining
    constexpr float LONG_BUFF_REFRESH_PCT = 0.10f;      // 10% remaining

    // Minimum refresh times (in ms)
    constexpr uint32 MEDIUM_BUFF_MIN_REFRESH = 5000;    // 5 seconds
    constexpr uint32 LONG_BUFF_MIN_REFRESH = 30000;     // 30 seconds

    // Maximum refresh threshold - don't refresh above this % remaining
    constexpr float MAX_REFRESH_THRESHOLD = 0.80f;      // 80% remaining
    constexpr float COMBAT_CRITICAL_THRESHOLD = 0.50f;  // 50% for critical buffs in combat
}

bool SmartBuffRefreshTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    if (!SpellTrigger::IsActive())
        return false;

    // Check if buff exists
    Aura* aura = botAI->GetAura(spell, target, true, true);
    if (!aura)
        return true;  // No buff = definitely apply it

    // Get buff timing info
    int32 duration = aura->GetDuration();
    int32 maxDuration = aura->GetMaxDuration();

    if (maxDuration <= 0)
        return false;  // Permanent buff, don't refresh

    // Calculate remaining percentage
    float remainingPct = static_cast<float>(duration) / static_cast<float>(maxDuration);

    // Never refresh if above max threshold (too much time remaining)
    if (remainingPct > BuffRefresh::MAX_REFRESH_THRESHOLD)
    {
        // Exception: critical buff in combat
        if (m_isCriticalBuff && bot->IsInCombat() &&
            remainingPct < BuffRefresh::COMBAT_CRITICAL_THRESHOLD)
        {
            // Allow early refresh of critical buffs in combat
        }
        else
        {
            return false;
        }
    }

    // Calculate optimal refresh window
    uint32 refreshWindow = CalculateRefreshWindow(static_cast<uint32>(maxDuration));

    // Check if we're in the refresh window
    return duration < static_cast<int32>(refreshWindow);
}

uint32 SmartBuffRefreshTrigger::CalculateRefreshWindow(uint32 maxDuration) const
{
    uint32 refreshWindow;

    if (maxDuration < BuffRefresh::SHORT_BUFF_THRESHOLD)
    {
        // Short buff: refresh at 20% remaining
        refreshWindow = static_cast<uint32>(maxDuration * BuffRefresh::SHORT_BUFF_REFRESH_PCT);
    }
    else if (maxDuration < BuffRefresh::MEDIUM_BUFF_THRESHOLD)
    {
        // Medium buff: refresh at 15% remaining, minimum 5 seconds
        refreshWindow = static_cast<uint32>(maxDuration * BuffRefresh::MEDIUM_BUFF_REFRESH_PCT);
        if (refreshWindow < BuffRefresh::MEDIUM_BUFF_MIN_REFRESH)
            refreshWindow = BuffRefresh::MEDIUM_BUFF_MIN_REFRESH;
    }
    else
    {
        // Long buff: refresh at 10% remaining, minimum 30 seconds
        refreshWindow = static_cast<uint32>(maxDuration * BuffRefresh::LONG_BUFF_REFRESH_PCT);
        if (refreshWindow < BuffRefresh::LONG_BUFF_MIN_REFRESH)
            refreshWindow = BuffRefresh::LONG_BUFF_MIN_REFRESH;
    }

    // In combat, refresh earlier for critical buffs
    if (m_isCriticalBuff && bot->IsInCombat())
    {
        refreshWindow = static_cast<uint32>(refreshWindow * 1.5f);
    }

    // Consider cast time of the spell
    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    if (spellId)
    {
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (spellInfo)
        {
            uint32 castTime = spellInfo->CalcCastTime();
            // Add cast time to refresh window so we start casting before buff falls off
            refreshWindow += castTime;
        }
    }

    return refreshWindow;
}

Value<Unit*>* BuffOnPartyTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", spell);
}

bool ProtectPartyMemberTrigger::IsActive() { return AI_VALUE(Unit*, "party member to protect"); }

Value<Unit*>* DebuffOnAttackerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("attacker without aura", spell);
}

Value<Unit*>* DebuffOnMeleeAttackerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("melee attacker without aura", spell);
}

bool NoAttackersTrigger::IsActive()
{
    return !AI_VALUE(Unit*, "current target") && AI_VALUE(uint8, "my attacker count") > 0;
}

bool InvalidTargetTrigger::IsActive() { return AI_VALUE2(bool, "invalid target", "current target"); }

bool NoTargetTrigger::IsActive() { return !AI_VALUE(Unit*, "current target"); }

bool MyAttackerCountTrigger::IsActive()
{
    return AI_VALUE2(bool, "combat", "self target") && AI_VALUE(uint8, "my attacker count") >= amount;
}

bool MediumThreatTrigger::IsActive()
{
    if (!AI_VALUE(Unit*, "main tank"))
        return false;
    return MyAttackerCountTrigger::IsActive();
}

bool LowTankThreatTrigger::IsActive()
{
    Unit* mt = AI_VALUE(Unit*, "main tank");
    if (!mt)
        return false;

    Unit* current_target = AI_VALUE(Unit*, "current target");
    if (!current_target)
        return false;

    ThreatMgr& mgr = current_target->GetThreatMgr();
    float threat = mgr.GetThreat(bot);
    float tankThreat = mgr.GetThreat(mt);
    return tankThreat == 0.0f || threat > tankThreat * 0.5f;
}

bool AoeTrigger::IsActive()
{
    Unit* current_target = AI_VALUE(Unit*, "current target");
    if (!current_target)
    {
        return false;
    }
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    int attackers_count = 0;
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;
        if (unit->GetDistance(current_target->GetPosition()) <= range)
        {
            attackers_count++;
        }
    }
    return attackers_count >= amount;
}

bool NoFoodTrigger::IsActive()
{
    bool isRandomBot = sRandomPlayerbotMgr->IsRandomBot(bot);
    if (isRandomBot && botAI->HasCheat(BotCheatMask::food))
        return false;

    return AI_VALUE2(std::vector<Item*>, "inventory items", "conjured food").empty();
}

bool NoDrinkTrigger::IsActive()
{
    bool isRandomBot = sRandomPlayerbotMgr->IsRandomBot(bot);
    if (isRandomBot && botAI->HasCheat(BotCheatMask::food))
        return false;

    return AI_VALUE2(std::vector<Item*>, "inventory items", "conjured water").empty();
}

bool TargetInSightTrigger::IsActive() { return AI_VALUE(Unit*, "grind target"); }

bool DebuffTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
    {
        return false;
    }
    return BuffTrigger::IsActive() && (target->GetHealth() / AI_VALUE(float, "estimated group dps")) >= needLifeTime;
}

bool DebuffOnBossTrigger::IsActive()
{
    if (!DebuffTrigger::IsActive())
    {
        return false;
    }
    Creature* c = GetTarget()->ToCreature();
    return c && ((c->IsDungeonBoss()) || (c->isWorldBoss()));
}

bool SpellTrigger::IsActive() { return GetTarget(); }

bool SpellCanBeCastTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && botAI->CanCastSpell(spell, target);
}

bool SpellNoCooldownTrigger::IsActive()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", name);
    if (!spellId)
        return false;

    return !bot->HasSpellCooldown(spellId);
}

bool SpellCooldownTrigger::IsActive()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", name);
    if (!spellId)
        return false;

    return bot->HasSpellCooldown(spellId);
}

RandomTrigger::RandomTrigger(PlayerbotAI* botAI, std::string const name, int32 probability)
    : Trigger(botAI, name), probability(probability), lastCheck(getMSTime())
{
}

bool RandomTrigger::IsActive()
{
    if (getMSTime() - lastCheck < sPlayerbotAIConfig->repeatDelay)
        return false;

    lastCheck = getMSTime();
    int32 k = (int32)(probability / sPlayerbotAIConfig->randomChangeMultiplier);
    if (k < 1)
        k = 1;
    return (rand() % k) == 0;
}

bool AndTrigger::IsActive() { return ls && rs && ls->IsActive() && rs->IsActive(); }

std::string const AndTrigger::getName()
{
    std::string name(ls->getName());
    name = name + " and ";
    name = name + rs->getName();
    return name;
}

bool TwoTriggers::IsActive()
{
    if (name1.empty() || name2.empty())
        return false;

    Trigger* trigger1 = botAI->GetAiObjectContext()->GetTrigger(name1);
    Trigger* trigger2 = botAI->GetAiObjectContext()->GetTrigger(name2);

    if (!trigger1 || !trigger2)
        return false;

    return trigger1->IsActive() && trigger2->IsActive();
}

std::string const TwoTriggers::getName()
{
    std::string name;
    name = name1 + " and " + name2;
    return name;
}

bool BoostTrigger::IsActive()
{
    if (!BuffTrigger::IsActive())
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->ToPlayer())
        return true;
    return AI_VALUE(uint8, "balance") <= balance;
}

bool GenericBoostTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->ToPlayer())
        return true;
    return AI_VALUE(uint8, "balance") <= balance;
}

bool HealerShouldAttackTrigger::IsActive()
{
    // nobody can help me
    if (botAI->GetNearGroupMemberCount(sPlayerbotAIConfig->sightDistance) <= 1)
        return true;

    if (AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->almostFullHealth)
        return false;

    // special check for resto druid (dont remove tree of life frequently)
    if (bot->GetAura(33891))
    {
        LastSpellCast& lastSpell = botAI->GetAiObjectContext()->GetValue<LastSpellCast&>("last spell cast")->Get();
        if (lastSpell.timer + 5 > time(nullptr))
            return false;
    }

    int manaThreshold;
    int balance = AI_VALUE(uint8, "balance");
    // higher threshold in higher pressure
    if (balance <= 50)
        manaThreshold = 85;
    else if (balance <= 100)
        manaThreshold = sPlayerbotAIConfig->highMana;
    else
        manaThreshold = sPlayerbotAIConfig->mediumMana;

    if (AI_VALUE2(bool, "has mana", "self target") && AI_VALUE2(uint8, "mana", "self target") < manaThreshold)
        return false;

    return true;
}

bool ItemCountTrigger::IsActive() { return AI_VALUE2(uint32, "item count", item) < count; }

bool InterruptSpellTrigger::IsActive()
{
    return SpellTrigger::IsActive() && botAI->IsInterruptableSpellCasting(GetTarget(), getName());
}

// ============================================================================
// CoordinatedInterruptTrigger Implementation
// ============================================================================

bool CoordinatedInterruptTrigger::IsActive()
{
    if (!SpellTrigger::IsActive())
        return false;

    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
        return false;

    // Check if target is casting
    Spell* currentSpell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!currentSpell)
        currentSpell = target->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

    if (!currentSpell)
        return false;

    uint32 spellId = currentSpell->GetSpellInfo()->Id;

    // Check if we can interrupt this spell
    if (!botAI->IsInterruptableSpellCasting(target, getName()))
        return false;

    // Get interrupt priority - skip very low priority spells
    uint8 priority = GetInterruptPriority(target, spellId);
    if (priority < 25)
        return false;

    // Check if this bot should be the one to interrupt
    if (!ShouldThisBotInterrupt(target, spellId))
        return false;

    // Claim the interrupt and proceed
    return ClaimInterrupt(target, spellId);
}

uint8 CoordinatedInterruptTrigger::GetInterruptPriority(Unit* target, uint32 spellId) const
{
    if (!target)
        return 0;

    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 25;  // Unknown spell - low priority

    // Critical priority: Mass CC, wipe mechanics
    if (IsCrowdControlSpell(spellId))
    {
        // Mass CC (affects multiple targets) = critical
        if (spellInfo->IsAffectingArea())
            return 100;
        return 75;
    }

    // High priority: Healing spells
    if (IsHealingSpell(spellId))
    {
        // Big heals are more important to interrupt
        // Check if it's a significant heal by looking at cast time
        if (spellInfo->CalcCastTime() >= 2000)
            return 85;
        return 70;
    }

    // High priority: Dangerous damage spells
    if (IsDangerousDamageSpell(spellId))
        return 80;

    // Medium priority: Long cast time spells
    if (spellInfo->CalcCastTime() >= 3000)
        return 50;

    // Medium priority: AOE spells
    if (spellInfo->IsAffectingArea())
        return 45;

    // Low priority: Everything else
    return 25;
}

bool CoordinatedInterruptTrigger::ShouldThisBotInterrupt(Unit* target, uint32 spellId) const
{
    // Check if interrupt is already claimed by another bot
    if (sIntentBroadcaster->IsInterruptClaimed(target->GetGUID(), spellId))
    {
        // Someone else is handling it
        return false;
    }

    // Check if our interrupt is on cooldown
    uint32 mySpellId = AI_VALUE2(uint32, "spell id", spell);
    if (!mySpellId)
        return false;

    if (!botAI->CanCastSpell(mySpellId, target, true))
        return false;

    // Get remaining cast time of target
    Spell* targetSpell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!targetSpell)
        targetSpell = target->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

    if (!targetSpell)
        return false;

    int32 remainingCastTime = targetSpell->GetCastTime() - targetSpell->GetTimer();

    // Don't try to interrupt if cast is almost complete (< 300ms)
    if (remainingCastTime < 300)
        return false;

    // If cast time is short, we need to act fast - be less picky
    if (remainingCastTime < 1500)
        return true;

    // For longer casts, just return true if we can do it
    return true;
}

bool CoordinatedInterruptTrigger::IsHealingSpell(uint32 spellId) const
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL_MAX_HEALTH ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL_MECHANICAL ||
            spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_PERIODIC_HEAL)
        {
            return true;
        }
    }

    return false;
}

bool CoordinatedInterruptTrigger::IsCrowdControlSpell(uint32 spellId) const
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        uint32 aura = spellInfo->Effects[i].ApplyAuraName;
        if (aura == SPELL_AURA_MOD_STUN ||
            aura == SPELL_AURA_MOD_FEAR ||
            aura == SPELL_AURA_MOD_CONFUSE ||
            aura == SPELL_AURA_MOD_CHARM ||
            aura == SPELL_AURA_MOD_SILENCE ||
            aura == SPELL_AURA_MOD_PACIFY ||
            aura == SPELL_AURA_MOD_ROOT ||
            aura == SPELL_AURA_TRANSFORM)
        {
            return true;
        }
    }

    return false;
}

bool CoordinatedInterruptTrigger::IsDangerousDamageSpell(uint32 spellId) const
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    // Check if it's a damage spell
    bool isDamage = false;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_SCHOOL_DAMAGE ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_WEAPON_DAMAGE ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_NORMALIZED_WEAPON_DMG)
        {
            isDamage = true;
            break;
        }
    }

    if (!isDamage)
        return false;

    // Consider it dangerous if:
    // - It's AOE
    // - It has a long cast time (charged up)
    if (spellInfo->IsAffectingArea())
        return true;

    if (spellInfo->CalcCastTime() >= 2500)
        return true;

    return false;
}

bool CoordinatedInterruptTrigger::ClaimInterrupt(Unit* target, uint32 spellId)
{
    if (!target || !bot)
        return false;

    // Broadcast our intent to interrupt
    return sIntentBroadcaster->BroadcastInterruptIntent(
        bot->GetGUID(),
        target->GetGUID(),
        spellId,
        2000  // 2 second claim duration
    );
}

bool DeflectSpellTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    if (!target->IsNonMeleeSpellCast(true))
        return false;

    if (target->GetTarget() != bot->GetGUID())
        return false;

    uint32 spellid = context->GetValue<uint32>("spell id", spell)->Get();
    if (!spellid)
        return false;

    SpellInfo const* deflectSpell = sSpellMgr->GetSpellInfo(spellid);
    if (!deflectSpell)
        return false;

    // warrior deflects all
    if (spell == "spell reflection")
        return true;

    // human priest feedback
    if (spell == "feedback")
        return true;

    SpellSchoolMask deflectSchool = SpellSchoolMask(deflectSpell->Effects[EFFECT_0].MiscValue);
    SpellSchoolMask attackSchool = SPELL_SCHOOL_MASK_NONE;

    if (Spell* spell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        if (SpellInfo const* tarSpellInfo = spell->GetSpellInfo())
        {
            attackSchool = tarSpellInfo->GetSchoolMask();
            if (deflectSchool == attackSchool)
                return true;
        }
    }
    return false;
}

bool AttackerCountTrigger::IsActive() { return AI_VALUE(uint8, "attacker count") >= amount; }

bool HasAuraTrigger::IsActive() { return botAI->HasAura(getName(), GetTarget(), false, false, -1, true); }

bool HasAuraStackTrigger::IsActive()
{
    Aura* aura = botAI->GetAura(getName(), GetTarget(), false, true, stack);
    // sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "HasAuraStackTrigger::IsActive %s %d", getName(), aura ?
    // aura->GetStackAmount() : -1);
    return aura;
}

bool TimerTrigger::IsActive()
{
    if (time(nullptr) != lastCheck)
    {
        lastCheck = time(nullptr);
        return true;
    }

    return false;
}

bool TimerBGTrigger::IsActive()
{
    time_t now = time(nullptr);

    if (now - lastCheck >= 60)
    {
        lastCheck = now;
        return true;
    }

    return false;
}

bool HasNoAuraTrigger::IsActive() { return !botAI->HasAura(getName(), GetTarget()); }

bool TankAssistTrigger::IsActive()
{
    if (!AI_VALUE(uint8, "attacker count"))
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (!currentTarget)
        return true;

    Unit* tankTarget = AI_VALUE(Unit*, "tank target");
    if (!tankTarget || currentTarget == tankTarget)
        return false;

    return AI_VALUE2(bool, "has aggro", "current target");
}

bool IsBehindTargetTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && AI_VALUE2(bool, "behind", "current target");
}

bool IsNotBehindTargetTrigger::IsActive()
{
    if (botAI->HasStrategy("stay", botAI->GetState()))
    {
        return false;
    }
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && !AI_VALUE2(bool, "behind", "current target");
}

bool IsNotFacingTargetTrigger::IsActive()
{
    if (botAI->HasStrategy("stay", botAI->GetState()))
    {
        return false;
    }
    return !AI_VALUE2(bool, "facing", "current target");
}

bool HasCcTargetTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "cc target", getName()) && !AI_VALUE2(Unit*, "current cc target", getName());
}

bool NoMovementTrigger::IsActive() { return !AI_VALUE2(bool, "moving", "self target"); }

bool NoPossibleTargetsTrigger::IsActive()
{
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    return !targets.size();
}

bool PossibleAddsTrigger::IsActive() { return AI_VALUE(bool, "possible adds") && !AI_VALUE(ObjectGuid, "pull target"); }

bool NotDpsTargetActiveTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    // do not switch if enemy target
    if (target && target->IsAlive())
    {
        Unit* enemy = AI_VALUE(Unit*, "enemy player target");
        if (target == enemy)
            return false;
    }

    Unit* dps = AI_VALUE(Unit*, "dps target");
    return dps && target != dps;
}

bool NotDpsAoeTargetActiveTrigger::IsActive()
{
    Unit* dps = AI_VALUE(Unit*, "dps aoe target");
    Unit* target = AI_VALUE(Unit*, "current target");
    Unit* enemy = AI_VALUE(Unit*, "enemy player target");

    // do not switch if enemy target
    if (target && target == enemy && target->IsAlive())
        return false;

    return dps && target != dps;
}

bool IsSwimmingTrigger::IsActive() { return AI_VALUE2(bool, "swimming", "self target"); }

bool HasNearestAddsTrigger::IsActive()
{
    GuidVector targets = AI_VALUE(GuidVector, "nearest adds");
    return targets.size();
}

bool HasItemForSpellTrigger::IsActive()
{
    std::string const spell = getName();
    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    return spellId && AI_VALUE2(Item*, "item for spell", spellId);
}

bool TargetChangedTrigger::IsActive()
{
    Unit* oldTarget = context->GetValue<Unit*>("old target")->Get();
    Unit* target = context->GetValue<Unit*>("current target")->Get();
    return target && oldTarget != target;
}

Value<Unit*>* InterruptEnemyHealerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("enemy healer target", spell);
}

bool RandomBotUpdateTrigger::IsActive() { return RandomTrigger::IsActive() && AI_VALUE(bool, "random bot update"); }

bool NoNonBotPlayersAroundTrigger::IsActive()
{
    return !botAI->HasPlayerNearby();
    /*if (!bot->InBattleground())
        return AI_VALUE(GuidVector, "nearest non bot players").empty();

    return false;
    */
}

bool NewPlayerNearbyTrigger::IsActive() { return AI_VALUE(ObjectGuid, "new player nearby"); }

bool CollisionTrigger::IsActive() { return AI_VALUE2(bool, "collision", "self target"); }

bool ReturnToStayPositionTrigger::IsActive()
{
    PositionInfo stayPosition = AI_VALUE(PositionMap&, "position")["stay"];
    if (stayPosition.isSet())
    {
        const float distance = bot->GetDistance(stayPosition.x, stayPosition.y, stayPosition.z);
        return distance > sPlayerbotAIConfig->followDistance;
    }

    return false;
}

bool GiveItemTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "party member without item", item) && AI_VALUE2(uint32, "item count", item);
}

bool GiveFoodTrigger::IsActive()
{
    return AI_VALUE(Unit*, "party member without food") && AI_VALUE2(uint32, "item count", item);
}

bool GiveWaterTrigger::IsActive()
{
    return AI_VALUE(Unit*, "party member without water") && AI_VALUE2(uint32, "item count", item);
}

Value<Unit*>* SnareTargetTrigger::GetTargetValue() { return context->GetValue<Unit*>("snare target", spell); }

bool StayTimeTrigger::IsActive()
{
    time_t stayTime = AI_VALUE(time_t, "stay time");
    time_t now = time(nullptr);
    return delay && stayTime && now > stayTime + 2 * delay / 1000;
}

bool IsMountedTrigger::IsActive() { return AI_VALUE2(bool, "mounted", "self target"); }

bool CorpseNearTrigger::IsActive()
{
    return bot->GetCorpse() && bot->GetCorpse()->IsWithinDistInMap(bot, CORPSE_RECLAIM_RADIUS, true);
}

bool IsFallingTrigger::IsActive() { return bot->HasUnitMovementFlag(MOVEMENTFLAG_FALLING); }

bool IsFallingFarTrigger::IsActive() { return bot->HasUnitMovementFlag(MOVEMENTFLAG_FALLING_FAR); }

bool HasAreaDebuffTrigger::IsActive() { return AI_VALUE2(bool, "has area debuff", "self target"); }

Value<Unit*>* BuffOnMainTankTrigger::GetTargetValue() { return context->GetValue<Unit*>("main tank", spell); }

bool AmmoCountTrigger::IsActive()
{
    if (bot->GetUInt32Value(PLAYER_AMMO_ID) != 0)
        return ItemCountTrigger::IsActive();  // Ammo already equipped

    if (botAI->FindAmmo())
        return true;  // Found ammo in inventory but not equipped

    return ItemCountTrigger::IsActive();
}

bool NewPetTrigger::IsActive()
{
    // Get the bot player object from the AI
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    // Try to get the current pet; initialize guardian and GUID to null/empty
    Pet* pet = bot->GetPet();
    Guardian* guardian = nullptr;
    ObjectGuid currentPetGuid = ObjectGuid::Empty;

    // If bot has a pet, get its GUID
    if (pet)
    {
        currentPetGuid = pet->GetGUID();
    }
    else
    {
        // If no pet, try to get a guardian pet and its GUID
        guardian = bot->GetGuardianPet();
        if (guardian)
            currentPetGuid = guardian->GetGUID();
    }

    // If the current pet or guardian GUID has changed (including becoming empty), reset the trigger state
    if (currentPetGuid != lastPetGuid)
    {
        triggered = false;
        lastPetGuid = currentPetGuid;
    }

    // If there's a valid current pet/guardian (non-empty GUID) and we haven't triggered yet, activate trigger
    if (currentPetGuid != ObjectGuid::Empty && !triggered)
    {
        triggered = true;
        return true;
    }

    // Otherwise, do not activate
    return false;
}

// Static member initialization for ResumeFollowAfterTeleportTrigger
std::unordered_map<uint64, uint32> ResumeFollowAfterTeleportTrigger::s_lastMapId;
std::unordered_map<uint64, bool> ResumeFollowAfterTeleportTrigger::s_wasFollowing;

bool ResumeFollowAfterTeleportTrigger::IsActive()
{
    // Don't do anything if bot is currently teleporting
    if (bot->IsBeingTeleported())
        return false;

    // Don't do anything if bot is not in world
    if (!bot->IsInWorld())
        return false;

    uint64 guid = bot->GetGUID().GetRawValue();
    uint32 currentMapId = bot->GetMapId();
    uint32 lastMapId = s_lastMapId[guid];

    // Detect map change
    bool mapChanged = (lastMapId != 0 && lastMapId != currentMapId);
    s_lastMapId[guid] = currentMapId;

    // Track if we had follow strategy before (for restoration after stay is added)
    bool hasFollow = botAI->HasStrategy("follow", BOT_STATE_NON_COMBAT);
    bool hasStay = botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT);

    // If we have follow and not stay, remember we were following
    if (hasFollow && !hasStay)
    {
        s_wasFollowing[guid] = true;
    }

    // If we have stay but were previously following, check if we should resume
    if (hasStay && s_wasFollowing[guid])
    {
        // Get master or group leader
        Player* master = botAI->GetMaster();
        if (!master)
            master = botAI->GetGroupLeader();

        // If we have a valid master on the same map, we should resume following
        if (master && master != bot && !master->IsBeingTeleported() &&
            master->IsInWorld() && master->GetMapId() == currentMapId)
        {
            // Clear the wasFollowing flag since we're about to resume
            s_wasFollowing[guid] = false;
            return true;
        }
    }

    // Also trigger on map change if we were following
    if (mapChanged && s_wasFollowing[guid])
    {
        Player* master = botAI->GetMaster();
        if (!master)
            master = botAI->GetGroupLeader();

        if (master && master != bot && !master->IsBeingTeleported() &&
            master->IsInWorld() && master->GetMapId() == currentMapId)
        {
            s_wasFollowing[guid] = false;
            return true;
        }
    }

    return false;
}
