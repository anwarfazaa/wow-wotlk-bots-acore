/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "PathfindingBotManager.h"
#include "ExplorationEngine.h"
#include "StuckRecoverySystem.h"
#include "PathLearner.h"
#include "WaypointGenerator.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "WorldSession.h"
#include <sstream>

PathfindingBotManager* PathfindingBotManager::instance()
{
    static PathfindingBotManager instance;
    return &instance;
}

PathfindingBotManager::PathfindingBotManager()
{
    m_explorationEngine = std::make_unique<ExplorationEngine>();
    m_stuckRecovery = std::make_unique<StuckRecoverySystem>();
    m_pathLearner = std::make_unique<PathLearner>();
    m_waypointGenerator = std::make_unique<WaypointGenerator>();
}

PathfindingBotManager::~PathfindingBotManager() = default;

void PathfindingBotManager::Initialize()
{
    LOG_INFO("playerbots", "PathfindingBotManager: Initializing...");

    // Load config from PlayerbotAIConfig
    m_config.enabled = sPlayerbotAIConfig->pathfindingBotEnabled;
    m_config.maxIterations = sPlayerbotAIConfig->pathfindingMaxIterations;
    m_config.stuckThresholdMs = sPlayerbotAIConfig->pathfindingStuckThresholdMs;
    m_config.convergenceIterations = sPlayerbotAIConfig->pathfindingConvergenceIterations;
    m_config.convergenceThreshold = sPlayerbotAIConfig->pathfindingConvergenceThreshold;
    m_config.autoPromoteWaypoints = sPlayerbotAIConfig->pathfindingAutoPromoteWaypoints;
    m_config.minConfidenceForPromotion = sPlayerbotAIConfig->pathfindingMinConfidence;

    LoadFromDatabase();

    LOG_INFO("playerbots", "PathfindingBotManager: Initialization complete");
}

void PathfindingBotManager::LoadFromDatabase()
{
    // Load known dungeon information
    m_dungeonInfo.clear();

    // WotLK Dungeons
    m_dungeonInfo[574] = {"Utgarde Keep", {148.1f, -89.6f, 10.4f, 0.0f}, {23953, 23954, 23980}};      // Keleseth, Skarvald/Dalronn, Ingvar
    m_dungeonInfo[575] = {"Utgarde Pinnacle", {268.1f, -460.2f, 109.5f, 0.0f}, {26668, 26687, 26693, 26861}};
    m_dungeonInfo[576] = {"The Nexus", {162.5f, 13.3f, -15.9f, 0.0f}, {26731, 26763, 26794, 26723}};
    m_dungeonInfo[578] = {"The Oculus", {1040.0f, 998.0f, 527.0f, 0.0f}, {27654, 27447, 27655, 27656}};
    m_dungeonInfo[595] = {"The Culling of Stratholme", {2363.6f, 1404.0f, 128.6f, 0.0f}, {26529, 26530, 26532, 26533}};
    m_dungeonInfo[599] = {"Halls of Stone", {1137.0f, 637.0f, 195.6f, 0.0f}, {27977, 27975, 28234, 27978}};
    m_dungeonInfo[600] = {"Drak'Tharon Keep", {-514.5f, -690.7f, 30.9f, 0.0f}, {26630, 26631, 27483, 26632}};
    m_dungeonInfo[601] = {"Azjol-Nerub", {518.8f, 545.4f, 694.9f, 0.0f}, {28684, 28921, 29120}};
    m_dungeonInfo[602] = {"Halls of Lightning", {1334.0f, -297.0f, 57.3f, 0.0f}, {28586, 28587, 28546, 28923}};
    m_dungeonInfo[604] = {"Gundrak", {1783.9f, 697.2f, 117.8f, 0.0f}, {29304, 29305, 29306, 29307, 29310}};
    m_dungeonInfo[608] = {"The Violet Hold", {1830.5f, 803.1f, 44.3f, 0.0f}, {29315, 29316}};  // Random bosses
    m_dungeonInfo[619] = {"Ahn'kahet: The Old Kingdom", {282.8f, -364.6f, -75.5f, 0.0f}, {29309, 29308, 29310, 29311, 29312}};
    m_dungeonInfo[632] = {"The Forge of Souls", {5644.0f, 2510.0f, 708.6f, 0.0f}, {36497, 36502}};
    m_dungeonInfo[658] = {"Pit of Saron", {5582.0f, 2015.0f, 798.2f, 0.0f}, {36494, 36476, 36658}};
    m_dungeonInfo[668] = {"Halls of Reflection", {5268.0f, 2039.0f, 709.3f, 0.0f}, {38112, 38113}};
    m_dungeonInfo[650] = {"Trial of the Champion", {746.8f, 620.8f, 411.1f, 0.0f}, {35119, 34928, 35451}};

    LOG_INFO("playerbots", "PathfindingBotManager: Loaded {} dungeon definitions", m_dungeonInfo.size());
}

