/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#ifndef _PLAYERBOT_EXPLORATIONENGINE_H
#define _PLAYERBOT_EXPLORATIONENGINE_H

#include "PathfindingBotContext.h"
#include <unordered_map>
#include <vector>
#include <queue>

class Map;
class PathGenerator;

/**
 * ExplorationEngine - Frontier-based dungeon exploration
 *
 * Uses a grid-based representation to track explored areas and identify
 * frontier cells (unexplored cells adjacent to explored areas) for
 * systematic dungeon coverage.
 *
 * Algorithm:
 * 1. Initialize grid at starting position
 * 2. Mark cells as explored when visited
 * 3. Identify frontier cells (unexplored with explored neighbors)
 * 4. Score frontiers by distance, direction, reachability
 * 5. Return highest-scored frontier as next target
 */
class ExplorationEngine
{
public:
    ExplorationEngine();
    ~ExplorationEngine();

    // Initialization
    void Initialize(uint32 mapId, const Position& startPos);
    void Reset();

    // Exploration tracking
    void MarkExplored(const Position& pos, float radius = 10.0f);
    void MarkUnreachable(const Position& pos);
    void MarkObstacle(const Position& pos);

    // Frontier management
    Position GetNextFrontierTarget(const Position& currentPos);
    std::vector<Position> GetAllFrontiers() const;
    uint32 GetFrontierCount() const;

    // Progress tracking
    bool IsFullyExplored() const;
    float GetExplorationPercent() const;
    uint32 GetExploredCellCount() const;
    uint32 GetTotalCellCount() const;

    // Grid queries
    bool IsCellExplored(const Position& pos) const;
    bool IsCellReachable(const Position& pos) const;
    bool IsCellFrontier(const Position& pos) const;

    // Configuration
    void SetCellSize(float size) { m_cellSize = size; }
    float GetCellSize() const { return m_cellSize; }
    void SetExplorationRadius(float radius) { m_explorationRadius = radius; }

private:
    // Grid cell management
    uint64 PositionToKey(const Position& pos) const;
    Position KeyToPosition(uint64 key) const;
    ExplorationGridCell* GetCell(const Position& pos);
    const ExplorationGridCell* GetCell(const Position& pos) const;
    ExplorationGridCell& GetOrCreateCell(const Position& pos);

    // Frontier detection
    void UpdateFrontiers();
    bool IsFrontierCell(const Position& pos) const;
    std::vector<Position> GetNeighborPositions(const Position& pos) const;

    // Frontier scoring
    float ScoreFrontier(const Position& frontier, const Position& currentPos) const;
    bool IsReachable(const Position& from, const Position& to) const;

    // Pathfinding integration
    bool ValidatePathToTarget(const Position& from, const Position& to) const;

    // Grid storage
    std::unordered_map<uint64, ExplorationGridCell> m_grid;
    std::vector<Position> m_frontiers;

    // Configuration
    float m_cellSize = 5.0f;           // Grid cell size in yards
    float m_explorationRadius = 10.0f;  // Radius marked as explored around bot
    uint32 m_mapId = 0;

    // Bounds tracking
    float m_minX = 0.0f, m_maxX = 0.0f;
    float m_minY = 0.0f, m_maxY = 0.0f;
    float m_minZ = 0.0f, m_maxZ = 0.0f;

    // Statistics
    uint32 m_exploredCount = 0;
    uint32 m_totalEstimatedCells = 0;

    // Direction bias for exploration (favor forward movement)
    Position m_lastDirection;
    float m_directionBias = 0.3f;
};

#endif  // _PLAYERBOT_EXPLORATIONENGINE_H
