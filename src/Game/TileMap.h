#pragma once

#include <array>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Renderer
{
class Renderer2D;
class Texture2D;
} // namespace Renderer

namespace GameLayer
{

namespace Material
{
constexpr std::uint16_t Air = 0;
constexpr std::uint16_t Dirt = 1;
constexpr std::uint16_t Sand = 2;
constexpr std::uint16_t Water = 3;
constexpr std::uint16_t Player = 4;
} // namespace Material

struct Tile
{
    std::uint16_t id = Material::Air;
    float hardness = 0.0f;
};

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv;
    float light = 1.0f;
};

struct ChunkCoord
{
    int x = 0;
    int y = 0;

    bool operator==(const ChunkCoord& other) const { return x == other.x && y == other.y; }
};

struct ChunkCoordHash
{
    std::size_t operator()(const ChunkCoord& coord) const;
};

struct Chunk
{
    static constexpr int kSize = 32;
    std::array<Tile, kSize * kSize> tiles{};

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    unsigned int indexCount = 0;

    bool gpuUploadPending = false;
};

class TileMap
{
public:
    static constexpr float kTileSize = 16.0f;

    TileMap();
    ~TileMap();

    void EnsureChunksAround(const glm::vec2& worldCenter, int radiusInChunks);
    void UnloadFarChunks(const glm::vec2& worldCenter, int keepRadiusInChunks);
    void Render(Renderer::Renderer2D& renderer, const Renderer::Texture2D& atlas, const glm::vec2& cameraPos, const glm::vec2& viewportSize);
    void UpdateSimulation();

    void SetTileWorld(int worldTileX, int worldTileY, std::uint16_t id);
    std::uint16_t GetTileIdWorld(int worldTileX, int worldTileY) const;
    float GetTileHardnessWorld(int worldTileX, int worldTileY) const;
    bool HasChunkAtWorldTile(int worldTileX, int worldTileY) const;
    bool IsSolidWorldTile(int worldTileX, int worldTileY) const;

    std::size_t LoadedChunkCount() const;

private:
    struct WorkerTask
    {
        enum class Type
        {
            Generate,
            Rebuild
        };

        Type type = Type::Generate;
        ChunkCoord coord{};
        std::array<Tile, Chunk::kSize * Chunk::kSize> tiles{};
    };

    struct WorkerResult
    {
        ChunkCoord coord{};
        bool hasTiles = false;
        std::array<Tile, Chunk::kSize * Chunk::kSize> tiles{};
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
    };

private:
    void StartWorker();
    void StopWorker();
    void WorkerLoop();

    void EnqueueGenerateTask(const ChunkCoord& coord);
    void EnqueueRebuildTask(const ChunkCoord& coord, const std::array<Tile, Chunk::kSize * Chunk::kSize>& tiles);
    void EnqueueChunkUpdate(const ChunkCoord& coord);
    void SimulateFallingStep(int maxMovesPerFrame);
    void ProcessChunkUpdateQueue(int maxUpdatesPerFrame);
    void ProcessCompletedTasks();

    void SaveWorldToFile() const;
    void LoadWorldFromFile();

    static void BuildChunkTiles(const ChunkCoord& coord, std::array<Tile, Chunk::kSize * Chunk::kSize>& outTiles);
    static void BuildChunkMesh(const ChunkCoord& coord,
                               const std::array<Tile, Chunk::kSize * Chunk::kSize>& tiles,
                               std::vector<Vertex>& outVertices,
                               std::vector<std::uint32_t>& outIndices);
    void UploadChunkMesh(Chunk& chunk);

    void ReleaseChunkMesh(Chunk& chunk);

private:
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> m_Chunks;

    std::unordered_set<ChunkCoord, ChunkCoordHash> m_PendingGenerate;
    std::unordered_set<ChunkCoord, ChunkCoordHash> m_PendingRebuild;

    std::deque<WorkerTask> m_TaskQueue;
    std::deque<WorkerResult> m_ResultQueue;
    std::deque<ChunkCoord> m_ChunkUpdateQueue;
    std::unordered_set<ChunkCoord, ChunkCoordHash> m_PendingChunkUpdates;

    mutable std::mutex m_ChunkMutex;
    std::mutex m_TaskMutex;
    std::mutex m_ResultMutex;
    std::condition_variable m_TaskCv;

    bool m_WorkerRunning = false;
    std::thread m_WorkerThread;

    int m_SimulationFrameCounter = 0;

    std::string m_SaveFilePath = std::string(SAVE_DIR) + "/world.bin";
};

} // namespace GameLayer