bool PathfindingBotManager::StartPathfinding(Player* bot, uint32 mapId)
{
    if (!bot || !m_config.enabled)
        return false;

    // Check if dungeon is known
    auto dungeonIt = m_dungeonInfo.find(mapId);
    if (dungeonIt == m_dungeonInfo.end())
    {
        LOG_WARN("playerbots", "PathfindingBotManager: Unknown dungeon map {}", mapId);
        return false;
    }

    uint64 guid = bot->GetGUID().GetRawValue();

    std::unique_lock<std::shared_mutex> lock(m_contextMutex);

    // Initialize or reset context
    PathfindingContext& ctx = m_contexts[guid];
    ctx.Reset();
    ctx.mapId = mapId;
    ctx.maxIterations = m_config.maxIterations;
    ctx.isActive = true;
    ctx.entrancePosition = dungeonIt->second.entrance;
    ctx.expectedBosses = dungeonIt->second.bossEntries;
    ctx.totalBossCount = dungeonIt->second.bossEntries.size();

    LOG_INFO("playerbots", "PathfindingBotManager: Starting pathfinding for bot {} in dungeon {} ({})",
             bot->GetName(), dungeonIt->second.name, mapId);

    SetState(bot, PathfindingState::ENTERING);
    return true;
}

void PathfindingBotManager::StopPathfinding(Player* bot)
{
    if (!bot)
        return;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::unique_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end())
    {
        LOG_INFO("playerbots", "PathfindingBotManager: Stopping pathfinding for bot {}", bot->GetName());
        it->second.isActive = false;
        it->second.state = PathfindingState::IDLE;
    }
}

void PathfindingBotManager::PausePathfinding(Player* bot)
{
    if (!bot)
        return;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end() && it->second.isActive)
    {
        it->second.isActive = false;
        LOG_INFO("playerbots", "PathfindingBotManager: Paused pathfinding for bot {}", bot->GetName());
    }
}

void PathfindingBotManager::ResumePathfinding(Player* bot)
{
    if (!bot)
        return;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end() && !it->second.isActive && it->second.state != PathfindingState::IDLE)
    {
        it->second.isActive = true;
        LOG_INFO("playerbots", "PathfindingBotManager: Resumed pathfinding for bot {}", bot->GetName());
    }
}

bool PathfindingBotManager::IsActive(Player* bot) const
{
    if (!bot)
        return false;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    return it != m_contexts.end() && it->second.isActive;
}

PathfindingState PathfindingBotManager::GetState(Player* bot) const
{
    if (!bot)
        return PathfindingState::IDLE;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end())
        return it->second.state;

    return PathfindingState::IDLE;
}

uint32 PathfindingBotManager::GetCurrentIteration(Player* bot) const
{
    if (!bot)
        return 0;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end())
        return it->second.currentIteration;

    return 0;
}

bool PathfindingBotManager::IsConverged(Player* bot) const
{
    if (!bot)
        return false;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end())
        return it->second.state == PathfindingState::COMPLETE;

    return false;
}

float PathfindingBotManager::GetExplorationPercent(Player* bot) const
{
    if (!bot)
        return 0.0f;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end())
        return it->second.explorationPercent;

    return 0.0f;
}

