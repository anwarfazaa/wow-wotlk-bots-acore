/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "DungeonChatter.h"
#include "Player.h"
#include "Random.h"
#include "Timer.h"

DungeonChatter* DungeonChatter::instance()
{
    static DungeonChatter instance;
    return &instance;
}

DungeonChatter::DungeonChatter()
{
}

void DungeonChatter::Initialize()
{
    if (m_initialized)
        return;

    LoadChatterLines();
    m_initialized = true;
}

void DungeonChatter::LoadChatterLines()
{
    // =========================================================================
    // ENTERING DUNGEON - First impressions
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::ENTERING_DUNGEON)] = {
        "Ah yes, another dark hole in the ground. My favorite.",
        "I have a good feeling about this place. And I'm usually wrong.",
        "This dungeon smells like adventure. And also feet.",
        "My grandmother told me never to go into strange dungeons with strangers. Here I am.",
        "If I die in here, tell my bank alt I loved them.",
        "I once cleared this place blindfolded. I was also lost for three hours.",
        "The architecture here is stunning. Stunning how anyone could build something this ugly.",
        "Is it just me, or does every dungeon look the same after a while?",
        "I brought snacks. Nobody asked, but I did.",
        "My horoscope said today was a good day to stay home. I should have listened.",
        "Who decorated this place? Evil overlords have no taste.",
        "I wonder if the boss has a suggestion box. I have notes.",
        "Time to make some poor life decisions together!",
        "Remember, we're professionals. We do stupid things professionally.",
        "I hope there's a gift shop at the end.",
        "My therapist said I should face my fears. I don't think she meant like this.",
        "Let's split up and look for clues! Just kidding, please don't.",
        "According to my calculations, we have a 50% chance of survival. Maybe 40%.",
        "I didn't sign up for this. Actually, I did. What was I thinking?",
        "If anyone asks, we were never here.",
        "Places like this are why I have trust issues.",
        "My insurance doesn't cover dungeon-related injuries.",
        "I smell loot. Also mold. Mostly mold.",
        "Last time I was here, I forgot my way out. Good times.",
        "This is either going to be epic or a disaster. No in-between."
    };

    // =========================================================================
    // BEFORE PULL - Pre-combat tension relief
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::BEFORE_PULL)] = {
        "On a scale of one to dead, how bad is this going to be?",
        "I'll count to three. One... two... good luck everyone!",
        "Remember the plan? Neither do I. Let's wing it.",
        "If this goes wrong, I was never here.",
        "Time to introduce ourselves. Violently.",
        "They don't look so tough. Famous last words.",
        "I've seen scarier things in my mailbox.",
        "Okay, who's volunteering to die first? Not me.",
        "Let's do this! By 'this' I mean survive.",
        "I have a plan. Step one: don't die. That's the whole plan.",
        "Stay calm. Stay focused. Stay behind me.",
        "I'm not saying this will hurt, but it will definitely hurt.",
        "On three we charge. Or run. Depending on how it goes.",
        "My weapon is ready. My body is ready. My mind is questioning everything.",
        "This is the part where heroes are made. Or respawn points.",
        "Let's make this quick. I have dinner plans.",
        "Remember, the tank goes first. That's why we have a tank.",
        "If I start screaming, just know it's a battle cry. Probably.",
        "I believe in us. That's my first mistake today.",
        "Time to earn our repair bills!",
        "May the odds be ever... ah who am I kidding, let's go!",
        "I've trained my whole life for this. Okay, maybe just last Tuesday.",
        "This is going to be legendary. Or a learning experience.",
        "Deep breaths everyone. Except the tank. Tank doesn't need air.",
        "I'm ready to make some questionable decisions!"
    };

    // =========================================================================
    // AFTER KILL - Victory commentary
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::AFTER_KILL)] = {
        "And that's how it's done. Mostly.",
        "We did it! I mean, I did it. We helped.",
        "Another one bites the dust. Classic.",
        "That was almost too easy. I'm suspicious now.",
        "Note to self: we're actually not terrible at this.",
        "They never saw it coming. Neither did I, honestly.",
        "Victory! Time to check their pockets.",
        "Is it wrong that I enjoyed that? Asking for a friend.",
        "One down, probably a million to go.",
        "That went better than my last date.",
        "I'd like to thank my weapon for its service.",
        "See? I told you we could do it. You didn't believe me.",
        "Who needs skill when you have determination? And luck. Lots of luck.",
        "That's going in my journal. 'Today I didn't die.'",
        "We're like a well-oiled machine. A slightly rusty well-oiled machine.",
        "They fought bravely. We fought better.",
        "I felt a little bad for them. A very little.",
        "My ancestors are smiling at me. Or laughing. Hard to tell.",
        "That's what happens when you stand in our way!",
        "Did anyone else see that? I was amazing.",
        "We make a good team. A chaotic, confusing team.",
        "Rest in pieces. Or whatever they do.",
        "I call dibs on the loot. All of it.",
        "That was for my repair bill!",
        "Clean up on aisle everywhere."
    };

    // =========================================================================
    // BOSS PULL - Before the big fight
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::BOSS_PULL)] = {
        "Big boss time. Try not to embarrass us.",
        "This is it. The moment we've been grinding for.",
        "I've seen bigger. I've also seen smaller. This is definitely one of those.",
        "Remember: stand in the safe spots. Or don't. I'm not your mom.",
        "If we wipe, we never speak of this again.",
        "Time to show this boss who's boss. Hint: it's us. Maybe.",
        "I've read the guide. Kind of. The first paragraph.",
        "This is either going to be our finest hour or our last.",
        "Let's make this boss regret spawning here today!",
        "Everyone knows the mechanics, right? RIGHT?",
        "I have a special attack planned. It's called 'trying my best.'",
        "We've got this. We've totally got this. Please say we've got this.",
        "Today, we make history. Or we make corpses. One of those.",
        "This boss doesn't know what's coming. Neither do we, but still.",
        "Focus up, team. This is what we trained for. Sort of.",
        "Big scary boss, meet bigger, scarier us!",
        "Let's give them something to remember. Preferably not our failure.",
        "I didn't come this far to come this far.",
        "Victory or death! Preferably victory!",
        "Time to put the 'fun' in 'fundamentally terrifying.'",
        "We're about to become legends. Or statistics.",
        "I hope this boss has health insurance.",
        "On my mark. Ready... probably not... GO!",
        "Let's turn this boss into a loot piñata!",
        "Remember the strategy. Forget the strategy. Just hit it hard."
    };

    // =========================================================================
    // BOSS KILL - Celebrating victory
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::BOSS_KILL)] = {
        "WE DID IT! I knew we would. I definitely didn't doubt us.",
        "And THAT is why you don't mess with us!",
        "Someone screenshot this moment! My greatest achievement!",
        "This boss thought it was tough. It was wrong.",
        "I'd like to thank the academy, my guild, and especially myself.",
        "That's one for the history books! Or at least my diary.",
        "We came, we saw, we conquered. Classic us.",
        "I'm not crying, it's just sweat from the victory.",
        "Boss down! Time for celebration food!",
        "And they said we couldn't do it. Who's 'they'? I don't know, but we showed them!",
        "That was for all the wipes that came before!",
        "Victory tastes sweet. Also my potion. That might be the potion.",
        "We're basically heroes now. Feel free to start the parade.",
        "I've never been more proud of a group of strangers.",
        "This boss clearly underestimated our stubborness.",
        "LOOT TIME! The real boss was the treasure all along.",
        "That's what I call teamwork. Chaotic, beautiful teamwork.",
        "We should do this again. Maybe in a week. I need to recover.",
        "I promised myself I wouldn't get emotional. I lied.",
        "To the victor go the spoils! And the bragging rights!",
        "Tell your friends about us! We're amazing!",
        "Boss defeated! Self-esteem restored!",
        "We're not heroes. We're LEGENDARY heroes.",
        "That boss is going to need therapy after what we did.",
        "Achievement unlocked: We Actually Did It."
    };

    // =========================================================================
    // WIPE - After everyone dies
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::WIPE)] = {
        "Well... that happened.",
        "I blame lag. Always blame lag.",
        "That was a practice run. Official run starts now.",
        "We were just testing the boss's damage. It's high. Very high.",
        "Okay, who forgot to do their job? Besides me.",
        "That went differently in my head.",
        "New plan: don't do what we just did.",
        "We call that a 'learning experience.' Very educational.",
        "The floor was comfortable anyway.",
        "On the bright side... give me a minute, I'll think of something.",
        "My repair bill is going to be legendary.",
        "We almost had it! And by 'almost' I mean not even close.",
        "Let's never speak of this again.",
        "Technical difficulties. Please stand by.",
        "That wasn't a wipe, that was a tactical retreat. To the ground.",
        "Same time next attempt?",
        "I saw my life flash before my eyes. It was mostly dungeons.",
        "We're just making the boss feel confident. Strategy.",
        "Rez please. And my dignity too, if you can.",
        "This is fine. Everything is fine. The fire is fine.",
        "I think we found out what NOT to do.",
        "We got this next time. Or the time after. Eventually.",
        "That was the warm-up wipe. The real wipe comes next.",
        "Our corpses will serve as a warning to... ourselves.",
        "Anyone want to reconsider their life choices together?"
    };

    // =========================================================================
    // LOW HEALTH - Panic mode
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::LOW_HEALTH)] = {
        "I'm going to need some healing over here! Or a miracle!",
        "My health bar is more of a health suggestion right now.",
        "I've seen better days. And better health bars.",
        "Is this the part where I panic? I feel like panicking.",
        "I can feel my ancestors calling me. Please make them stop.",
        "If anyone has a bandage, now would be the time!",
        "I'm fine! This is fine! Nothing is fine!",
        "My health pot is on cooldown and so is my will to live.",
        "A little help? A lot of help? Any help?",
        "I'm not dying, I'm just aggressively resting.",
        "Tell my bank I loved my gold!",
        "My screen is getting red. That's bad, right?",
        "I believe in healing! Please believe in healing me!",
        "This is just a scratch. A very deep, life-threatening scratch.",
        "Remember me as I was! Young, foolish, and not dead!",
        "I should have brought more potions.",
        "My life is flashing before my eyes. It needs better graphics.",
        "Is there a healer in the house?! Anyone?!",
        "I'm holding on by a thread. A very thin thread.",
        "Might want to speed up those heals, just saying!"
    };

    // =========================================================================
    // LOW MANA - Caster problems
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::LOW_MANA)] = {
        "Running low on mana. And patience.",
        "My mana bar looks like my bank account. Empty.",
        "I need to drink. The blue stuff, not the fun stuff.",
        "Out of mana. I'll just judge from here.",
        "If only I could heal with good intentions.",
        "My magic well has run dry. Give me a moment.",
        "Brain juice depleted. Please stand by.",
        "Mana break or I start throwing rocks instead.",
        "I'm not useless, I'm just mana-challenged right now.",
        "The spirits are willing but the mana is gone.",
        "Someone should have told me this dungeon would be long.",
        "Mana low. Sass still high.",
        "I need a drink. The glowing blue kind.",
        "My spells now cost hopes and dreams. I'm out of both.",
        "Time to sit and contemplate my mana choices.",
        "The only thing emptier than my mana bar is my patience.",
        "Going to need a mana break unless you want weak spells.",
        "I can either cast or breathe. Not both.",
        "Anyone got water? Magical water? Regular water? Anything?",
        "My mana pool is more of a mana puddle right now."
    };

    // =========================================================================
    // WAITING - During downtime
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::WAITING)] = {
        "So... nice weather we're having in this dungeon.",
        "Anyone else hungry? Just me? Okay.",
        "I spy with my little eye... something dead. It's everything.",
        "While we wait, let me tell you about my fishing skill...",
        "This is the perfect time for a bathroom break. Not me though.",
        "Is it just me or are dungeons getting longer?",
        "I wonder what the boss is doing right now. Probably waiting for us.",
        "Did I leave my stove on? Probably not. Maybe. I hope not.",
        "So, anyone have any hobbies? Besides this?",
        "We should get matching guild tabards after this.",
        "This reminds me of a story. Never mind, no one asked.",
        "I've been thinking about redecorating my bank space.",
        "Anyone else wondering what's for dinner?",
        "Time moves differently in dungeons. Specifically, slower.",
        "I should have brought a book.",
        "We're making good time. By 'good' I mean 'some kind of' time.",
        "This is nice. Group bonding. In a murder cave.",
        "Does anyone else talk to their pets? No? Just me?",
        "I'm not impatient, I'm just efficiently waiting.",
        "The real treasure is the friends we made. Also the actual treasure.",
        "Anyone know any good jokes? No? Fair enough.",
        "I wonder if the mobs respawn while we wait. Let's not find out.",
        "This is the most productive standing around I've ever done.",
        "Let me know when we're doing something. I'll be here.",
        "Patience is a virtue. I'm running low on virtue."
    };

    // =========================================================================
    // RANDOM - General dungeon banter
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::RANDOM)] = {
        "Is it me, or do all dungeons smell like this?",
        "I once got lost in here for three days. Good times.",
        "They should really put up some signs in these places.",
        "Whoever designed this dungeon hates us personally.",
        "I wonder who cleans up after we leave.",
        "My feet hurt. Does anyone else's feet hurt?",
        "I should have worn my other boots.",
        "Does anyone actually know where we're going?",
        "I'm starting to think the treasure isn't worth it. Just kidding, it totally is.",
        "This would make a great vacation spot. If you're into danger.",
        "I've been in worse places. Can't remember where, but I have.",
        "Anyone else feel like we're being watched?",
        "Left or right? I always pick wrong.",
        "My adventure journal is getting quite thick.",
        "I hope there's a shortcut out of here.",
        "Note to self: pack more snacks next time.",
        "Is it me, or is this dungeon judging us?",
        "I should have become a farmer like my mother wanted.",
        "At least the company is good. Well, tolerable.",
        "Every dungeon I enter, I leave a little more traumatized.",
        "I've seen things in dungeons you people wouldn't believe.",
        "The loot better be worth all this walking.",
        "My back is going to hurt tomorrow.",
        "Does this dungeon ever end? Asking for my sanity.",
        "I'm not complaining, but I'm definitely complaining.",
        "Remember when we thought this would be easy?",
        "I should write a book about my dungeon experiences.",
        "The real adventure was the stress we felt along the way.",
        "Anyone else wondering how we got here? Life choices, I mean.",
        "I hope there's a nice warm inn waiting for us."
    };

    // =========================================================================
    // LOOT - When finding treasure
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::LOOT)] = {
        "Ooh, shiny! Mine? Please?",
        "This is why I wake up in the morning. Loot.",
        "I can already feel my inventory crying.",
        "Is it my birthday? Because this feels like presents!",
        "Time to roll need on everything. Just kidding. Maybe.",
        "This is going straight to my bank. Or the auction house.",
        "Finally, some treasure to pay for my therapy!",
        "Loot is the best part of any job.",
        "My precious! Wait, wrong universe.",
        "I wonder how much this is worth. A lot, hopefully.",
        "This boss had great taste in loot.",
        "My bags are full but my heart is fuller.",
        "I didn't come here for the exercise.",
        "Cha-ching! That's the sound of victory.",
        "I'm not greedy, I just have expensive taste.",
        "One person's trash is another person's epic gear.",
        "I'll fight anyone for this. Peacefully, of course.",
        "This is going to look great on me. Or in my bank.",
        "May the RNG gods bless us all.",
        "I've never seen anything more beautiful. Except gold."
    };

    // =========================================================================
    // STUCK - When getting stuck
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::STUCK)] = {
        "I appear to be... architecturally challenged.",
        "This wall and I are having a disagreement.",
        "I'm not stuck, I'm just exploring very locally.",
        "Help! I've fallen and I choose not to get up!",
        "My navigation skills have failed me. Again.",
        "Who put this wall here?! Very inconsiderate.",
        "I'm conducting an experiment. It's called 'being stuck.'",
        "If I stand here long enough, maybe the dungeon will move.",
        "Plot twist: the dungeon is the maze and I'm the mouse.",
        "I've become one with this spot. It's my home now.",
        "Don't mind me, just having a moment with this rock.",
        "Pathing issues? Never heard of them. *is clearly stuck*",
        "I meant to do this. It's called... strategic positioning.",
        "Maybe if I believe hard enough, I'll phase through.",
        "This is what we call a 'character building moment.'",
        "The dungeon is testing my patience. I'm failing.",
        "Going around would have been easier in hindsight.",
        "I'm not lost, I know exactly where I am. I'm stuck.",
        "Whoever made this path owes me an apology.",
        "My legs are working, the geometry is not cooperating."
    };

    // =========================================================================
    // DEATH - When dying
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::DEATH)] = {
        "Tell my gold I loved it...",
        "I regret nothing! Well, maybe that last pull.",
        "This is embarrassing. See you at the graveyard.",
        "That was my plan. To die heroically. Mostly die.",
        "My corpse will mark the spot for others.",
        "I always wanted a dramatic death. Got a silly one instead.",
        "If anyone needs me, I'll be dead over here.",
        "Death is just a setback. An expensive, annoying setback.",
        "At least I don't have to walk anymore.",
        "This is fine. Being dead is fine.",
        "I call this move 'the tactical floor inspection.'",
        "First death of the day! Wait, that's not a good record.",
        "I zigged when I should have zagged.",
        "My epic life ends... like this? Disappointing.",
        "Ground perspective isn't so bad actually.",
        "Res pls. And maybe some dignity while you're at it.",
        "Well, that's going to leave a mark.",
        "Deaths are just unplanned breaks, right? Right?",
        "Note to self: don't stand there next time.",
        "The floor called, it missed me. We're reunited now."
    };

    // =========================================================================
    // RESURRECT - When getting resurrected
    // =========================================================================
    m_chatterLines[static_cast<uint8>(ChatterCategory::RESURRECT)] = {
        "I'm alive! Again! This happens more often than you'd think.",
        "Thanks for the rez! I almost saw a light at the end.",
        "Back from the dead and ready to do it all again!",
        "My near-death experience taught me nothing!",
        "Thank you! I was getting bored being dead.",
        "Resurrection complete! Bad decisions resuming!",
        "You saved me! Now I can die again later!",
        "I appreciate the rez. My repair bill appreciates it less.",
        "I have returned! Did you miss me? I missed me.",
        "Back to life! Death was surprisingly boring.",
        "Thank you, kind healer! I owe you my continued existence!",
        "I've been to the other side. The food was terrible.",
        "Resurrected and ready to make more mistakes!",
        "That was a nice nap. Where were we?",
        "I'm back! And I haven't learned anything!",
        "Thank you! I'll try to stay alive longer this time.",
        "Second chance! I promise to waste it wisely.",
        "Death couldn't hold me! Neither could common sense, apparently.",
        "Thanks for bringing me back! The graveyard was boring.",
        "Alive again! The adventure continues! Poorly!"
    };
}

