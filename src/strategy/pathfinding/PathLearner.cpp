/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "PathLearner.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <numeric>

PathLearner::PathLearner() = default;
PathLearner::~PathLearner() = default;

void PathLearner::RecordIteration(const IterationResult& result)
{
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);

    DungeonLearningData& data = m_dungeonData[result.mapId];
    data.iterations.push_back(result);

    // Update best if this is better
    if (result.score > data.bestScore)
    {
        data.bestResult = result;
        data.bestScore = result.score;
        data.hasBestResult = true;

        LOG_DEBUG("playerbots", "PathLearner: New best score {:.2f} for map {} (iteration {})",
                  result.score, result.mapId, result.iteration);
    }
}

float PathLearner::CalculateScore(const IterationResult& result, const PathfindingConfig& config) const
{
    // Normalize all factors to 0-1 range (higher = better)
    float timeScore = NormalizeTime(result.durationMs);
    float deathScore = NormalizeDeaths(result.deaths);
    float stuckScore = NormalizeStuck(result.stuckEvents);
    float distanceScore = NormalizeDistance(result.totalDistance);

    // Weighted sum
    float score = (config.weightTime * timeScore) +
                  (config.weightDeaths * deathScore) +
                  (config.weightStuck * stuckScore) +
                  (config.weightDistance * distanceScore);

    // Bonus for high exploration
    if (result.explorationPct > 0.9f)
        score *= 1.1f;  // 10% bonus for 90%+ exploration

    // Penalty for incomplete runs (not all bosses killed)
    // This would require knowing expected boss count
    // For now, we assume complete runs

    return score;
}

float PathLearner::NormalizeTime(uint32 durationMs) const
{
    // Score: 1.0 for instant, 0.0 for baseline time or longer
    // Linear interpolation
    if (durationMs >= BASELINE_TIME_MS)
        return 0.0f;

    return 1.0f - (static_cast<float>(durationMs) / static_cast<float>(BASELINE_TIME_MS));
}

float PathLearner::NormalizeDeaths(uint32 deaths) const
{
    // Score: 1.0 for no deaths, 0.0 for max expected or more
    if (deaths >= MAX_EXPECTED_DEATHS)
        return 0.0f;

    return 1.0f - (static_cast<float>(deaths) / static_cast<float>(MAX_EXPECTED_DEATHS));
}

float PathLearner::NormalizeStuck(uint32 stuckEvents) const
{
    // Score: 1.0 for no stuck events, 0.0 for max expected or more
    if (stuckEvents >= MAX_EXPECTED_STUCK)
        return 0.0f;

    return 1.0f - (static_cast<float>(stuckEvents) / static_cast<float>(MAX_EXPECTED_STUCK));
}

float PathLearner::NormalizeDistance(float distance) const
{
    // Score: 1.0 for shortest path (0), 0.0 for max expected or more
    // Shorter is better
    if (distance >= MAX_EXPECTED_DISTANCE)
        return 0.0f;

    return 1.0f - (distance / MAX_EXPECTED_DISTANCE);
}

bool PathLearner::HasConverged(const std::vector<IterationResult>& runs, const PathfindingConfig& config) const
{
    // Need at least convergenceIterations runs
    if (runs.size() < config.convergenceIterations)
        return false;

    // Get the last N iterations
    size_t startIdx = runs.size() - config.convergenceIterations;
    std::vector<IterationResult> recentRuns(runs.begin() + startIdx, runs.end());

    // Check 1: Score improvement threshold
    float maxScore = 0.0f;
    float minScore = std::numeric_limits<float>::max();

    for (const auto& run : recentRuns)
    {
        maxScore = std::max(maxScore, run.score);
        minScore = std::min(minScore, run.score);
    }

    float scoreRange = maxScore - minScore;
    float avgScore = (maxScore + minScore) / 2.0f;

    if (avgScore > 0.0f && (scoreRange / avgScore) > config.convergenceThreshold)
    {
        // Scores still varying too much
        return false;
    }

    // Check 2: Boss kill order consistency
    if (!AreBossOrdersSame(recentRuns))
    {
        return false;
    }

    // Check 3: Path similarity
    for (size_t i = 1; i < recentRuns.size(); ++i)
    {
        float similarity = CalculatePathSimilarity(recentRuns[0].path, recentRuns[i].path);
        if (similarity < 0.95f)  // Less than 95% similar
        {
            return false;
        }
    }

    LOG_INFO("playerbots", "PathLearner: Route has converged for map {} after {} iterations",
             runs.back().mapId, runs.size());

    return true;
}

float PathLearner::CalculatePathSimilarity(const std::vector<Position>& path1,
                                           const std::vector<Position>& path2) const
{
    if (path1.empty() || path2.empty())
        return 0.0f;

    // Use Hausdorff-like distance for path similarity
    float hausdorff = CalculateHausdorffDistance(path1, path2);

    // Convert distance to similarity (0 distance = 1.0 similarity)
    // Assume paths within 10 yards of each other are "similar"
    const float maxDist = 50.0f;  // 50 yards max for comparison
    float similarity = 1.0f - std::min(1.0f, hausdorff / maxDist);

    return similarity;
}

