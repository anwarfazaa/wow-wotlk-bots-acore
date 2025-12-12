/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_PATHLEARNER_H
#define _PLAYERBOT_PATHLEARNER_H

#include "PathfindingBotContext.h"
#include <vector>
#include <unordered_map>

/**
 * PathLearner - Route optimization through iterative learning
 *
 * Analyzes iteration results to:
 * - Calculate efficiency scores for routes
 * - Detect route convergence
 * - Identify the best route for each dungeon
 * - Track improvement over iterations
 *
 * Score calculation:
 * - 40% weight on time (faster = better)
 * - 30% weight on deaths (fewer = better)
 * - 20% weight on stuck events (fewer = better)
 * - 10% weight on distance (shorter = better)
 */
class PathLearner
{
public:
    PathLearner();
    ~PathLearner();

    // Recording
    void RecordIteration(const IterationResult& result);

    // Scoring
    float CalculateScore(const IterationResult& result, const PathfindingConfig& config) const;
    float NormalizeTime(uint32 durationMs) const;
    float NormalizeDeaths(uint32 deaths) const;
    float NormalizeStuck(uint32 stuckEvents) const;
    float NormalizeDistance(float distance) const;

    // Convergence detection
    bool HasConverged(const std::vector<IterationResult>& runs, const PathfindingConfig& config) const;
    float CalculatePathSimilarity(const std::vector<Position>& path1, const std::vector<Position>& path2) const;
    bool AreBossOrdersSame(const std::vector<IterationResult>& runs) const;

    // Best route management
    const IterationResult* GetBestRoute(uint32 mapId) const;
    void SetBestRoute(uint32 mapId, const IterationResult& result);
    float GetBestScore(uint32 mapId) const;

    // Statistics
    uint32 GetIterationCount(uint32 mapId) const;
    float GetAverageScore(uint32 mapId) const;
    float GetScoreImprovement(uint32 mapId) const;  // Percentage improvement from first to best

    // Route analysis
    std::vector<Position> GetOptimizedPath(uint32 mapId) const;
    std::vector<Position> MergePaths(const std::vector<std::vector<Position>>& paths) const;

private:
    // Normalization helpers (convert raw values to 0-1 range)
    static constexpr uint32 BASELINE_TIME_MS = 1800000;     // 30 minutes baseline
    static constexpr uint32 MAX_EXPECTED_DEATHS = 10;
    static constexpr uint32 MAX_EXPECTED_STUCK = 20;
    static constexpr float MAX_EXPECTED_DISTANCE = 5000.0f;  // 5000 yards

    // Path similarity calculation
    float CalculateHausdorffDistance(const std::vector<Position>& path1,
                                     const std::vector<Position>& path2) const;
    float PointToPathDistance(const Position& point, const std::vector<Position>& path) const;
    float PointDistance(const Position& a, const Position& b) const;

    // Per-dungeon data
    struct DungeonLearningData
    {
        std::vector<IterationResult> iterations;
        IterationResult bestResult;
        float bestScore = 0.0f;
        bool hasBestResult = false;
    };

    std::unordered_map<uint32, DungeonLearningData> m_dungeonData;
    mutable std::shared_mutex m_dataMutex;
};

#endif  // _PLAYERBOT_PATHLEARNER_H
