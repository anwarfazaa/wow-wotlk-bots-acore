/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 * Copyright (C) 2021+ mod-playerbots <https://github.com/liyunfan1223/mod-playerbots>
 */

#include "ExplorationEngine.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "PathGenerator.h"
#include <algorithm>
#include <cmath>

ExplorationEngine::ExplorationEngine() = default;
ExplorationEngine::~ExplorationEngine() = default;

void ExplorationEngine::Initialize(uint32 mapId, const Position& startPos)
{
    Reset();
    m_mapId = mapId;

    // Initialize bounds from starting position
    m_minX = startPos.GetPositionX() - 500.0f;
    m_maxX = startPos.GetPositionX() + 500.0f;
    m_minY = startPos.GetPositionY() - 500.0f;
    m_maxY = startPos.GetPositionY() + 500.0f;
    m_minZ = startPos.GetPositionZ() - 100.0f;
    m_maxZ = startPos.GetPositionZ() + 100.0f;

    // Estimate total cells (will be refined as we explore)
    float width = m_maxX - m_minX;
    float height = m_maxY - m_minY;
    m_totalEstimatedCells = static_cast<uint32>((width / m_cellSize) * (height / m_cellSize) * 0.3f);  // Assume 30% is traversable

    // Mark starting position as explored
    MarkExplored(startPos, m_explorationRadius);

    LOG_DEBUG("playerbots", "ExplorationEngine: Initialized for map {} at ({}, {}, {})",
              mapId, startPos.GetPositionX(), startPos.GetPositionY(), startPos.GetPositionZ());
}

void ExplorationEngine::Reset()
{
    m_grid.clear();
    m_frontiers.clear();
    m_exploredCount = 0;
    m_totalEstimatedCells = 0;
    m_lastDirection = Position();
}

void ExplorationEngine::MarkExplored(const Position& pos, float radius)
{
    // Mark all cells within radius as explored
    int cellRadius = static_cast<int>(std::ceil(radius / m_cellSize));

    for (int dx = -cellRadius; dx <= cellRadius; ++dx)
    {
        for (int dy = -cellRadius; dy <= cellRadius; ++dy)
        {
            Position cellPos;
            cellPos.m_positionX = pos.GetPositionX() + dx * m_cellSize;
            cellPos.m_positionY = pos.GetPositionY() + dy * m_cellSize;
            cellPos.m_positionZ = pos.GetPositionZ();

            float dist = std::sqrt(dx * dx + dy * dy) * m_cellSize;
            if (dist <= radius)
            {
                ExplorationGridCell& cell = GetOrCreateCell(cellPos);
                if (!cell.explored)
                {
                    cell.explored = true;
                    cell.visitCount++;
                    cell.lastVisitTime = getMSTime();
                    m_exploredCount++;
                }
                else
                {
                    cell.visitCount++;
                    cell.lastVisitTime = getMSTime();
                }
            }
        }
    }

    // Update bounds
    m_minX = std::min(m_minX, pos.GetPositionX() - radius);
    m_maxX = std::max(m_maxX, pos.GetPositionX() + radius);
    m_minY = std::min(m_minY, pos.GetPositionY() - radius);
    m_maxY = std::max(m_maxY, pos.GetPositionY() + radius);

    // Update frontiers
    UpdateFrontiers();
}

void ExplorationEngine::MarkUnreachable(const Position& pos)
{
    ExplorationGridCell& cell = GetOrCreateCell(pos);
    cell.reachable = false;
}

void ExplorationEngine::MarkObstacle(const Position& pos)
{
    ExplorationGridCell& cell = GetOrCreateCell(pos);
    cell.hasObstacle = true;
    cell.reachable = false;
}

Position ExplorationEngine::GetNextFrontierTarget(const Position& currentPos)
{
    if (m_frontiers.empty())
    {
        UpdateFrontiers();
        if (m_frontiers.empty())
        {
            return Position();  // No frontiers - exploration complete
        }
    }

    // Score all frontiers and pick the best one
    float bestScore = -std::numeric_limits<float>::max();
    Position bestFrontier;

    for (const Position& frontier : m_frontiers)
    {
        float score = ScoreFrontier(frontier, currentPos);
        if (score > bestScore)
        {
            bestScore = score;
            bestFrontier = frontier;
        }
    }

    // Update direction bias
    if (bestFrontier.GetPositionX() != 0.0f)
    {
        m_lastDirection.m_positionX = bestFrontier.GetPositionX() - currentPos.GetPositionX();
        m_lastDirection.m_positionY = bestFrontier.GetPositionY() - currentPos.GetPositionY();
        float len = std::sqrt(m_lastDirection.m_positionX * m_lastDirection.m_positionX +
                              m_lastDirection.m_positionY * m_lastDirection.m_positionY);
        if (len > 0.0f)
        {
            m_lastDirection.m_positionX /= len;
            m_lastDirection.m_positionY /= len;
        }
    }

    return bestFrontier;
}

