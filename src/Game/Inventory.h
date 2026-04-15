#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace GameLayer
{

struct Item
{
    std::uint16_t id = 0;
    int count = 0;
    int maxStack = 64;

    bool IsEmpty() const { return count <= 0; }
};

class Inventory
{
public:
    static constexpr std::size_t kDefaultSize = 32;

    explicit Inventory(std::size_t size = kDefaultSize);

    bool AddItem(std::uint16_t id, int count);
    bool RemoveItem(std::uint16_t id, int count);
    int CountItem(std::uint16_t id) const;

    const std::vector<Item>& GetSlots() const { return m_Slots; }

private:
    static int ResolveMaxStack(std::uint16_t id);

private:
    std::vector<Item> m_Slots;
};

} // namespace GameLayer