std::string PathfindingBotManager::GetStatusString(Player* bot) const
{
    if (!bot)
        return "No bot";

    const PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return "Not pathfinding";

    std::stringstream ss;
    ss << "Map: " << GetDungeonName(ctx->mapId)
       << " | State: " << GetPathfindingStateName(ctx->state)
       << " | Iteration: " << ctx->currentIteration << "/" << ctx->maxIterations
       << " | Explored: " << static_cast<int>(ctx->explorationPercent * 100) << "%"
       << " | Bosses: " << ctx->bossesKilled.size() << "/" << ctx->totalBossCount
       << " | Deaths: " << ctx->deathCount
       << " | Stuck: " << ctx->stuckCount;

    if (ctx->bestScore > 0.0f)
        ss << " | Best Score: " << ctx->bestScore;

    return ss.str();
}

PathfindingContext* PathfindingBotManager::GetContext(Player* bot)
{
    if (!bot)
        return nullptr;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end())
        return &it->second;

    return nullptr;
}

const PathfindingContext* PathfindingBotManager::GetContext(Player* bot) const
{
    if (!bot)
        return nullptr;

    uint64 guid = bot->GetGUID().GetRawValue();

    std::shared_lock<std::shared_mutex> lock(m_contextMutex);

    auto it = m_contexts.find(guid);
    if (it != m_contexts.end())
        return &it->second;

    return nullptr;
}

void PathfindingBotManager::Update(Player* bot, uint32 diff)
{
    if (!bot || !m_config.enabled)
        return;

    PathfindingContext* ctx = GetContext(bot);
    if (!ctx || !ctx->isActive)
        return;

    ctx->lastUpdateTime = getMSTime();

    // Record position periodically
    RecordPosition(bot, *ctx);

    // State machine update
    switch (ctx->state)
    {
        case PathfindingState::IDLE:
            UpdateIdle(bot, *ctx, diff);
            break;
        case PathfindingState::ENTERING:
            UpdateEntering(bot, *ctx, diff);
            break;
        case PathfindingState::EXPLORING:
            UpdateExploring(bot, *ctx, diff);
            break;
        case PathfindingState::STUCK_RECOVERY:
            UpdateStuckRecovery(bot, *ctx, diff);
            break;
        case PathfindingState::COMBAT:
            UpdateCombat(bot, *ctx, diff);
            break;
        case PathfindingState::BOSS_ENCOUNTER:
            UpdateBossEncounter(bot, *ctx, diff);
            break;
        case PathfindingState::EXITING:
            UpdateExiting(bot, *ctx, diff);
            break;
        case PathfindingState::RESETTING:
            UpdateResetting(bot, *ctx, diff);
            break;
        case PathfindingState::ANALYZING:
            UpdateAnalyzing(bot, *ctx, diff);
            break;
        case PathfindingState::COMPLETE:
            // Do nothing - waiting for external action
            break;
    }
}

void PathfindingBotManager::SetState(Player* bot, PathfindingState newState)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return;

    PathfindingState oldState = ctx->state;
    if (oldState == newState)
        return;

    LOG_DEBUG("playerbots", "PathfindingBotManager: Bot {} state change: {} -> {}",
              bot->GetName(), GetPathfindingStateName(oldState), GetPathfindingStateName(newState));

    OnExitState(bot, oldState);
    ctx->state = newState;
    ctx->stateStartTime = getMSTime();
    OnEnterState(bot, newState);
}

void PathfindingBotManager::OnEnterState(Player* bot, PathfindingState state)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return;

    switch (state)
    {
        case PathfindingState::ENTERING:
            EnterDungeon(bot, ctx->mapId);
            break;
        case PathfindingState::EXPLORING:
            m_explorationEngine->Initialize(ctx->mapId, bot->GetPosition());
            ctx->runStartTime = getMSTime();
            break;
        case PathfindingState::STUCK_RECOVERY:
            ctx->stuckPosition = bot->GetPosition();
            ctx->stuckStartTime = getMSTime();
            ctx->recoveryAttempts = 0;
            ctx->stuckCount++;
            break;
        case PathfindingState::ANALYZING:
            CompleteIteration(bot);
            break;
        case PathfindingState::RESETTING:
            ResetInstance(bot);
            break;
        default:
            break;
    }
}

