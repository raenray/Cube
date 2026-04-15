#include "Game/TileMap.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

#include <glad/gl.h>

#include "Renderer/Renderer2D.h"
#include "Renderer/Texture2D.h"

namespace GameLayer
{

namespace
{

float GetHardnessForTileId(std::uint16_t id)
{
    switch (id)
    {
    case Material::Air:
        return 0.0f;
    case Material::Dirt:
        return 1.2f;
    case Material::Sand:
        return 0.8f;
    case Material::Water:
        return 1000000.0f; // 基本不可挖
    default:
        return 1.0f;
    }
}

int FloorDiv(int value, int divisor)
{
    const int q = value / divisor;
    const int r = value % divisor;
    if (r != 0 && ((r > 0) != (divisor > 0)))
    {
        return q - 1;
    }
    return q;
}

int PositiveMod(int value, int mod)
{
    int r = value % mod;
    if (r < 0)
    {
        r += mod;
    }
    return r;
}

float SmoothStep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float HashNoise1D(int x)
{
    unsigned int n = static_cast<unsigned int>(x);
    n ^= 2747636419u;
    n *= 2654435769u;
    n ^= (n >> 16);
    n *= 2654435769u;
    n ^= (n >> 16);
    const float normalized = static_cast<float>(n & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
    return normalized * 2.0f - 1.0f;
}

float ValueNoise1D(float x)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int x1 = x0 + 1;
    const float t = x - static_cast<float>(x0);
    const float s = SmoothStep(t);

    const float n0 = HashNoise1D(x0);
    const float n1 = HashNoise1D(x1);
    return n0 + (n1 - n0) * s;
}

float FractalNoise1D(float x)
{
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;

    constexpr int octaves = 4;
    for (int i = 0; i < octaves; ++i)
    {
        total += ValueNoise1D(x * frequency) * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return (norm > 0.0f) ? (total / norm) : 0.0f;
}

int GetGroundHeightAtWorldX(int worldTileX)
{
    constexpr float kNoiseScale = 0.02f;
    constexpr float kBaseHeight = 10.0f;
    constexpr float kHeightAmplitude = 12.0f;

    const float h = FractalNoise1D(static_cast<float>(worldTileX) * kNoiseScale);
    return static_cast<int>(std::floor(kBaseHeight + h * kHeightAmplitude));
}

} // namespace

std::size_t ChunkCoordHash::operator()(const ChunkCoord& coord) const
{
    const std::size_t hx = std::hash<int>{}(coord.x);
    const std::size_t hy = std::hash<int>{}(coord.y);
    return hx ^ (hy + 0x9e3779b9 + (hx << 6) + (hx >> 2));
}

TileMap::TileMap()
{
    StartWorker();
    LoadWorldFromFile();

    if (LoadedChunkCount() == 0)
    {
        EnsureChunksAround(glm::vec2(0.0f, 0.0f), 1);
    }
}

TileMap::~TileMap()
{
    StopWorker();
    ProcessCompletedTasks();
    SaveWorldToFile();

    std::scoped_lock lock(m_ChunkMutex);
    for (auto& entry : m_Chunks)
    {
        ReleaseChunkMesh(entry.second);
    }
}

void TileMap::EnsureChunksAround(const glm::vec2& worldCenter, int radiusInChunks)
{
    const float chunkWorldSize = static_cast<float>(Chunk::kSize) * kTileSize;
    const int centerChunkX = static_cast<int>(std::floor(worldCenter.x / chunkWorldSize));
    const int centerChunkY = static_cast<int>(std::floor(worldCenter.y / chunkWorldSize));

    for (int dy = -radiusInChunks; dy <= radiusInChunks; ++dy)
    {
        for (int dx = -radiusInChunks; dx <= radiusInChunks; ++dx)
        {
            EnqueueGenerateTask(ChunkCoord{centerChunkX + dx, centerChunkY + dy});
        }
    }
}

void TileMap::UnloadFarChunks(const glm::vec2& worldCenter, int keepRadiusInChunks)
{
    const float chunkWorldSize = static_cast<float>(Chunk::kSize) * kTileSize;
    const int centerChunkX = static_cast<int>(std::floor(worldCenter.x / chunkWorldSize));
    const int centerChunkY = static_cast<int>(std::floor(worldCenter.y / chunkWorldSize));

    const auto isOutOfRange = [&](const ChunkCoord& coord) {
        const int dx = std::abs(coord.x - centerChunkX);
        const int dy = std::abs(coord.y - centerChunkY);
        return (dx > keepRadiusInChunks) || (dy > keepRadiusInChunks);
    };

    {
        std::scoped_lock lock(m_ChunkMutex);
        for (auto it = m_Chunks.begin(); it != m_Chunks.end();)
        {
            if (isOutOfRange(it->first))
            {
                ReleaseChunkMesh(it->second);
                m_PendingGenerate.erase(it->first);
                m_PendingRebuild.erase(it->first);
                it = m_Chunks.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_PendingGenerate.begin(); it != m_PendingGenerate.end();)
        {
            if (isOutOfRange(*it))
            {
                it = m_PendingGenerate.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_PendingRebuild.begin(); it != m_PendingRebuild.end();)
        {
            if (isOutOfRange(*it))
            {
                it = m_PendingRebuild.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_PendingChunkUpdates.begin(); it != m_PendingChunkUpdates.end();)
        {
            if (isOutOfRange(*it))
            {
                it = m_PendingChunkUpdates.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_ChunkUpdateQueue.begin(); it != m_ChunkUpdateQueue.end();)
        {
            if (isOutOfRange(*it))
            {
                it = m_ChunkUpdateQueue.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    {
        std::scoped_lock lock(m_TaskMutex);
        for (auto it = m_TaskQueue.begin(); it != m_TaskQueue.end();)
        {
            if (isOutOfRange(it->coord))
            {
                it = m_TaskQueue.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    {
        std::scoped_lock lock(m_ResultMutex);
        for (auto it = m_ResultQueue.begin(); it != m_ResultQueue.end();)
        {
            if (isOutOfRange(it->coord))
            {
                it = m_ResultQueue.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void TileMap::Render(Renderer::Renderer2D& renderer, const Renderer::Texture2D& atlas, const glm::vec2& cameraPos, const glm::vec2& viewportSize)
{
    // 提高每帧重建吞吐，避免“碰撞已更新但画面残留旧块”
    ProcessChunkUpdateQueue(32);
    ProcessCompletedTasks();

    const float left = cameraPos.x;
    const float bottom = cameraPos.y;
    const float right = cameraPos.x + viewportSize.x;
    const float top = cameraPos.y + viewportSize.y;

    const int minTileX = static_cast<int>(std::floor(left / kTileSize));
    const int maxTileX = static_cast<int>(std::floor((right - 1.0f) / kTileSize));
    const int minTileY = static_cast<int>(std::floor(bottom / kTileSize));
    const int maxTileY = static_cast<int>(std::floor((top - 1.0f) / kTileSize));

    const int minChunkX = FloorDiv(minTileX, Chunk::kSize);
    const int maxChunkX = FloorDiv(maxTileX, Chunk::kSize);
    const int minChunkY = FloorDiv(minTileY, Chunk::kSize);
    const int maxChunkY = FloorDiv(maxTileY, Chunk::kSize);

    std::scoped_lock lock(m_ChunkMutex);
    for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY)
    {
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            const ChunkCoord coord{chunkX, chunkY};
            auto it = m_Chunks.find(coord);
            if (it == m_Chunks.end())
            {
                continue;
            }

            Chunk& chunk = it->second;
            if (chunk.gpuUploadPending)
            {
                UploadChunkMesh(chunk);
            }

            renderer.DrawMesh(atlas, chunk.vao, chunk.indexCount);
        }
    }
}

void TileMap::UpdateSimulation()
{
    // 固定降速：约 120 FPS 下每 6 帧（~20Hz）模拟一次，避免肉眼“瞬塌”
    ++m_SimulationFrameCounter;
    if (m_SimulationFrameCounter < 6)
    {
        return;
    }
    m_SimulationFrameCounter = 0;

    SimulateFallingStep(12);
}

void TileMap::SetTileWorld(int worldTileX, int worldTileY, std::uint16_t id)
{
    const int chunkX = FloorDiv(worldTileX, Chunk::kSize);
    const int chunkY = FloorDiv(worldTileY, Chunk::kSize);
    const int localX = PositiveMod(worldTileX, Chunk::kSize);
    const int localY = PositiveMod(worldTileY, Chunk::kSize);

    const ChunkCoord coord{chunkX, chunkY};
    bool needRebuild = false;
    {
        std::scoped_lock lock(m_ChunkMutex);
        auto it = m_Chunks.find(coord);
        if (it == m_Chunks.end())
        {
            // 锁外再入队，避免锁顺序反转
        }
        else
        {
            Chunk& chunk = it->second;
            Tile& tile = chunk.tiles[static_cast<size_t>(localY * Chunk::kSize + localX)];
            if (tile.id != id)
            {
                tile.id = id;
                tile.hardness = GetHardnessForTileId(id);
                needRebuild = true;
            }
        }
    }

    if (needRebuild)
    {
        EnqueueChunkUpdate(coord);
        return;
    }

    {
        std::scoped_lock lock(m_ChunkMutex);
        if (m_Chunks.find(coord) != m_Chunks.end())
        {
            return;
        }
    }
    EnqueueGenerateTask(coord);
}

std::uint16_t TileMap::GetTileIdWorld(int worldTileX, int worldTileY) const
{
    const int chunkX = FloorDiv(worldTileX, Chunk::kSize);
    const int chunkY = FloorDiv(worldTileY, Chunk::kSize);
    const int localX = PositiveMod(worldTileX, Chunk::kSize);
    const int localY = PositiveMod(worldTileY, Chunk::kSize);

    std::scoped_lock lock(m_ChunkMutex);
    const auto it = m_Chunks.find(ChunkCoord{chunkX, chunkY});
    if (it == m_Chunks.end())
    {
        return Material::Air;
    }

    const Tile& tile = it->second.tiles[static_cast<size_t>(localY * Chunk::kSize + localX)];
    return tile.id;
}

float TileMap::GetTileHardnessWorld(int worldTileX, int worldTileY) const
{
    const int chunkX = FloorDiv(worldTileX, Chunk::kSize);
    const int chunkY = FloorDiv(worldTileY, Chunk::kSize);
    const int localX = PositiveMod(worldTileX, Chunk::kSize);
    const int localY = PositiveMod(worldTileY, Chunk::kSize);

    std::scoped_lock lock(m_ChunkMutex);
    const auto it = m_Chunks.find(ChunkCoord{chunkX, chunkY});
    if (it == m_Chunks.end())
    {
        return 0.0f;
    }

    const Tile& tile = it->second.tiles[static_cast<size_t>(localY * Chunk::kSize + localX)];
    return tile.hardness;
}

bool TileMap::HasChunkAtWorldTile(int worldTileX, int worldTileY) const
{
    const int chunkX = FloorDiv(worldTileX, Chunk::kSize);
    const int chunkY = FloorDiv(worldTileY, Chunk::kSize);

    std::scoped_lock lock(m_ChunkMutex);
    return m_Chunks.find(ChunkCoord{chunkX, chunkY}) != m_Chunks.end();
}

bool TileMap::IsSolidWorldTile(int worldTileX, int worldTileY) const
{
    const int chunkX = FloorDiv(worldTileX, Chunk::kSize);
    const int chunkY = FloorDiv(worldTileY, Chunk::kSize);
    const int localX = PositiveMod(worldTileX, Chunk::kSize);
    const int localY = PositiveMod(worldTileY, Chunk::kSize);

    std::scoped_lock lock(m_ChunkMutex);
    const auto it = m_Chunks.find(ChunkCoord{chunkX, chunkY});
    if (it == m_Chunks.end())
    {
        return false;
    }

    const Tile& tile = it->second.tiles[static_cast<size_t>(localY * Chunk::kSize + localX)];
    return tile.id != Material::Air && tile.id != Material::Water;
}

std::size_t TileMap::LoadedChunkCount() const
{
    std::scoped_lock lock(m_ChunkMutex);
    return m_Chunks.size();
}

void TileMap::StartWorker()
{
    m_WorkerRunning = true;
    m_WorkerThread = std::thread(&TileMap::WorkerLoop, this);
}

void TileMap::StopWorker()
{
    {
        std::scoped_lock lock(m_TaskMutex);
        m_WorkerRunning = false;
    }
    m_TaskCv.notify_all();

    if (m_WorkerThread.joinable())
    {
        m_WorkerThread.join();
    }
}

void TileMap::WorkerLoop()
{
    while (true)
    {
        WorkerTask task;
        {
            std::unique_lock lock(m_TaskMutex);
            m_TaskCv.wait(lock, [&]() { return !m_WorkerRunning || !m_TaskQueue.empty(); });

            if (!m_WorkerRunning && m_TaskQueue.empty())
            {
                return;
            }

            task = std::move(m_TaskQueue.front());
            m_TaskQueue.pop_front();
        }

        WorkerResult result{};
        result.coord = task.coord;

        if (task.type == WorkerTask::Type::Generate)
        {
            BuildChunkTiles(task.coord, result.tiles);
            result.hasTiles = true;
            BuildChunkMesh(task.coord, result.tiles, result.vertices, result.indices);
        }
        else
        {
            result.tiles = task.tiles;
            result.hasTiles = false;
            BuildChunkMesh(task.coord, task.tiles, result.vertices, result.indices);
        }

        {
            std::scoped_lock lock(m_ResultMutex);
            m_ResultQueue.push_back(std::move(result));
        }
    }
}

void TileMap::EnqueueGenerateTask(const ChunkCoord& coord)
{
    bool shouldEnqueue = false;
    {
        std::scoped_lock chunkLock(m_ChunkMutex);
        if (m_Chunks.find(coord) == m_Chunks.end() && m_PendingGenerate.find(coord) == m_PendingGenerate.end())
        {
            m_PendingGenerate.insert(coord);
            shouldEnqueue = true;
        }
    }

    if (!shouldEnqueue)
    {
        return;
    }

    {
        std::scoped_lock lock(m_TaskMutex);
        WorkerTask task{};
        task.type = WorkerTask::Type::Generate;
        task.coord = coord;
        m_TaskQueue.push_back(std::move(task));
    }
    m_TaskCv.notify_one();
}

void TileMap::EnqueueRebuildTask(const ChunkCoord& coord, const std::array<Tile, Chunk::kSize * Chunk::kSize>& tiles)
{
    {
        std::scoped_lock lock(m_ChunkMutex);
        if (m_PendingRebuild.find(coord) != m_PendingRebuild.end())
        {
            return;
        }
        m_PendingRebuild.insert(coord);
    }

    {
        std::scoped_lock lock(m_TaskMutex);
        WorkerTask task{};
        task.type = WorkerTask::Type::Rebuild;
        task.coord = coord;
        task.tiles = tiles;
        m_TaskQueue.push_back(std::move(task));
    }
    m_TaskCv.notify_one();
}

void TileMap::EnqueueChunkUpdate(const ChunkCoord& coord)
{
    std::scoped_lock lock(m_ChunkMutex);
    if (m_Chunks.find(coord) == m_Chunks.end())
    {
        return;
    }
    if (m_PendingChunkUpdates.find(coord) != m_PendingChunkUpdates.end())
    {
        return;
    }
    m_PendingChunkUpdates.insert(coord);
    m_ChunkUpdateQueue.push_back(coord);
}

void TileMap::SimulateFallingStep(int maxMovesPerFrame)
{
    if (maxMovesPerFrame <= 0)
    {
        return;
    }

    std::unordered_set<ChunkCoord, ChunkCoordHash> touchedChunks;
    touchedChunks.reserve(16);

    int minWorldTileX = std::numeric_limits<int>::max();
    int minWorldTileY = std::numeric_limits<int>::max();
    int maxWorldTileX = std::numeric_limits<int>::min();
    int maxWorldTileY = std::numeric_limits<int>::min();

    {
        std::scoped_lock lock(m_ChunkMutex);
        if (m_Chunks.empty())
        {
            return;
        }

        for (const auto& [coord, _chunk] : m_Chunks)
        {
            const int chunkMinX = coord.x * Chunk::kSize;
            const int chunkMinY = coord.y * Chunk::kSize;
            const int chunkMaxX = chunkMinX + Chunk::kSize - 1;
            const int chunkMaxY = chunkMinY + Chunk::kSize - 1;

            minWorldTileX = std::min(minWorldTileX, chunkMinX);
            minWorldTileY = std::min(minWorldTileY, chunkMinY);
            maxWorldTileX = std::max(maxWorldTileX, chunkMaxX);
            maxWorldTileY = std::max(maxWorldTileY, chunkMaxY);
        }

        auto tryGetTilePtr = [&](int worldTileX, int worldTileY) -> Tile* {
            const int chunkX = FloorDiv(worldTileX, Chunk::kSize);
            const int chunkY = FloorDiv(worldTileY, Chunk::kSize);
            const int localX = PositiveMod(worldTileX, Chunk::kSize);
            const int localY = PositiveMod(worldTileY, Chunk::kSize);

            auto it = m_Chunks.find(ChunkCoord{chunkX, chunkY});
            if (it == m_Chunks.end())
            {
                return nullptr;
            }

            Chunk& chunk = it->second;
            return &chunk.tiles[static_cast<size_t>(localY * Chunk::kSize + localX)];
        };

        int moved = 0;
        // 关键：从上往下扫描，避免同一帧链式“整列瞬塌”
        for (int worldY = maxWorldTileY; worldY >= minWorldTileY + 1 && moved < maxMovesPerFrame; --worldY)
        {
            for (int worldX = minWorldTileX; worldX <= maxWorldTileX && moved < maxMovesPerFrame; ++worldX)
            {
                Tile* current = tryGetTilePtr(worldX, worldY);
                Tile* below = tryGetTilePtr(worldX, worldY - 1);
                if (!current || !below)
                {
                    continue;
                }

                const bool isFallingMaterial = (current->id == Material::Sand || current->id == Material::Water);
                if (!isFallingMaterial)
                {
                    continue;
                }

                if (below->id != Material::Air)
                {
                    continue;
                }

                below->id = current->id;
                below->hardness = current->hardness;
                current->id = Material::Air;
                current->hardness = 0.0f;

                touchedChunks.insert(ChunkCoord{FloorDiv(worldX, Chunk::kSize), FloorDiv(worldY, Chunk::kSize)});
                touchedChunks.insert(ChunkCoord{FloorDiv(worldX, Chunk::kSize), FloorDiv(worldY - 1, Chunk::kSize)});
                ++moved;
            }
        }
    }

    for (const ChunkCoord& coord : touchedChunks)
    {
        EnqueueChunkUpdate(coord);
    }
}

void TileMap::ProcessChunkUpdateQueue(int maxUpdatesPerFrame)
{
    std::vector<std::pair<ChunkCoord, std::array<Tile, Chunk::kSize * Chunk::kSize>>> rebuildList;
    rebuildList.reserve(static_cast<size_t>(std::max(0, maxUpdatesPerFrame)));

    {
        std::scoped_lock lock(m_ChunkMutex);
        int processed = 0;
        while (processed < maxUpdatesPerFrame && !m_ChunkUpdateQueue.empty())
        {
            const ChunkCoord coord = m_ChunkUpdateQueue.front();
            m_ChunkUpdateQueue.pop_front();
            m_PendingChunkUpdates.erase(coord);

            if (m_PendingRebuild.find(coord) != m_PendingRebuild.end())
            {
                ++processed;
                continue;
            }

            auto it = m_Chunks.find(coord);
            if (it != m_Chunks.end())
            {
                rebuildList.emplace_back(coord, it->second.tiles);
            }

            ++processed;
        }
    }

    for (const auto& [coord, tiles] : rebuildList)
    {
        EnqueueRebuildTask(coord, tiles);
    }
}

void TileMap::ProcessCompletedTasks()
{
    std::deque<WorkerResult> localResults;
    {
        std::scoped_lock lock(m_ResultMutex);
        if (m_ResultQueue.empty())
        {
            return;
        }
        localResults.swap(m_ResultQueue);
    }

    std::scoped_lock lock(m_ChunkMutex);
    for (auto& result : localResults)
    {
        if (result.hasTiles)
        {
            if (m_PendingGenerate.find(result.coord) == m_PendingGenerate.end())
            {
                continue;
            }

            Chunk& chunk = m_Chunks[result.coord];
            chunk.tiles = result.tiles;
            m_PendingGenerate.erase(result.coord);

            chunk.vertices = std::move(result.vertices);
            chunk.indices = std::move(result.indices);
            chunk.gpuUploadPending = true;
        }
        else
        {
            if (m_PendingRebuild.find(result.coord) == m_PendingRebuild.end())
            {
                continue;
            }

            m_PendingRebuild.erase(result.coord);
            auto chunkIt = m_Chunks.find(result.coord);
            if (chunkIt == m_Chunks.end())
            {
                continue;
            }

            Chunk& chunk = chunkIt->second;
            chunk.vertices = std::move(result.vertices);
            chunk.indices = std::move(result.indices);
            chunk.gpuUploadPending = true;
        }
    }
}

void TileMap::SaveWorldToFile() const
{
    namespace fs = std::filesystem;

    std::error_code ec;
    const fs::path savePath(m_SaveFilePath);
    if (savePath.has_parent_path())
    {
        fs::create_directories(savePath.parent_path(), ec);
    }

    std::ofstream out(m_SaveFilePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        return;
    }

    constexpr std::uint32_t kMagic = 0x43554245; // 'CUBE'
    constexpr std::uint32_t kVersion = 2;

    std::scoped_lock lock(m_ChunkMutex);
    const std::uint32_t chunkCount = static_cast<std::uint32_t>(m_Chunks.size());

    out.write(reinterpret_cast<const char*>(&kMagic), sizeof(kMagic));
    out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));
    out.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));

    for (const auto& [coord, chunk] : m_Chunks)
    {
        out.write(reinterpret_cast<const char*>(&coord.x), sizeof(coord.x));
        out.write(reinterpret_cast<const char*>(&coord.y), sizeof(coord.y));
        out.write(reinterpret_cast<const char*>(chunk.tiles.data()),
                  static_cast<std::streamsize>(chunk.tiles.size() * sizeof(Tile)));
    }
}

void TileMap::LoadWorldFromFile()
{
    std::ifstream in(m_SaveFilePath, std::ios::binary);
    if (!in.is_open())
    {
        return;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t chunkCount = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));

    constexpr std::uint32_t kMagic = 0x43554245; // 'CUBE'
    if (!in.good() || magic != kMagic || (version != 1 && version != 2))
    {
        return;
    }

    std::vector<std::pair<ChunkCoord, std::array<Tile, Chunk::kSize * Chunk::kSize>>> loaded;
    loaded.reserve(chunkCount);

    for (std::uint32_t i = 0; i < chunkCount; ++i)
    {
        ChunkCoord coord{};
        std::array<Tile, Chunk::kSize * Chunk::kSize> tiles{};

        in.read(reinterpret_cast<char*>(&coord.x), sizeof(coord.x));
        in.read(reinterpret_cast<char*>(&coord.y), sizeof(coord.y));

        if (version == 2)
        {
            in.read(reinterpret_cast<char*>(tiles.data()), static_cast<std::streamsize>(tiles.size() * sizeof(Tile)));
        }
        else
        {
            // 旧存档（v1）仅保存 tile id，需重建 hardness
            std::array<std::uint16_t, Chunk::kSize * Chunk::kSize> ids{};
            in.read(reinterpret_cast<char*>(ids.data()), static_cast<std::streamsize>(ids.size() * sizeof(std::uint16_t)));
            if (in.good())
            {
                for (size_t t = 0; t < ids.size(); ++t)
                {
                    tiles[t].id = ids[t];
                    tiles[t].hardness = GetHardnessForTileId(ids[t]);
                }
            }
        }

        if (!in.good())
        {
            break;
        }

        // 防御式修正：确保 hardness 与 id 对齐（兼容异常旧数据）
        for (Tile& tile : tiles)
        {
            tile.hardness = GetHardnessForTileId(tile.id);
        }

        loaded.emplace_back(coord, tiles);
    }

    {
        std::scoped_lock lock(m_ChunkMutex);
        for (const auto& [coord, tiles] : loaded)
        {
            Chunk& chunk = m_Chunks[coord];
            chunk.tiles = tiles;
            chunk.gpuUploadPending = false;
        }
    }

    for (const auto& [coord, tiles] : loaded)
    {
        EnqueueRebuildTask(coord, tiles);
    }
}

void TileMap::BuildChunkTiles(const ChunkCoord& coord, std::array<Tile, Chunk::kSize * Chunk::kSize>& outTiles)
{
    constexpr int kWaterLevel = 8;
    constexpr int kSandLayerThickness = 6;

    for (int localY = 0; localY < Chunk::kSize; ++localY)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const int worldTileX = coord.x * Chunk::kSize + localX;
            const int worldTileY = coord.y * Chunk::kSize + localY;
            const int groundHeight = GetGroundHeightAtWorldX(worldTileX);

            Tile& tile = outTiles[static_cast<size_t>(localY * Chunk::kSize + localX)];
            if (worldTileY <= groundHeight)
            {
                // 靠近地表使用沙子，更深层用泥土
                tile.id = (worldTileY >= groundHeight - (kSandLayerThickness - 1)) ? Material::Sand : Material::Dirt;
            }
            else if (worldTileY <= kWaterLevel)
            {
                tile.id = Material::Water;
            }
            else
            {
                tile.id = Material::Air;
            }
            tile.hardness = GetHardnessForTileId(tile.id);
        }
    }
}

void TileMap::BuildChunkMesh(const ChunkCoord& coord,
                             const std::array<Tile, Chunk::kSize * Chunk::kSize>& tiles,
                             std::vector<Vertex>& outVertices,
                             std::vector<std::uint32_t>& outIndices)
{
    outVertices.clear();
    outIndices.clear();

    constexpr float atlasCols = 5.0f;
    constexpr float atlasRows = 1.0f;
    constexpr float cellW = 1.0f / atlasCols;
    constexpr float cellH = 1.0f / atlasRows;

    // 简单 2D 光照传播：
    // 1) 顶部天空光灌入
    // 2) 4 邻域衰减扩散
    std::array<float, Chunk::kSize * Chunk::kSize> light{};

    auto cellIndex = [](int x, int y) { return static_cast<size_t>(y * Chunk::kSize + x); };

    // 顶部天空光（按世界高度图注入，避免地下 chunk 顶部产生伪光带）
    for (int x = 0; x < Chunk::kSize; ++x)
    {
        const int worldTileX = coord.x * Chunk::kSize + x;
        const int groundHeight = GetGroundHeightAtWorldX(worldTileX);
        for (int y = Chunk::kSize - 1; y >= 0; --y)
        {
            const int worldTileY = coord.y * Chunk::kSize + y;
            const size_t idx = cellIndex(x, y);

            if (worldTileY > groundHeight && tiles[idx].id == Material::Air)
            {
                light[idx] = 1.0f;
            }
        }
    }

    // BFS 衰减扩散
    std::deque<int> queue;
    for (int y = 0; y < Chunk::kSize; ++y)
    {
        for (int x = 0; x < Chunk::kSize; ++x)
        {
            const size_t idx = cellIndex(x, y);
            if (light[idx] > 0.01f)
            {
                queue.push_back(static_cast<int>(idx));
            }
        }
    }

    const auto tryRelax = [&](int fromX, int fromY, int toX, int toY) {
        if (toX < 0 || toX >= Chunk::kSize || toY < 0 || toY >= Chunk::kSize)
        {
            return;
        }

        const size_t fromIdx = cellIndex(fromX, fromY);
        const size_t toIdx = cellIndex(toX, toY);
        const bool toSolid = (tiles[toIdx].id != Material::Air && tiles[toIdx].id != Material::Water);

        const float decay = toSolid ? 0.22f : 0.10f;
        const float candidate = light[fromIdx] - decay;
        if (candidate > light[toIdx] + 0.01f && candidate > 0.0f)
        {
            light[toIdx] = candidate;
            queue.push_back(static_cast<int>(toIdx));
        }
    };

    while (!queue.empty())
    {
        const int idx = queue.front();
        queue.pop_front();

        const int x = idx % Chunk::kSize;
        const int y = idx / Chunk::kSize;

        tryRelax(x, y, x + 1, y);
        tryRelax(x, y, x - 1, y);
        tryRelax(x, y, x, y + 1);
        tryRelax(x, y, x, y - 1);
    }

    // 量化光照用于 greedy 合并约束（避免把亮暗差异过大的面合并）
    std::array<std::uint8_t, Chunk::kSize * Chunk::kSize> lightBucket{};
    for (int y = 0; y < Chunk::kSize; ++y)
    {
        for (int x = 0; x < Chunk::kSize; ++x)
        {
            const size_t idx = cellIndex(x, y);
            const float l = std::clamp(light[idx], 0.0f, 1.0f);
            lightBucket[idx] = static_cast<std::uint8_t>(std::round(l * 15.0f));
        }
    }

    std::array<std::uint8_t, Chunk::kSize * Chunk::kSize> consumed{};

    for (int localY = 0; localY < Chunk::kSize; ++localY)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const size_t startIndex = static_cast<size_t>(localY * Chunk::kSize + localX);
            if (consumed[startIndex] != 0)
            {
                continue;
            }

            const std::uint16_t tileId = tiles[startIndex].id;
            const std::uint8_t bucket = lightBucket[startIndex];
            if (tileId == Material::Air)
            {
                consumed[startIndex] = 1;
                continue;
            }

            int mergeWidth = 1;
            while (localX + mergeWidth < Chunk::kSize)
            {
                const size_t idx = static_cast<size_t>(localY * Chunk::kSize + (localX + mergeWidth));
                if (consumed[idx] != 0 || tiles[idx].id != tileId || lightBucket[idx] != bucket)
                {
                    break;
                }
                ++mergeWidth;
            }

            int mergeHeight = 1;
            while (localY + mergeHeight < Chunk::kSize)
            {
                bool canExpand = true;
                for (int dx = 0; dx < mergeWidth; ++dx)
                {
                    const size_t idx = static_cast<size_t>((localY + mergeHeight) * Chunk::kSize + (localX + dx));
                    if (consumed[idx] != 0 || tiles[idx].id != tileId || lightBucket[idx] != bucket)
                    {
                        canExpand = false;
                        break;
                    }
                }
                if (!canExpand)
                {
                    break;
                }
                ++mergeHeight;
            }

            for (int dy = 0; dy < mergeHeight; ++dy)
            {
                for (int dx = 0; dx < mergeWidth; ++dx)
                {
                    const size_t idx = static_cast<size_t>((localY + dy) * Chunk::kSize + (localX + dx));
                    consumed[idx] = 1;
                }
            }

            const int worldTileX = coord.x * Chunk::kSize + localX;
            const int worldTileY = coord.y * Chunk::kSize + localY;

            const float x0 = worldTileX * kTileSize;
            const float y0 = worldTileY * kTileSize;
            const float x1 = x0 + static_cast<float>(mergeWidth) * kTileSize;
            const float y1 = y0 + static_cast<float>(mergeHeight) * kTileSize;

            const int atlasX = static_cast<int>(tileId);
            const int atlasY = 0;
            const float u0 = atlasX * cellW;
            const float v0 = atlasY * cellH;
            const float u1 = (atlasX + 1) * cellW;
            const float v1 = (atlasY + 1) * cellH;

            const float vertexLight = static_cast<float>(bucket) / 15.0f;

            const std::uint32_t baseIndex = static_cast<std::uint32_t>(outVertices.size());
            outVertices.push_back(Vertex{glm::vec3(x0, y0, 0.0f), glm::vec2(u0, v0), vertexLight});
            outVertices.push_back(Vertex{glm::vec3(x1, y0, 0.0f), glm::vec2(u1, v0), vertexLight});
            outVertices.push_back(Vertex{glm::vec3(x1, y1, 0.0f), glm::vec2(u1, v1), vertexLight});
            outVertices.push_back(Vertex{glm::vec3(x0, y1, 0.0f), glm::vec2(u0, v1), vertexLight});

            outIndices.push_back(baseIndex + 0);
            outIndices.push_back(baseIndex + 1);
            outIndices.push_back(baseIndex + 2);
            outIndices.push_back(baseIndex + 2);
            outIndices.push_back(baseIndex + 3);
            outIndices.push_back(baseIndex + 0);
        }
    }
}

void TileMap::UploadChunkMesh(Chunk& chunk)
{
    if (chunk.vao == 0)
    {
        glGenVertexArrays(1, &chunk.vao);
        glGenBuffers(1, &chunk.vbo);
        glGenBuffers(1, &chunk.ebo);
    }

    glBindVertexArray(chunk.vao);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(chunk.vertices.size() * sizeof(Vertex)),
                 chunk.vertices.empty() ? nullptr : chunk.vertices.data(),
                 GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(chunk.indices.size() * sizeof(std::uint32_t)),
                 chunk.indices.empty() ? nullptr : chunk.indices.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, light)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    chunk.indexCount = static_cast<unsigned int>(chunk.indices.size());
    chunk.gpuUploadPending = false;
}

void TileMap::ReleaseChunkMesh(Chunk& chunk)
{
    if (chunk.ebo != 0)
    {
        glDeleteBuffers(1, &chunk.ebo);
        chunk.ebo = 0;
    }
    if (chunk.vbo != 0)
    {
        glDeleteBuffers(1, &chunk.vbo);
        chunk.vbo = 0;
    }
    if (chunk.vao != 0)
    {
        glDeleteVertexArrays(1, &chunk.vao);
        chunk.vao = 0;
    }
    chunk.indexCount = 0;
}

} // namespace GameLayer
