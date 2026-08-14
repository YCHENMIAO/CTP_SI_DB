#ifndef GUI_CHART_PANEL_H
#define GUI_CHART_PANEL_H

#include "gui/BasePanel.h"

// 图表面板骨架。
// TODO(下一阶段)：注入 CDataBuffer/指标依赖，建立 ImPlot Context 后绘制实时 MidPrice。
class ChartPanel final : public BasePanel
{
public:
    explicit ChartPanel(std::uint64_t id);

private:
    void RenderContent() override; // TODO(下一阶段)：当前只渲染占位文字。
};

#endif