void PathfindingBotManager::OnExitState(Player* bot, PathfindingState state)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return;

    switch (state)
    {
        case PathfindingState::COMBAT:
        case PathfindingState::BOSS_ENCOUNTER:
            // Record combat encounter end time
            if (!ctx->combatEncounters.empty())
            {
                ctx->combatEncounters.back().endTime = getMSTime();
            }
            break;
        default:
            break;
    }
}

void PathfindingBotManager::OnBotDeath(Player* bot)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx || !ctx->isActive)
        return;

    ctx->deathCount++;
    LOG_DEBUG("playerbots", "PathfindingBotManager: Bot {} died (death count: {})",
              bot->GetName(), ctx->deathCount);
}

void PathfindingBotManager::OnBotRespawn(Player* bot)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx || !ctx->isActive)
        return;

    // Continue exploration after respawn
    if (ctx->state == PathfindingState::COMBAT ||
        ctx->state == PathfindingState::BOSS_ENCOUNTER ||
        ctx->state == PathfindingState::STUCK_RECOVERY)
    {
        SetState(bot, PathfindingState::EXPLORING);
    }
}

void PathfindingBotManager::OnBossKilled(Player* bot, uint32 bossEntry)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx || !ctx->isActive)
        return;

    ctx->bossesKilled.push_back(bossEntry);

    // Record boss location
    if (!ctx->combatEncounters.empty())
    {
        ctx->combatEncounters.back().isBoss = true;
        ctx->combatEncounters.back().bossEntry = bossEntry;
    }

    LOG_INFO("playerbots", "PathfindingBotManager: Bot {} killed boss {} ({}/{})",
             bot->GetName(), bossEntry, ctx->bossesKilled.size(), ctx->totalBossCount);

    // Check if all bosses killed
    if (ctx->bossesKilled.size() >= ctx->totalBossCount)
    {
        SetState(bot, PathfindingState::ANALYZING);
    }
    else
    {
        SetState(bot, PathfindingState::EXPLORING);
    }
}

void PathfindingBotManager::OnCombatStart(Player* bot)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx || !ctx->isActive)
        return;

    // Check if fighting a boss
    if (IsBossTarget(bot))
    {
        SetState(bot, PathfindingState::BOSS_ENCOUNTER);
    }
    else if (ctx->state == PathfindingState::EXPLORING)
    {
        SetState(bot, PathfindingState::COMBAT);
    }

    // Record combat encounter
    CombatEncounter encounter;
    encounter.pos = bot->GetPosition();
    encounter.startTime = getMSTime();
    ctx->combatEncounters.push_back(encounter);
}

void PathfindingBotManager::OnCombatEnd(Player* bot)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx || !ctx->isActive)
        return;

    if (ctx->state == PathfindingState::COMBAT)
    {
        // Record trash pack location
        ctx->trashPackLocations.push_back(bot->GetPosition());
        SetState(bot, PathfindingState::EXPLORING);
    }
}

void PathfindingBotManager::OnPositionChanged(Player* bot, const Position& newPos)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx || !ctx->isActive)
        return;

    // Update distance
    float dist = CalculateDistance(ctx->lastPosition, newPos);
    if (dist > 0.5f)  // Minimum movement threshold
    {
        ctx->totalDistance += dist;
        ctx->lastPosition = newPos;
        ctx->lastMoveTime = getMSTime();
    }
}

void PathfindingBotManager::RecordPosition(Player* bot, PathfindingContext& ctx)
{
    uint32 now = getMSTime();

    // Record breadcrumb periodically
    if (now - ctx.lastMoveTime > m_config.breadcrumbInterval)
    {
        Position pos = bot->GetPosition();
        UpdateBreadcrumbTrail(ctx, pos);

        // Also record in path
        ctx.pathTaken.push_back(pos);
    }
}

void PathfindingBotManager::UpdateBreadcrumbTrail(PathfindingContext& ctx, const Position& pos)
{
    ctx.breadcrumbTrail.push_back(pos);

    // Limit trail size
    while (ctx.breadcrumbTrail.size() > m_config.maxBreadcrumbs)
    {
        ctx.breadcrumbTrail.erase(ctx.breadcrumbTrail.begin());
    }
}

// State update methods
void PathfindingBotManager::UpdateIdle(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    // Nothing to do in idle state
}