std::vector<Position> ExplorationEngine::GetAllFrontiers() const
{
    return m_frontiers;
}

uint32 ExplorationEngine::GetFrontierCount() const
{
    return m_frontiers.size();
}

bool ExplorationEngine::IsFullyExplored() const
{
    return m_frontiers.empty() && m_exploredCount > 0;
}

float ExplorationEngine::GetExplorationPercent() const
{
    if (m_totalEstimatedCells == 0)
        return 0.0f;

    // Dynamically adjust estimate based on explored area
    uint32 adjustedTotal = std::max(m_totalEstimatedCells, m_exploredCount + static_cast<uint32>(m_frontiers.size()));

    return std::min(1.0f, static_cast<float>(m_exploredCount) / static_cast<float>(adjustedTotal));
}

uint32 ExplorationEngine::GetExploredCellCount() const
{
    return m_exploredCount;
}

uint32 ExplorationEngine::GetTotalCellCount() const
{
    return m_totalEstimatedCells;
}

bool ExplorationEngine::IsCellExplored(const Position& pos) const
{
    const ExplorationGridCell* cell = GetCell(pos);
    return cell && cell->explored;
}

bool ExplorationEngine::IsCellReachable(const Position& pos) const
{
    const ExplorationGridCell* cell = GetCell(pos);
    return !cell || cell->reachable;  // Unknown cells are assumed reachable
}

bool ExplorationEngine::IsCellFrontier(const Position& pos) const
{
    const ExplorationGridCell* cell = GetCell(pos);
    return cell && cell->isFrontier;
}

uint64 ExplorationEngine::PositionToKey(const Position& pos) const
{
    // Convert position to grid coordinates
    int32 x = static_cast<int32>(std::floor(pos.GetPositionX() / m_cellSize));
    int32 y = static_cast<int32>(std::floor(pos.GetPositionY() / m_cellSize));

    // Combine into single key (using int16 ranges to fit in uint64)
    uint64 ux = static_cast<uint64>(static_cast<uint32>(x + 32768));
    uint64 uy = static_cast<uint64>(static_cast<uint32>(y + 32768));

    return (ux << 32) | uy;
}

Position ExplorationEngine::KeyToPosition(uint64 key) const
{
    int32 x = static_cast<int32>((key >> 32) & 0xFFFFFFFF) - 32768;
    int32 y = static_cast<int32>(key & 0xFFFFFFFF) - 32768;

    Position pos;
    pos.m_positionX = (x + 0.5f) * m_cellSize;
    pos.m_positionY = (y + 0.5f) * m_cellSize;
    pos.m_positionZ = 0.0f;  // Z needs to be determined separately

    return pos;
}

ExplorationGridCell* ExplorationEngine::GetCell(const Position& pos)
{
    uint64 key = PositionToKey(pos);
    auto it = m_grid.find(key);
    if (it != m_grid.end())
        return &it->second;
    return nullptr;
}

const ExplorationGridCell* ExplorationEngine::GetCell(const Position& pos) const
{
    uint64 key = PositionToKey(pos);
    auto it = m_grid.find(key);
    if (it != m_grid.end())
        return &it->second;
    return nullptr;
}

ExplorationGridCell& ExplorationEngine::GetOrCreateCell(const Position& pos)
{
    uint64 key = PositionToKey(pos);
    return m_grid[key];
}

void ExplorationEngine::UpdateFrontiers()
{
    m_frontiers.clear();

    // Reset frontier flags
    for (auto& [key, cell] : m_grid)
    {
        cell.isFrontier = false;
    }

    // Find all frontier cells
    for (auto& [key, cell] : m_grid)
    {
        if (!cell.explored)
            continue;

        Position cellPos = KeyToPosition(key);
        std::vector<Position> neighbors = GetNeighborPositions(cellPos);

        for (const Position& neighbor : neighbors)
        {
            if (!IsCellExplored(neighbor) && IsCellReachable(neighbor))
            {
                // This neighbor is a frontier
                ExplorationGridCell& frontierCell = GetOrCreateCell(neighbor);
                if (!frontierCell.isFrontier && frontierCell.reachable)
                {
                    frontierCell.isFrontier = true;
                    m_frontiers.push_back(neighbor);
                }
            }
        }
    }

    // Limit frontier count to prevent explosion
    const uint32 maxFrontiers = 100;
    if (m_frontiers.size() > maxFrontiers)
    {
        // Keep only the closest frontiers
        // Note: In a real implementation, we'd sort by distance to current position
        m_frontiers.resize(maxFrontiers);
    }
}

