#include "gui/PlotAxis.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>

void SmoothPlotAxisRange(PlotAxisState& state, double targetMin, double targetMax)
{
    if (!state.autoScale || !std::isfinite(targetMin) || !std::isfinite(targetMax) ||
        targetMax <= targetMin)
        return;

    if (!state.initialized)
    {
        state.min = targetMin;
        state.max = targetMax;
        state.initialized = true;
        return;
    }

    const float deltaTime = std::max(ImGui::GetIO().DeltaTime, 0.0F);
    const double alpha = 1.0 - std::exp(-static_cast<double>(deltaTime) / 0.5);
    state.min = targetMin < state.min ? targetMin : state.min + (targetMin - state.min) * alpha;
    state.max = targetMax > state.max ? targetMax : state.max + (targetMax - state.max) * alpha;
}

void HandlePlotAxisInteraction(PlotAxisState& xAxis, PlotAxisState& yAxis)
{
    const ImGuiIO& io = ImGui::GetIO();
    const bool dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    const bool zooming = io.MouseWheel != 0.0F;
    const bool doubleClick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const bool xHovered = ImPlot::IsAxisHovered(ImAxis_X1);
    const bool yHovered = ImPlot::IsAxisHovered(ImAxis_Y1);
    const bool plotHovered = ImPlot::IsPlotHovered();

    if (doubleClick && (xHovered || plotHovered))
    {
        xAxis.autoScale = true;
        xAxis.initialized = false;
    }
    else if ((dragging || zooming) && (xHovered || plotHovered))
        xAxis.autoScale = false;

    if (doubleClick && (yHovered || plotHovered))
    {
        yAxis.autoScale = true;
        yAxis.initialized = false;
    }
    else if ((dragging || zooming) && (yHovered || plotHovered))
        yAxis.autoScale = false;

    if (!xAxis.autoScale || !yAxis.autoScale)
    {
        const ImPlotRect limits = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
        if (!xAxis.autoScale)
        {
            xAxis.min = std::max(0.0, limits.X.Min);
            xAxis.max = std::max(xAxis.min + 1.0e-6, limits.X.Max);
            xAxis.initialized = true;
        }
        if (!yAxis.autoScale)
        {
            yAxis.min = limits.Y.Min;
            yAxis.max = limits.Y.Max;
            yAxis.initialized = true;
        }
    }
}
