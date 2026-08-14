#ifndef GUI_PLOT_AXIS_H
#define GUI_PLOT_AXIS_H

// 可由多个 Panel 复用的 ImPlot 坐标状态。具体数据范围仍由各 Panel 计算。
struct PlotAxisState
{
    double min = 0.0;
    double max = 1.0;
    bool autoScale = true;
    bool initialized = false;
};

// 自动范围扩大立即生效，收缩按时间平滑，减少实时 Tick 引起的画面抖动。
void SmoothPlotAxisRange(PlotAxisState& state, double targetMin, double targetMax);

// 捕获 ImPlot 的平移/缩放结果；手动操作暂停自动比例，双击恢复。
// 必须在 BeginPlot()/EndPlot() 之间调用。
void HandlePlotAxisInteraction(PlotAxisState& xAxis, PlotAxisState& yAxis);

#endif
