#include "UI/InventoryUI.h"

#include "Game/Inventory.h"
#include "Game/ItemDatabase.h"

#include <imgui.h>

namespace UI
{

void DrawInventoryUI(const GameLayer::Inventory& inventory)
{
    constexpr int kCols = 8;
    constexpr int kRows = 4;

    ImGui::Begin("Inventory");

    const auto& slots = inventory.GetSlots();
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            const int index = row * kCols + col;
            if (index >= static_cast<int>(slots.size()))
            {
                break;
            }

            const auto& slot = slots[static_cast<std::size_t>(index)];

            ImGui::PushID(index);
            if (slot.IsEmpty())
            {
                // ImGui::Button("Empty", ImVec2(90.0f, 42.0f));
            }
            else
            {
                const GameLayer::ItemDef* def = GameLayer::ItemDatabase::GetItemDef(slot.id);
                const char* name = def ? def->name.c_str() : "Unknown";
                ImGui::Button(name, ImVec2(90.0f, 42.0f));
                ImGui::Text("x%d", slot.count);
            }

            ImGui::PopID();

            if (col < kCols - 1)
            {
                ImGui::SameLine();
            }
        }
    }

    ImGui::End();
}

} // namespace UI
