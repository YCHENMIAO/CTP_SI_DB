#ifndef GUI_BASE_PANEL_H
#define GUI_BASE_PANEL_H

#include "gui/IPanel.h"

#include <string>

// ImGui 面板的通用窗口外壳。
// 统一管理窗口身份、首次定位、关闭状态以及 Begin/End 配对；
// 派生类只实现 RenderContent()，不得绕过这里自行管理窗口生命周期。
class BasePanel : public IPanel
{
public:
    BasePanel(std::uint64_t id,
              std::string title,
              std::string typeId,
              float defaultWidth = 520.0F,
              float defaultHeight = 360.0F);

    std::uint64_t Id() const final;
    const std::string& Title() const final;
    bool IsOpen() const final;
    void Render() final;

protected:
    // 仅在 ImGui::Begin() 返回 true 时调用，负责绘制窗口客户区内容。
    virtual void RenderContent() = 0;

private:
    std::uint64_t m_id;
    std::string m_title;
    std::string m_windowName;
    float m_defaultWidth;
    float m_defaultHeight;
    bool m_open = true;
    bool m_firstDisplay = true;
};

#endif
