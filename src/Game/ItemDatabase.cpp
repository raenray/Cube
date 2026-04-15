#include "Game/ItemDatabase.h"

#include <unordered_map>

#include "Game/TileMap.h"

namespace GameLayer
{

namespace
{

const std::unordered_map<std::uint16_t, ItemDef>& GetDefs()
{
    static const std::unordered_map<std::uint16_t, ItemDef> kDefs = {
        {Material::Dirt, ItemDef{Material::Dirt, "Dirt", 64, ItemDef::Type::Normal, 1.0f}},
        {Material::Sand, ItemDef{Material::Sand, "Sand", 64, ItemDef::Type::Normal, 1.0f}},
        {Material::Water, ItemDef{Material::Water, "Water Bucket", 16, ItemDef::Type::Normal, 1.0f}},
        {Material::Player, ItemDef{Material::Player, "Debug Player Token", 1, ItemDef::Type::Tool, 3.0f}},
    };
    return kDefs;
}

} // namespace

const ItemDef* ItemDatabase::GetItemDef(std::uint16_t id)
{
    const auto& defs = GetDefs();
    const auto it = defs.find(id);
    if (it == defs.end())
    {
        return nullptr;
    }
    return &it->second;
}

} // namespace GameLayer