void PathfindingBotManager::UpdateEntering(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    // Check if bot is in the dungeon
    if (bot->GetMapId() == ctx.mapId)
    {
        LOG_INFO("playerbots", "PathfindingBotManager: Bot {} entered dungeon {}", bot->GetName(), ctx.mapId);
        SetState(bot, PathfindingState::EXPLORING);
    }
    else
    {
        // Check timeout
        uint32 elapsed = getMSTime() - ctx.stateStartTime;
        if (elapsed > 30000)  // 30 second timeout
        {
            LOG_WARN("playerbots", "PathfindingBotManager: Bot {} failed to enter dungeon, stopping", bot->GetName());
            StopPathfinding(bot);
        }
    }
}

void PathfindingBotManager::UpdateExploring(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    // Check for stuck condition
    uint32 timeSinceMove = getMSTime() - ctx.lastMoveTime;
    if (timeSinceMove > m_config.stuckThresholdMs)
    {
        SetState(bot, PathfindingState::STUCK_RECOVERY);
        return;
    }

    // Check if in combat
    if (bot->IsInCombat())
    {
        OnCombatStart(bot);
        return;
    }

    // Update exploration
    m_explorationEngine->MarkExplored(bot->GetPosition());
    ctx.explorationPercent = m_explorationEngine->GetExplorationPercent();

    // Get next exploration target
    Position target = m_explorationEngine->GetNextFrontierTarget(bot->GetPosition());
    if (target.GetPositionX() != 0.0f)
    {
        ctx.currentExplorationTarget = target;
        // The action system will handle actual movement
    }
    else if (m_explorationEngine->IsFullyExplored())
    {
        // Exploration complete
        SetState(bot, PathfindingState::ANALYZING);
    }
}

void PathfindingBotManager::UpdateStuckRecovery(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    RecoveryResult result = m_stuckRecovery->AttemptRecovery(bot, ctx);

    if (result.success)
    {
        LOG_DEBUG("playerbots", "PathfindingBotManager: Bot {} recovered from stuck using {}",
                  bot->GetName(), GetRecoveryMethodName(result.methodUsed));
        SetState(bot, PathfindingState::EXPLORING);
    }
    else if (ctx.recoveryAttempts >= m_config.maxRecoveryAttempts)
    {
        LOG_WARN("playerbots", "PathfindingBotManager: Bot {} failed to recover, teleporting to entrance",
                 bot->GetName());
        // Teleport to entrance as last resort
        bot->TeleportTo(ctx.mapId, ctx.entrancePosition.GetPositionX(),
                        ctx.entrancePosition.GetPositionY(), ctx.entrancePosition.GetPositionZ(),
                        ctx.entrancePosition.GetOrientation());
        SetState(bot, PathfindingState::EXPLORING);
    }

    ctx.recoveryAttempts++;
}

void PathfindingBotManager::UpdateCombat(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    if (!bot->IsInCombat())
    {
        OnCombatEnd(bot);
    }
}

void PathfindingBotManager::UpdateBossEncounter(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    // Boss encounter is handled by OnBossKilled callback
    if (!bot->IsInCombat())
    {
        // Combat ended but boss wasn't killed - return to exploring
        SetState(bot, PathfindingState::EXPLORING);
    }
}

void PathfindingBotManager::UpdateExiting(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    ExitDungeon(bot);

    if (bot->GetMapId() != ctx.mapId)
    {
        SetState(bot, PathfindingState::RESETTING);
    }
}

void PathfindingBotManager::UpdateResetting(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    // Wait for reset to complete, then start next iteration
    uint32 elapsed = getMSTime() - ctx.stateStartTime;
    if (elapsed > 5000)  // 5 second wait
    {
        if (!CheckConvergence(bot))
        {
            ResetForNewIteration(bot);
            SetState(bot, PathfindingState::ENTERING);
        }
        else
        {
            SetState(bot, PathfindingState::COMPLETE);
        }
    }
}