bool ExplorationEngine::IsFrontierCell(const Position& pos) const
{
    // A frontier cell is unexplored but adjacent to an explored cell
    if (IsCellExplored(pos))
        return false;

    std::vector<Position> neighbors = GetNeighborPositions(pos);
    for (const Position& neighbor : neighbors)
    {
        if (IsCellExplored(neighbor))
            return true;
    }

    return false;
}

std::vector<Position> ExplorationEngine::GetNeighborPositions(const Position& pos) const
{
    std::vector<Position> neighbors;
    neighbors.reserve(8);

    // 8-connected neighbors
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            if (dx == 0 && dy == 0)
                continue;

            Position neighbor;
            neighbor.m_positionX = pos.GetPositionX() + dx * m_cellSize;
            neighbor.m_positionY = pos.GetPositionY() + dy * m_cellSize;
            neighbor.m_positionZ = pos.GetPositionZ();

            neighbors.push_back(neighbor);
        }
    }

    return neighbors;
}

float ExplorationEngine::ScoreFrontier(const Position& frontier, const Position& currentPos) const
{
    float score = 0.0f;

    // Distance factor - closer is better (but not too close)
    float dx = frontier.GetPositionX() - currentPos.GetPositionX();
    float dy = frontier.GetPositionY() - currentPos.GetPositionY();
    float distance = std::sqrt(dx * dx + dy * dy);

    // Ideal distance is around 20-40 yards
    if (distance < 5.0f)
        score -= 100.0f;  // Too close, probably already explored
    else if (distance < 20.0f)
        score += 50.0f - distance;  // Close is good
    else if (distance < 50.0f)
        score += 30.0f - (distance - 20.0f) * 0.5f;  // Medium distance
    else
        score -= (distance - 50.0f) * 0.3f;  // Far is less desirable

    // Direction bias - favor continuing in same direction
    if (m_lastDirection.GetPositionX() != 0.0f || m_lastDirection.GetPositionY() != 0.0f)
    {
        float dirX = dx / (distance + 0.001f);
        float dirY = dy / (distance + 0.001f);
        float dot = dirX * m_lastDirection.GetPositionX() + dirY * m_lastDirection.GetPositionY();
        score += dot * m_directionBias * 20.0f;  // Bonus for continuing forward
    }

    // Frontier density - areas with more frontiers are more interesting
    const ExplorationGridCell* cell = GetCell(frontier);
    if (cell)
    {
        // Penalize cells we've tried before
        score -= cell->visitCount * 10.0f;
    }

    // Check reachability (expensive, so only for top candidates)
    // In practice, this would use the navmesh
    if (!IsCellReachable(frontier))
        score -= 1000.0f;

    return score;
}

bool ExplorationEngine::IsReachable(const Position& from, const Position& to) const
{
    // Simplified reachability check
    // In full implementation, use PathGenerator

    float dx = to.GetPositionX() - from.GetPositionX();
    float dy = to.GetPositionY() - from.GetPositionY();
    float dz = to.GetPositionZ() - from.GetPositionZ();

    // Check for excessive height difference
    if (std::abs(dz) > 20.0f)
        return false;

    // Check for obstacles along the path (simplified)
    // Full implementation would use navmesh raycasting

    return true;
}

bool ExplorationEngine::ValidatePathToTarget(const Position& from, const Position& to) const
{
    // Use pathfinding to validate target is reachable
    // This is a placeholder - full implementation would use PathGenerator

    Map* map = sMapMgr->FindBaseNonInstanceMap(m_mapId);
    if (!map)
        return false;

    // In full implementation:
    // PathGenerator path(nullptr);
    // path.SetMap(map);
    // bool result = path.CalculatePath(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
    //                                  to.GetPositionX(), to.GetPositionY(), to.GetPositionZ());
    // return result && path.GetPathType() != PATHFIND_NOPATH;

    return true;  // Assume reachable for now
}
