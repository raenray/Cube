#pragma once

#include "Game/Inventory.h"
#include "Game/TileMap.h"
#include "Renderer/Camera2D.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Texture2D.h"

#include <vector>

namespace GameLayer
{

struct Player
{
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 velocity{0.0f, 0.0f};
    glm::vec2 size{TileMap::kTileSize * 0.8f, TileMap::kTileSize * 1.6f};
    bool onGround = false;
};

class Game
{
public:
    Game(int viewportWidth, int viewportHeight);

    void Update(float deltaTime);
    void Render();
    void RenderUI() const;

    const Inventory& GetInventory() const { return m_Inventory; }

private:
    struct DroppedItem
    {
        glm::vec2 position{0.0f, 0.0f};
        Item item{};
        glm::vec2 velocity{0.0f, 0.0f};
    };

    glm::vec2 ScreenToWorld(const glm::vec2& mouseScreen) const;
    void SpawnDroppedItem(std::uint16_t itemId, const glm::vec2& worldPos);
    void UpdateDroppedItems(float deltaTime);
    void CollectNearbyDroppedItems();
    void RenderDroppedItems();
    void ResetMiningState();
    float ResolveMiningPower() const;
    bool IsSolidAtAABB(const glm::vec2& pos, const glm::vec2& size) const;
    void ResolveHorizontal(float deltaX);
    void ResolveVertical(float deltaY);
    void ResolveInitialSpawnIfNeeded();

private:
    Player m_Player;
    Renderer::Camera2D m_Camera;
    Renderer::Renderer2D m_Renderer;
    Renderer::Texture2D m_TileAtlas;
    TileMap m_TileMap;
    Inventory m_Inventory;
    std::vector<DroppedItem> m_DroppedItems;
    int m_MiningTileX = 0;
    int m_MiningTileY = 0;
    bool m_HasMiningTarget = false;
    float m_MiningProgress = 0.0f;
    float m_TimeAccumulator = 0.0f;
    bool m_JumpPressedLastFrame = false;
    bool m_SpawnResolved = false;
};

} // namespace GameLayer
