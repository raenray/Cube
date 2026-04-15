#pragma once

#include <cstdint>
#include <string>

namespace GameLayer
{

struct ItemDef
{
    enum class Type
    {
        Normal,
        Tool
    };

    std::uint16_t id = 0;
    std::string name;
    int maxStack = 64;
    Type type = Type::Normal;
    float miningPower = 1.0f;
};

class ItemDatabase
{
public:
    static const ItemDef* GetItemDef(std::uint16_t id);
};

} // namespace GameLayer
