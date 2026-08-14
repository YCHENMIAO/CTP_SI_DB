#include "gui/BasePanel.h"

#include "imgui.h"

#include <utility>

BasePanel::BasePanel(std::uint64_t id,
                     std::string title,
                     std::string typeId,
                     float defaultWidth,
                     float defaultHeight)
    : m_id(id),
      m_title(std::move(title)),
      // ### 之前是可见标题，之后是稳定的 ImGui 内部 ID。
      m_windowName(m_title + "###" + std::move(typeId) + "_" + std::to_string(m_id)),
      m_defaultWidth(defaultWidth),
      m_defaultHeight(defaultHeight)
{
}

std::uint64_t BasePanel::Id() const { return m_id; }
const std::string& BasePanel::Title() const { return m_title; }
bool BasePanel::IsOpen() const { return m_open; }

void BasePanel::Render()
{
    // 只在创建后的第一帧指定位置和尺寸。之后用户可自由移动、缩放，
    // 或将窗口拖出主窗口形成独立的 ImGui Platform Viewport。
    if (m_firstDisplay)
    {
        const ImVec2 workPos = ImGui::GetMainViewport()->WorkPos;
        const float offset = 35.0F * static_cast<float>((m_id - 1) % 10);
        ImGui::SetNextWindowPos(ImVec2(workPos.x + 80.0F + offset, workPos.y + 80.0F + offset),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_defaultWidth, m_defaultHeight), ImGuiCond_Always);
        m_firstDisplay = false;
    }

    if (ImGui::Begin(m_windowName.c_str(), &m_open))
        RenderContent();

    // 即使 Begin() 因窗口折叠返回 false，也必须调用与之配对的 End()。
    ImGui::End();
}