std::string DungeonChatter::GetChatter(ChatterCategory category, Player* bot)
{
    if (!m_initialized)
        Initialize();

    uint8 cat = static_cast<uint8>(category);
    auto it = m_chatterLines.find(cat);
    if (it == m_chatterLines.end() || it->second.empty())
        return "";

    const auto& lines = it->second;
    uint32 index = urand(0, lines.size() - 1);
    return lines[index];
}

bool DungeonChatter::ShouldChatter(Player* bot, ChatterCategory category)
{
    if (!bot)
        return false;

    if (!m_initialized)
        Initialize();

    uint64 guid = bot->GetGUID().GetCounter();
    uint32 now = getMSTime();

    // Check cooldown
    auto it = m_lastChatterTime.find(guid);
    if (it != m_lastChatterTime.end())
    {
        uint32 elapsed = getMSTimeDiff(it->second, now);
        // Add some randomness to cooldown
        uint32 cooldown = urand(m_minCooldownMs, m_maxCooldownMs);
        if (elapsed < cooldown)
            return false;
    }

    // Roll for chance
    return urand(1, 100) <= m_chatterChance;
}

void DungeonChatter::RecordChatter(Player* bot)
{
    if (!bot)
        return;

    m_lastChatterTime[bot->GetGUID().GetCounter()] = getMSTime();
}