void PathfindingBotManager::UpdateAnalyzing(Player* bot, PathfindingContext& ctx, uint32 diff)
{
    // Analysis is done in CompleteIteration, transition to next state
    if (CheckConvergence(bot))
    {
        LOG_INFO("playerbots", "PathfindingBotManager: Bot {} route converged for dungeon {}",
                 bot->GetName(), ctx.mapId);
        SetState(bot, PathfindingState::COMPLETE);

        // Generate waypoint candidates
        m_waypointGenerator->GenerateWaypoints(ctx);

        if (m_config.autoPromoteWaypoints)
        {
            PromoteWaypointCandidates(ctx.mapId);
        }
    }
    else if (ctx.currentIteration >= ctx.maxIterations)
    {
        LOG_INFO("playerbots", "PathfindingBotManager: Bot {} reached max iterations for dungeon {}",
                 bot->GetName(), ctx.mapId);
        SetState(bot, PathfindingState::COMPLETE);
    }
    else
    {
        SetState(bot, PathfindingState::EXITING);
    }
}

void PathfindingBotManager::CompleteIteration(Player* bot)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return;

    uint32 durationMs = getMSTime() - ctx->runStartTime;

    // Create iteration result
    IterationResult result;
    result.mapId = ctx->mapId;
    result.iteration = ctx->currentIteration;
    result.durationMs = durationMs;
    result.deaths = ctx->deathCount;
    result.stuckEvents = ctx->stuckCount;
    result.totalDistance = ctx->totalDistance;
    result.explorationPct = ctx->explorationPercent;
    result.path = ctx->pathTaken;
    result.bossesKilled = ctx->bossesKilled;
    result.pathJson = SerializePathToJson(ctx->pathTaken);

    // Calculate score
    result.score = m_pathLearner->CalculateScore(result, m_config);

    // Record iteration
    m_pathLearner->RecordIteration(result);
    ctx->previousRuns.push_back(result);

    // Update best score
    if (result.score > ctx->bestScore)
    {
        ctx->bestScore = result.score;
        ctx->iterationsWithoutImprovement = 0;

        // Save best route
        SaveBestRouteToDatabase(ctx->mapId, result);
    }
    else
    {
        ctx->iterationsWithoutImprovement++;
    }

    // Save to database
    SaveIterationToDatabase(bot, result);

    LOG_INFO("playerbots", "PathfindingBotManager: Bot {} completed iteration {} | Score: {:.2f} | Time: {}ms | Deaths: {} | Stuck: {}",
             bot->GetName(), ctx->currentIteration, result.score, durationMs, result.deaths, result.stuckEvents);
}

void PathfindingBotManager::ResetForNewIteration(Player* bot)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return;

    ctx->ResetForNewIteration();
    m_explorationEngine->Reset();

    LOG_INFO("playerbots", "PathfindingBotManager: Bot {} starting iteration {}", bot->GetName(), ctx->currentIteration);
}

bool PathfindingBotManager::CheckConvergence(Player* bot)
{
    PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return false;

    return m_pathLearner->HasConverged(ctx->previousRuns, m_config);
}

void PathfindingBotManager::SaveIterationToDatabase(Player* bot, const IterationResult& result)
{
    CharacterDatabase.Execute(
        "INSERT INTO playerbots_pathfinding_iterations "
        "(map_id, bot_guid, iteration, duration_ms, death_count, stuck_count, total_distance, "
        "score, path_json, bosses_killed, exploration_pct) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, '{}', '{}', {})",
        result.mapId, bot->GetGUID().GetRawValue(), result.iteration, result.durationMs,
        result.deaths, result.stuckEvents, result.totalDistance, result.score,
        result.pathJson, "", result.explorationPct);
}

void PathfindingBotManager::SaveBestRouteToDatabase(uint32 mapId, const IterationResult& bestResult)
{
    std::string dungeonName = GetDungeonName(mapId);

    CharacterDatabase.Execute(
        "INSERT INTO playerbots_pathfinding_best_routes "
        "(map_id, dungeon_name, best_iteration_id, total_iterations, converged, best_score, avg_duration_ms, path_json) "
        "VALUES ({}, '{}', 0, {}, 0, {}, {}, '{}') "
        "ON DUPLICATE KEY UPDATE best_score = {}, avg_duration_ms = {}, path_json = '{}'",
        mapId, dungeonName, bestResult.iteration, bestResult.score, bestResult.durationMs, bestResult.pathJson,
        bestResult.score, bestResult.durationMs, bestResult.pathJson);
}

