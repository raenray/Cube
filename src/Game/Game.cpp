#include "Game/Game.h"

#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

#include "Core/Input.h"
#include "Game/ItemDatabase.h"
#include "UI/InventoryUI.h"

namespace GameLayer
{

namespace
{

std::array<std::uint8_t, 5 * 1 * 4> CreateSimpleAtlasPixels()
{
    std::array<std::uint8_t, 5 * 1 * 4> pixels{};

    // tile 0: air (transparent)
    pixels[0] = 0;
    pixels[1] = 0;
    pixels[2] = 0;
    pixels[3] = 0;

    // tile 1: ground (brown)
    pixels[4] = 120;
    pixels[5] = 85;
    pixels[6] = 45;
    pixels[7] = 255;

    // tile 2: sand (yellow)
    pixels[8] = 210;
    pixels[9] = 190;
    pixels[10] = 120;
    pixels[11] = 255;

    // tile 3: water (blue)
    pixels[12] = 40;
    pixels[13] = 120;
    pixels[14] = 220;
    pixels[15] = 220;

    // tile 4: player (bright blue)
    pixels[16] = 70;
    pixels[17] = 140;
    pixels[18] = 255;
    pixels[19] = 255;

    return pixels;
}

} // namespace

Game::Game(int viewportWidth, int viewportHeight)
    : m_Camera(static_cast<float>(viewportWidth), static_cast<float>(viewportHeight))
    , m_TileAtlas(5, 1, CreateSimpleAtlasPixels().data())
{
    m_Player.position = glm::vec2(64.0f, 240.0f);
    // 默认给一个基础工具（当前用 Debug Player Token 作为 Tool）
    m_Inventory.AddItem(Material::Player, 1);
}

void Game::Update(float deltaTime)
{
    m_TimeAccumulator += deltaTime;

    constexpr float moveSpeed = 300.0f;
    constexpr float gravity = -1400.0f;
    constexpr float jumpSpeed = 560.0f;

    float horizontalInput = 0.0f;
    if (Core::Input::IsKeyPressed(GLFW_KEY_A))
    {
        horizontalInput -= 1.0f;
    }
    if (Core::Input::IsKeyPressed(GLFW_KEY_D))
    {
        horizontalInput += 1.0f;
    }

    const bool jumpPressed = Core::Input::IsKeyPressed(GLFW_KEY_SPACE);
    ResolveInitialSpawnIfNeeded();

    // 出生点未解析前，不执行常规物理，避免持续下落到地底
    if (!m_SpawnResolved)
    {
        const glm::vec2 playerCenter = m_Player.position + m_Player.size * 0.5f;
        m_Camera.SetPosition(playerCenter - glm::vec2(m_Camera.GetViewportWidth() * 0.5f, m_Camera.GetViewportHeight() * 0.5f));

        m_TileMap.EnsureChunksAround(playerCenter, 2);
        return;
    }

    if (jumpPressed && !m_JumpPressedLastFrame && m_Player.onGround)
    {
        m_Player.velocity.y = jumpSpeed;
        m_Player.onGround = false;
    }
    m_JumpPressedLastFrame = jumpPressed;

    m_Player.velocity.x = horizontalInput * moveSpeed;
    m_Player.velocity.y += gravity * deltaTime;

    ResolveHorizontal(m_Player.velocity.x * deltaTime);
    ResolveVertical(m_Player.velocity.y * deltaTime);

    const glm::vec2 playerCenter = m_Player.position + m_Player.size * 0.5f;
    m_Camera.SetPosition(playerCenter - glm::vec2(m_Camera.GetViewportWidth() * 0.5f, m_Camera.GetViewportHeight() * 0.5f));

    // 以当前视口中心作为加载中心，避免向右移动时右侧可见区域短暂缺块
    const glm::vec2 loadCenter =
        m_Camera.GetPosition() + glm::vec2(m_Camera.GetViewportWidth() * 0.5f, m_Camera.GetViewportHeight() * 0.5f);
    m_TileMap.EnsureChunksAround(loadCenter, 2);
    m_TileMap.UnloadFarChunks(loadCenter, 4);
    m_TileMap.UpdateSimulation();

    UpdateDroppedItems(deltaTime);
    CollectNearbyDroppedItems();

    const glm::vec2 mouseScreen = Core::Input::GetMousePosition();
    const glm::vec2 mouseWorld = ScreenToWorld(mouseScreen);
    const int tileX = static_cast<int>(std::floor(mouseWorld.x / TileMap::kTileSize));
    const int tileY = static_cast<int>(std::floor(mouseWorld.y / TileMap::kTileSize));

    if (Core::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
    {
        const std::uint16_t tileId = m_TileMap.GetTileIdWorld(tileX, tileY);
        if (tileId == Material::Air)
        {
            ResetMiningState();
        }
        else
        {
            if (!m_HasMiningTarget || tileX != m_MiningTileX || tileY != m_MiningTileY)
            {
                m_HasMiningTarget = true;
                m_MiningTileX = tileX;
                m_MiningTileY = tileY;
                m_MiningProgress = 0.0f;
            }

            const float hardness = m_TileMap.GetTileHardnessWorld(m_MiningTileX, m_MiningTileY);
            if (hardness <= 0.0f || hardness >= 999999.0f)
            {
                // 近似不可挖
                m_MiningProgress = 0.0f;
            }
            else
            {
                const float miningPower = ResolveMiningPower();
                m_MiningProgress += deltaTime * miningPower;

                if (m_MiningProgress >= hardness)
                {
                    const glm::vec2 dropPos((m_MiningTileX + 0.5f) * TileMap::kTileSize, (m_MiningTileY + 0.5f) * TileMap::kTileSize);
                    SpawnDroppedItem(tileId, dropPos);
                    m_TileMap.SetTileWorld(m_MiningTileX, m_MiningTileY, Material::Air);
                    ResetMiningState();
                }
            }
        }
    }
    else
    {
        ResetMiningState();
    }
    if (Core::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT))
    {
        m_TileMap.SetTileWorld(tileX, tileY, Material::Dirt); // 放：Dirt
    }
}

void Game::Render()
{
    m_Renderer.BeginScene(m_Camera);
    m_TileMap.Render(m_Renderer, m_TileAtlas, m_Camera.GetPosition(), glm::vec2(m_Camera.GetViewportWidth(), m_Camera.GetViewportHeight()));

    // player
    constexpr float cellW = 1.0f / 5.0f;
    const glm::vec2 playerUvMin(static_cast<float>(Material::Player) * cellW, 0.0f);
    const glm::vec2 playerUvMax((static_cast<float>(Material::Player) + 1.0f) * cellW, 1.0f);
    m_Renderer.DrawTexturedQuad(m_Player.position, m_Player.size, playerUvMin, playerUvMax, m_TileAtlas);

    RenderDroppedItems();

    m_Renderer.EndScene();
}

void Game::RenderUI() const
{
    UI::DrawInventoryUI(m_Inventory);
}

void Game::SpawnDroppedItem(std::uint16_t itemId, const glm::vec2& worldPos)
{
    DroppedItem drop{};
    drop.position = worldPos;
    drop.item.id = itemId;
    drop.item.count = 1;
    drop.item.maxStack = 64;
    drop.velocity = glm::vec2(0.0f, 70.0f);
    m_DroppedItems.push_back(drop);
}

void Game::UpdateDroppedItems(float deltaTime)
{
    constexpr float dropGravity = -900.0f;
    const glm::vec2 dropHalfSize(TileMap::kTileSize * 0.3f, TileMap::kTileSize * 0.3f);

    for (DroppedItem& drop : m_DroppedItems)
    {
        drop.velocity.y += dropGravity * deltaTime;

        glm::vec2 nextPos = drop.position + drop.velocity * deltaTime;

        // 简单落地：仅处理向下速度，与地块顶部对齐（不做复杂碰撞）
        if (drop.velocity.y <= 0.0f)
        {
            const float footY = nextPos.y - dropHalfSize.y;
            const int footTileY = static_cast<int>(std::floor(footY / TileMap::kTileSize));

            const float leftX = nextPos.x - dropHalfSize.x * 0.8f;
            const float rightX = nextPos.x + dropHalfSize.x * 0.8f;
            const int leftTileX = static_cast<int>(std::floor(leftX / TileMap::kTileSize));
            const int rightTileX = static_cast<int>(std::floor(rightX / TileMap::kTileSize));

            const bool hitGround =
                m_TileMap.IsSolidWorldTile(leftTileX, footTileY) || m_TileMap.IsSolidWorldTile(rightTileX, footTileY);
            if (hitGround)
            {
                nextPos.y = (footTileY + 1) * TileMap::kTileSize + dropHalfSize.y;
                drop.velocity.y = 0.0f;
            }
        }

        drop.position = nextPos;
    }
}

void Game::CollectNearbyDroppedItems()
{
    constexpr float pickupRadius = TileMap::kTileSize * 1.5f;
    const glm::vec2 playerCenter = m_Player.position + m_Player.size * 0.5f;

    for (auto it = m_DroppedItems.begin(); it != m_DroppedItems.end();)
    {
        const glm::vec2 delta = it->position - playerCenter;
        const float dist2 = delta.x * delta.x + delta.y * delta.y;
        if (dist2 <= pickupRadius * pickupRadius)
        {
            if (m_Inventory.AddItem(it->item.id, it->item.count))
            {
                it = m_DroppedItems.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void Game::RenderDroppedItems()
{
    constexpr float cellW = 1.0f / 5.0f;
    const glm::vec2 dropSize(TileMap::kTileSize * 0.6f, TileMap::kTileSize * 0.6f);

    for (const DroppedItem& drop : m_DroppedItems)
    {
        const float tile = static_cast<float>(drop.item.id);
        const glm::vec2 uvMin(tile * cellW, 0.0f);
        const glm::vec2 uvMax((tile + 1.0f) * cellW, 1.0f);

        m_Renderer.DrawTexturedQuad(drop.position - dropSize * 0.5f, dropSize, uvMin, uvMax, m_TileAtlas);
    }
}

void Game::ResetMiningState()
{
    m_HasMiningTarget = false;
    m_MiningProgress = 0.0f;
}

float Game::ResolveMiningPower() const
{
    float bestPower = 1.0f; // 徒手可挖（约 0.8~1.2s），避免体感“挖不动”
    for (const Item& slot : m_Inventory.GetSlots())
    {
        if (slot.IsEmpty())
        {
            continue;
        }

        const ItemDef* def = ItemDatabase::GetItemDef(slot.id);
        if (!def)
        {
            continue;
        }

        if (def->type == ItemDef::Type::Tool)
        {
            bestPower = std::max(bestPower, def->miningPower);
        }
    }
    return bestPower;
}

glm::vec2 Game::ScreenToWorld(const glm::vec2& mouseScreen) const
{
    // GLFW 鼠标原点在左上，世界坐标原点在左下
    const float worldX = m_Camera.GetPosition().x + mouseScreen.x;
    const float worldY = m_Camera.GetPosition().y + (m_Camera.GetViewportHeight() - mouseScreen.y);
    return glm::vec2(worldX, worldY);
}

bool Game::IsSolidAtAABB(const glm::vec2& pos, const glm::vec2& size) const
{
    constexpr float epsilon = 0.001f;
    const float minXf = pos.x;
    const float minYf = pos.y;
    const float maxXf = pos.x + size.x - epsilon;
    const float maxYf = pos.y + size.y - epsilon;

    const int minTileX = static_cast<int>(std::floor(minXf / TileMap::kTileSize));
    const int maxTileX = static_cast<int>(std::floor(maxXf / TileMap::kTileSize));
    const int minTileY = static_cast<int>(std::floor(minYf / TileMap::kTileSize));
    const int maxTileY = static_cast<int>(std::floor(maxYf / TileMap::kTileSize));

    for (int y = minTileY; y <= maxTileY; ++y)
    {
        for (int x = minTileX; x <= maxTileX; ++x)
        {
            if (m_TileMap.IsSolidWorldTile(x, y))
            {
                return true;
            }
        }
    }

    return false;
}

void Game::ResolveHorizontal(float deltaX)
{
    if (deltaX == 0.0f)
    {
        return;
    }

    float remaining = deltaX;
    const float direction = (deltaX > 0.0f) ? 1.0f : -1.0f;
    const float stepMax = TileMap::kTileSize * 0.25f;

    int safety = 0;
    while (std::abs(remaining) > 0.0f && safety < 256)
    {
        const float step = direction * std::min(std::abs(remaining), stepMax);
        glm::vec2 next = m_Player.position;
        next.x += step;

        if (IsSolidAtAABB(next, m_Player.size))
        {
            m_Player.velocity.x = 0.0f;
            break;
        }

        m_Player.position = next;
        remaining -= step;
        ++safety;
    }
}

void Game::ResolveVertical(float deltaY)
{
    m_Player.onGround = false;
    if (deltaY == 0.0f)
    {
        return;
    }

    float remaining = deltaY;
    const float direction = (deltaY > 0.0f) ? 1.0f : -1.0f;
    const float stepMax = TileMap::kTileSize * 0.25f;

    int safety = 0;
    while (std::abs(remaining) > 0.0f && safety < 256)
    {
        const float step = direction * std::min(std::abs(remaining), stepMax);
        glm::vec2 next = m_Player.position;
        next.y += step;

        if (IsSolidAtAABB(next, m_Player.size))
        {
            if (deltaY < 0.0f)
            {
                m_Player.onGround = true;
            }
            m_Player.velocity.y = 0.0f;
            break;
        }

        m_Player.position = next;
        remaining -= step;
        ++safety;
    }
}

void Game::ResolveInitialSpawnIfNeeded()
{
    if (m_SpawnResolved)
    {
        return;
    }

    // 先保证出生点附近 chunk 已请求加载
    m_TileMap.EnsureChunksAround(m_Player.position + m_Player.size * 0.5f, 2);

    // 仅当出生点所在区域 chunk 已就绪时才解析出生，避免“把未加载当空气”导致落到地底
    const int spawnTileX = static_cast<int>(std::floor(m_Player.position.x / TileMap::kTileSize));
    const int spawnTileY = static_cast<int>(std::floor(m_Player.position.y / TileMap::kTileSize));
    constexpr int kSpawnSearchHalfWidthTiles = 12;
    const float searchTopY = m_Player.position.y + TileMap::kTileSize * 80.0f;
    const int searchTopTileY = static_cast<int>(std::floor(searchTopY / TileMap::kTileSize));

    // 只检查当前加载半径内应覆盖的高度区间，避免等待永远不会加载的远处 chunk
    const int probeY0 = spawnTileY - Chunk::kSize * 2;
    const int probeY1 = searchTopTileY;
    bool spawnAreaReady = true;
    for (int tx = spawnTileX - kSpawnSearchHalfWidthTiles; tx <= spawnTileX + kSpawnSearchHalfWidthTiles && spawnAreaReady; ++tx)
    {
        for (int ty = probeY0; ty <= probeY1; ty += 32)
        {
            if (!m_TileMap.HasChunkAtWorldTile(tx, ty))
            {
                spawnAreaReady = false;
                break;
            }
        }
    }

    if (!spawnAreaReady)
    {
        return;
    }

    // 从一段水平范围内“从上往下找地面”，选择最高可站立点，避免出生在竖井/地下空洞
    const float step = TileMap::kTileSize * 0.5f;

    auto FindStandYAtX = [&](float x, float& outY) {
        glm::vec2 probePos(x, searchTopY);

        int climbSafety = 0;
        while (IsSolidAtAABB(probePos, m_Player.size) && climbSafety < 512)
        {
            probePos.y += step;
            ++climbSafety;
        }

        for (int i = 0; i < 2048; ++i)
        {
            glm::vec2 next = probePos;
            next.y -= step;

            if (IsSolidAtAABB(next, m_Player.size))
            {
                outY = probePos.y;
                return true;
            }

            probePos = next;
        }

        return false;
    };

    bool foundAny = false;
    glm::vec2 bestSpawn = m_Player.position;

    for (int dx = -kSpawnSearchHalfWidthTiles; dx <= kSpawnSearchHalfWidthTiles; ++dx)
    {
        const int tileX = spawnTileX + dx;
        const float candidateX = tileX * TileMap::kTileSize + (TileMap::kTileSize - m_Player.size.x) * 0.5f;

        float standY = 0.0f;
        if (!FindStandYAtX(candidateX, standY))
        {
            continue;
        }

        if (!foundAny || standY > bestSpawn.y)
        {
            foundAny = true;
            bestSpawn = glm::vec2(candidateX, standY);
        }
    }

    if (foundAny)
    {
        m_Player.position = bestSpawn;
        m_Player.velocity = glm::vec2(0.0f, 0.0f);
        m_Player.onGround = true;
        m_SpawnResolved = true;
        return;
    }

    // 若暂时没找到地面（chunk 还没就绪），下一帧继续尝试
}

} // namespace GameLayer
