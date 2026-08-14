#include "gui/panels/ChartPanel.h"

#include "imgui.h"

ChartPanel::ChartPanel(std::uint64_t id)
    : BasePanel(id, "图表面板 " + std::to_string(id), "chart")
{
}

void ChartPanel::RenderContent()
{
    // TODO(下一阶段)：替换此占位文字。需要完成：
    // 1. 构造函数注入 CDataBuffer 和合约列表；2. 增加合约选择；
    // 3. GuiApp 初始化/销毁 ImPlot Context；4. 使用 ImPlot 绘制实时 MidPrice。
    ImGui::TextUnformatted("图表面板待实现");
}