float PathLearner::CalculateHausdorffDistance(const std::vector<Position>& path1,
                                              const std::vector<Position>& path2) const
{
    // Hausdorff distance: max of the two directed Hausdorff distances
    float maxDist1 = 0.0f;
    for (const Position& p : path1)
    {
        float minDist = PointToPathDistance(p, path2);
        maxDist1 = std::max(maxDist1, minDist);
    }

    float maxDist2 = 0.0f;
    for (const Position& p : path2)
    {
        float minDist = PointToPathDistance(p, path1);
        maxDist2 = std::max(maxDist2, minDist);
    }

    return std::max(maxDist1, maxDist2);
}

float PathLearner::PointToPathDistance(const Position& point, const std::vector<Position>& path) const
{
    float minDist = std::numeric_limits<float>::max();

    for (const Position& p : path)
    {
        float dist = PointDistance(point, p);
        minDist = std::min(minDist, dist);
    }

    return minDist;
}

float PathLearner::PointDistance(const Position& a, const Position& b) const
{
    float dx = a.GetPositionX() - b.GetPositionX();
    float dy = a.GetPositionY() - b.GetPositionY();
    float dz = a.GetPositionZ() - b.GetPositionZ();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool PathLearner::AreBossOrdersSame(const std::vector<IterationResult>& runs) const
{
    if (runs.empty())
        return true;

    const std::vector<uint32>& reference = runs[0].bossesKilled;

    for (size_t i = 1; i < runs.size(); ++i)
    {
        const std::vector<uint32>& current = runs[i].bossesKilled;

        // Must have same number of bosses
        if (current.size() != reference.size())
            return false;

        // Must be in same order
        for (size_t j = 0; j < reference.size(); ++j)
        {
            if (current[j] != reference[j])
                return false;
        }
    }

    return true;
}

const IterationResult* PathLearner::GetBestRoute(uint32 mapId) const
{
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);

    auto it = m_dungeonData.find(mapId);
    if (it != m_dungeonData.end() && it->second.hasBestResult)
        return &it->second.bestResult;

    return nullptr;
}

void PathLearner::SetBestRoute(uint32 mapId, const IterationResult& result)
{
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);

    DungeonLearningData& data = m_dungeonData[mapId];
    data.bestResult = result;
    data.bestScore = result.score;
    data.hasBestResult = true;
}

float PathLearner::GetBestScore(uint32 mapId) const
{
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);

    auto it = m_dungeonData.find(mapId);
    if (it != m_dungeonData.end())
        return it->second.bestScore;

    return 0.0f;
}

uint32 PathLearner::GetIterationCount(uint32 mapId) const
{
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);

    auto it = m_dungeonData.find(mapId);
    if (it != m_dungeonData.end())
        return it->second.iterations.size();

    return 0;
}

float PathLearner::GetAverageScore(uint32 mapId) const
{
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);

    auto it = m_dungeonData.find(mapId);
    if (it == m_dungeonData.end() || it->second.iterations.empty())
        return 0.0f;

    float sum = 0.0f;
    for (const auto& result : it->second.iterations)
        sum += result.score;

    return sum / it->second.iterations.size();
}

float PathLearner::GetScoreImprovement(uint32 mapId) const
{
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);

    auto it = m_dungeonData.find(mapId);
    if (it == m_dungeonData.end() || it->second.iterations.empty())
        return 0.0f;

    float firstScore = it->second.iterations.front().score;
    float bestScore = it->second.bestScore;

    if (firstScore <= 0.0f)
        return 0.0f;

    return (bestScore - firstScore) / firstScore * 100.0f;  // Return as percentage
}

std::vector<Position> PathLearner::GetOptimizedPath(uint32 mapId) const
{
    const IterationResult* best = GetBestRoute(mapId);
    if (!best)
        return {};

    return best->path;
}

std::vector<Position> PathLearner::MergePaths(const std::vector<std::vector<Position>>& paths) const
{
    if (paths.empty())
        return {};

    if (paths.size() == 1)
        return paths[0];

    // Find the longest path as base
    size_t longestIdx = 0;
    size_t maxLen = 0;
    for (size_t i = 0; i < paths.size(); ++i)
    {
        if (paths[i].size() > maxLen)
        {
            maxLen = paths[i].size();
            longestIdx = i;
        }
    }

    const std::vector<Position>& basePath = paths[longestIdx];

    // For each point in base path, average with closest points from other paths
    std::vector<Position> mergedPath;
    mergedPath.reserve(basePath.size());

    for (const Position& basePoint : basePath)
    {
        float sumX = basePoint.GetPositionX();
        float sumY = basePoint.GetPositionY();
        float sumZ = basePoint.GetPositionZ();
        uint32 count = 1;

        for (size_t i = 0; i < paths.size(); ++i)
        {
            if (i == longestIdx)
                continue;

            // Find closest point in this path
            float minDist = std::numeric_limits<float>::max();
            const Position* closest = nullptr;

            for (const Position& p : paths[i])
            {
                float dist = PointDistance(basePoint, p);
                if (dist < minDist && dist < 20.0f)  // Only merge if within 20 yards
                {
                    minDist = dist;
                    closest = &p;
                }
            }

            if (closest)
            {
                sumX += closest->GetPositionX();
                sumY += closest->GetPositionY();
                sumZ += closest->GetPositionZ();
                count++;
            }
        }

        Position merged;
        merged.m_positionX = sumX / count;
        merged.m_positionY = sumY / count;
        merged.m_positionZ = sumZ / count;
        merged.m_orientation = basePoint.GetOrientation();

        mergedPath.push_back(merged);
    }

    return mergedPath;
}
