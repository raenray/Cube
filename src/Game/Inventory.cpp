#include "Game/Inventory.h"

#include <algorithm>

#include "Game/ItemDatabase.h"

namespace GameLayer
{

Inventory::Inventory(std::size_t size)
    : m_Slots(size)
{
}

bool Inventory::AddItem(std::uint16_t id, int count)
{
    if (id == 0 || count <= 0)
    {
        return false;
    }

    int remaining = count;
    const int maxStack = ResolveMaxStack(id);

    // 1) 先堆叠到已有同类槽位
    for (Item& slot : m_Slots)
    {
        if (remaining <= 0)
        {
            break;
        }

        if (slot.IsEmpty() || slot.id != id)
        {
            continue;
        }

        const int space = slot.maxStack - slot.count;
        if (space <= 0)
        {
            continue;
        }

        const int toAdd = std::min(space, remaining);
        slot.count += toAdd;
        remaining -= toAdd;
    }

    // 2) 再使用空槽分配新堆叠
    for (Item& slot : m_Slots)
    {
        if (remaining <= 0)
        {
            break;
        }

        if (!slot.IsEmpty())
        {
            continue;
        }

        const int toAdd = std::min(maxStack, remaining);
        slot.id = id;
        slot.maxStack = maxStack;
        slot.count = toAdd;
        remaining -= toAdd;
    }

    return remaining == 0;
}

bool Inventory::RemoveItem(std::uint16_t id, int count)
{
    if (id == 0 || count <= 0)
    {
        return false;
    }

    if (CountItem(id) < count)
    {
        return false;
    }

    int remaining = count;
    for (Item& slot : m_Slots)
    {
        if (remaining <= 0)
        {
            break;
        }

        if (slot.IsEmpty() || slot.id != id)
        {
            continue;
        }

        const int toRemove = std::min(slot.count, remaining);
        slot.count -= toRemove;
        remaining -= toRemove;

        if (slot.count <= 0)
        {
            slot = Item{};
        }
    }

    return true;
}

int Inventory::CountItem(std::uint16_t id) const
{
    if (id == 0)
    {
        return 0;
    }

    int total = 0;
    for (const Item& slot : m_Slots)
    {
        if (!slot.IsEmpty() && slot.id == id)
        {
            total += slot.count;
        }
    }
    return total;
}

int Inventory::ResolveMaxStack(std::uint16_t id)
{
    if (const ItemDef* def = ItemDatabase::GetItemDef(id))
    {
        return std::max(1, def->maxStack);
    }
    return 64;
}

} // namespace GameLayer