void PathfindingBotManager::LoadBestRouteFromDatabase(uint32 mapId)
{
    // Implemented when needed for resuming pathfinding
}

void PathfindingBotManager::PromoteWaypointCandidates(uint32 mapId)
{
    m_waypointGenerator->PromoteToWaypoints(mapId, m_config.minConfidenceForPromotion);
}

void PathfindingBotManager::ClearLearnedData(uint32 mapId)
{
    CharacterDatabase.Execute("DELETE FROM playerbots_pathfinding_iterations WHERE map_id = {}", mapId);
    CharacterDatabase.Execute("DELETE FROM playerbots_pathfinding_waypoint_candidates WHERE map_id = {}", mapId);
    CharacterDatabase.Execute("DELETE FROM playerbots_pathfinding_best_routes WHERE map_id = {}", mapId);
    CharacterDatabase.Execute("DELETE FROM playerbots_pathfinding_stuck_locations WHERE map_id = {}", mapId);

    LOG_INFO("playerbots", "PathfindingBotManager: Cleared all learned data for dungeon {}", mapId);
}

std::vector<uint32> PathfindingBotManager::GetDungeonBosses(uint32 mapId) const
{
    auto it = m_dungeonInfo.find(mapId);
    if (it != m_dungeonInfo.end())
        return it->second.bossEntries;
    return {};
}

Position PathfindingBotManager::GetDungeonEntrance(uint32 mapId) const
{
    auto it = m_dungeonInfo.find(mapId);
    if (it != m_dungeonInfo.end())
        return it->second.entrance;
    return Position();
}

std::string PathfindingBotManager::GetDungeonName(uint32 mapId) const
{
    auto it = m_dungeonInfo.find(mapId);
    if (it != m_dungeonInfo.end())
        return it->second.name;
    return "Unknown";
}

float PathfindingBotManager::CalculateDistance(const Position& a, const Position& b) const
{
    float dx = a.GetPositionX() - b.GetPositionX();
    float dy = a.GetPositionY() - b.GetPositionY();
    float dz = a.GetPositionZ() - b.GetPositionZ();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool PathfindingBotManager::IsBossTarget(Player* bot) const
{
    if (!bot)
        return false;

    Unit* target = bot->GetVictim();
    if (!target || !target->ToCreature())
        return false;

    const PathfindingContext* ctx = GetContext(bot);
    if (!ctx)
        return false;

    uint32 entry = target->GetEntry();
    for (uint32 bossEntry : ctx->expectedBosses)
    {
        if (entry == bossEntry)
            return true;
    }

    return false;
}

void PathfindingBotManager::EnterDungeon(Player* bot, uint32 mapId)
{
    Position entrance = GetDungeonEntrance(mapId);
    if (entrance.GetPositionX() != 0.0f)
    {
        bot->TeleportTo(mapId, entrance.GetPositionX(), entrance.GetPositionY(),
                        entrance.GetPositionZ(), entrance.GetOrientation());
    }
}

void PathfindingBotManager::ExitDungeon(Player* bot)
{
    // Teleport to home location or dungeon entrance outdoor position
    bot->TeleportTo(bot->m_homebindMapId, bot->m_homebindX, bot->m_homebindY, bot->m_homebindZ, 0.0f);
}

void PathfindingBotManager::ResetInstance(Player* bot)
{
    // Reset the instance via packet
    // Note: Bot needs to be outside the instance for this to work
    WorldPacket data(CMSG_RESET_INSTANCES, 0);
    bot->GetSession()->HandleResetInstancesOpcode(data);
}

std::string PathfindingBotManager::SerializePathToJson(const std::vector<Position>& path) const
{
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < path.size(); ++i)
    {
        if (i > 0) ss << ",";
        ss << "{\"x\":" << path[i].GetPositionX()
           << ",\"y\":" << path[i].GetPositionY()
           << ",\"z\":" << path[i].GetPositionZ() << "}";
    }
    ss << "]";
    return ss.str();
}

std::vector<Position> PathfindingBotManager::DeserializePathFromJson(const std::string& json) const
{
    // Simple JSON parser - in production use a proper JSON library
    std::vector<Position> path;
    // Implementation would parse the JSON array of position objects
    return path;
}
