#include "gui/panels/MarketPanel.h"

#include "DataBuffer.h"
#include "indicator.h"
#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>

namespace
{
constexpr std::size_t kMaxVisibleTicks = 1000;

// CTP 的 ActionDay 表示行情实际发生的自然日，夜盘时比 TradingDay 更适合作为横轴日期。
// 返回 Unix 本地时间戳（含毫秒小数），ImPlotScale_Time 会将其格式化为时间标签。
bool GetTickTimestamp(const CThostFtdcDepthMarketDataField& tick, double& timestamp)
{
    const char* date = tick.ActionDay[0] != '\0' ? tick.ActionDay : tick.TradingDay;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(date, "%4d%2d%2d", &year, &month, &day) != 3 ||
        std::sscanf(tick.UpdateTime, "%2d:%2d:%2d", &hour, &minute, &second) != 3)
        return false;

    std::tm localTime{};
    localTime.tm_year = year - 1900;
    localTime.tm_mon = month - 1;
    localTime.tm_mday = day;
    localTime.tm_hour = hour;
    localTime.tm_min = minute;
    localTime.tm_sec = second;
    localTime.tm_isdst = -1;
    const std::time_t seconds = std::mktime(&localTime);
    if (seconds < 0)
        return false;

    timestamp = static_cast<double>(seconds) +
                static_cast<double>(std::clamp(tick.UpdateMillisec, 0, 999)) / 1000.0;
    return timestamp >= 0.0;
}

double GetBarWidth(const std::vector<double>& x)
{
    double minimumSpacing = 1.0;
    bool foundSpacing = false;
    for (std::size_t i = 1; i < x.size(); ++i)
    {
        const double spacing = x[i] - x[i - 1];
        if (spacing > 0.0 && (!foundSpacing || spacing < minimumSpacing))
        {
            minimumSpacing = spacing;
            foundSpacing = true;
        }
    }
    return std::max(minimumSpacing * 0.8, 0.001);
}

bool RenderSelectedButton(const char* label, bool selected)
{
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    const bool pressed = ImGui::Button(label);
    if (selected)
        ImGui::PopStyleColor(2);
    return pressed;
}
}

MarketPanel::MarketPanel(std::uint64_t id, CDataBuffer& dataBuffer,
                         const std::vector<std::string>& contracts)
    // dataBuffer 和 contracts 都由上层拥有；MarketPanel 仅保存引用，不复制行情数据。
    : BasePanel(id, "行情面板 " + std::to_string(id), "market", 900.0F, 700.0F),
      m_dataBuffer(dataBuffer),
      m_contracts(contracts)
{
}

void MarketPanel::RenderContent()
{
    RenderToolbar();

    std::vector<CThostFtdcDepthMarketDataField> ticks;
    const bool hasTicks = !m_selectedContract.empty() &&
                          m_dataBuffer.GetRecentTicks(m_selectedContract, kMaxVisibleTicks, ticks) &&
                          !ticks.empty();
    std::vector<double> timestamps;
    timestamps.reserve(ticks.size());
    for (const CThostFtdcDepthMarketDataField& tick : ticks)
    {
        double timestamp = 0.0;
        if (GetTickTimestamp(tick, timestamp))
            timestamps.push_back(timestamp);
    }
    UpdateAutoXAxis(timestamps);

    RenderQuoteTable(hasTicks ? &ticks.back() : nullptr);

    if (m_selectedContract.empty())
        ImGui::TextUnformatted("请选择合约");
    else if (!hasTicks)
        ImGui::TextUnformatted("等待行情数据……");

    // 即时数值和按钮占用固定高度，其余空间按约 65:35 分给上下两个图。
    const float availableHeight = std::max(ImGui::GetContentRegionAvail().y, 260.0F);
    const float indicatorButtonsHeight = ImGui::GetFrameHeightWithSpacing();
    const float plotHeight = std::max(availableHeight - indicatorButtonsHeight, 220.0F);
    const float priceHeight = std::max(plotHeight * 0.65F, 140.0F);
    const float indicatorHeight = std::max(plotHeight - priceHeight, 80.0F);

    RenderMidPricePlot(ticks, priceHeight);
    RenderIndicatorButtons();
    RenderIndicatorPlot(ticks, indicatorHeight);

}

void MarketPanel::RenderToolbar()
{
    // 每个 MarketPanel 保存自己的选择，因此多个面板可以同时观察不同合约。
    const char* preview = m_selectedContract.empty() ? "请选择合约" : m_selectedContract.c_str();
    const std::string comboLabel = "合约##contract_" + std::to_string(Id());
    ImGui::SetNextItemWidth(240.0F);
    if (ImGui::BeginCombo(comboLabel.c_str(), preview))
    {
        for (const std::string& contract : m_contracts)
        {
            const bool selected = m_selectedContract == contract;
            if (ImGui::Selectable(contract.c_str(), selected))
            {
                m_selectedContract = contract;
                ResetAxisStates();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    RenderSelectedButton("Tick", true);
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::Button("1分");
    ImGui::SameLine();
    ImGui::Button("5分");
    ImGui::SameLine();
    ImGui::Button("15分");
    
    ImGui::EndDisabled();
}

void MarketPanel::RenderQuoteTable(const CThostFtdcDepthMarketDataField* tick)
{
    if (tick == nullptr)
        return;

    double midPrice = 0.0;
    double spread = 0.0;
    // 统一复用指标层的盘口校验：买卖价无效或卖价低于买价时显示 --。
    const bool quoteValid = CIndicator::GetMidprice(*tick, midPrice) &&
                            CIndicator::Getspread(*tick, spread);
    const std::string tableId = "##market_values_" + std::to_string(Id());
    if (ImGui::BeginTable(tableId.c_str(), 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame))
    {
        const auto renderValue = [quoteValid](const char* label, double value) {
            ImGui::TextDisabled("%s", label);
            if (quoteValid)
                ImGui::Text("%.2f", value);
            else
                ImGui::TextUnformatted("--");
        };
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        renderValue("买一", tick->BidPrice1);
        ImGui::TableSetColumnIndex(1);
        renderValue("卖一", tick->AskPrice1);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        renderValue("MidPrice", midPrice);
        ImGui::TableSetColumnIndex(1);
        renderValue("Spread", spread);
        ImGui::EndTable();
    }
}

void MarketPanel::RenderMidPricePlot(
    const std::vector<CThostFtdcDepthMarketDataField>& ticks, float height)
{
    std::vector<double> x;
    std::vector<double> midPrices;
    std::vector<double> spreads;
    x.reserve(ticks.size());
    midPrices.reserve(ticks.size());
    spreads.reserve(ticks.size());
    for (const CThostFtdcDepthMarketDataField& tick : ticks)
    {
        double timestamp = 0.0;
        double midPrice = 0.0;
        double spread = 0.0;
        if (GetTickTimestamp(tick, timestamp) &&
            CIndicator::GetMidprice(tick, midPrice) &&
            CIndicator::Getspread(tick, spread) &&
            std::isfinite(midPrice) && std::isfinite(spread))
        {
            x.push_back(timestamp);
            midPrices.push_back(midPrice);
            spreads.push_back(spread);
        }
    }

    UpdatePriceYAxis(x, midPrices, spreads);

    const std::string plotId = "MidPrice##mid_price_" + std::to_string(Id());
    if (ImPlot::BeginPlot(plotId.c_str(), ImVec2(-1.0F, height)))
    {
        ImPlot::SetupAxis(ImAxis_X1, nullptr,
                          ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks);
        ImPlot::SetupAxis(ImAxis_Y1, "MidPrice");
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0.0,
                                           std::numeric_limits<double>::max());
        ImPlot::SetupAxisLinks(ImAxis_X1, &m_xAxis.min, &m_xAxis.max);
        ImPlot::SetupAxisLinks(ImAxis_Y1, &m_priceYAxis.min, &m_priceYAxis.max);
        if (!x.empty())
            ImPlot::PlotLine("MidPrice", x.data(), midPrices.data(), static_cast<int>(x.size()));
        HandlePlotAxisInteraction(m_xAxis, m_priceYAxis);
        ImPlot::EndPlot();
    }
}

void MarketPanel::RenderIndicatorButtons()
{
    if (RenderSelectedButton("Vol", m_selectedIndicator == IndicatorType::Volume))
        m_selectedIndicator = IndicatorType::Volume;

    ImGui::SameLine();
    if (RenderSelectedButton("OI", m_selectedIndicator == IndicatorType::OpenInterest))
        m_selectedIndicator = IndicatorType::OpenInterest;

    // 尚未实现的指标仅保留同一行入口：禁用后不会改变 Vol/OI 的选择状态，
    // 也不会错误复用 OpenInterest 的数据和 ImPlot 窗口 ID。
    ImGui::BeginDisabled();
    ImGui::SameLine();
    ImGui::Button("MACD");
    ImGui::SameLine();
    ImGui::Button("MA5");
    ImGui::SameLine();
    ImGui::Button("MA10");
    ImGui::EndDisabled();
}

void MarketPanel::RenderIndicatorPlot(
    const std::vector<CThostFtdcDepthMarketDataField>& ticks, float height)
{
    std::vector<double> x;
    std::vector<double> values;
    x.reserve(ticks.size());
    values.reserve(ticks.size());
    for (std::size_t i = 0; i < ticks.size(); ++i)
    {
        double timestamp = 0.0;
        if (!GetTickTimestamp(ticks[i], timestamp))
            continue;

        double value = 0.0;
        if (m_selectedIndicator == IndicatorType::OpenInterest)
        {
            value = std::isfinite(ticks[i].OpenInterest) ? ticks[i].OpenInterest : 0.0;
        }
        else if (i > 0)
        {
            // Volume 是日内累计值。发生换日或源端回退时，以当前累计值作为新段首值。
            value = ticks[i].Volume >= ticks[i - 1].Volume
                        ? static_cast<double>(ticks[i].Volume - ticks[i - 1].Volume)
                        : static_cast<double>(std::max(ticks[i].Volume, 0));
        }
        x.push_back(timestamp);
        values.push_back(value);
    }

    // Vol 仍按相邻 Tick 的成交量增量着色；OI 使用绝对持仓量折线，
    // 不再根据相邻持仓的增减拆分为多组柱子。
    std::vector<double> redX;
    std::vector<double> redValues;
    std::vector<double> greenX;
    std::vector<double> greenValues;
    std::vector<double> flatX;
    std::vector<double> flatValues;
    if (m_selectedIndicator == IndicatorType::Volume)
    {
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            std::vector<double>* colorX = &flatX;
            std::vector<double>* colorValues = &flatValues;
            if (i > 0 && values[i] > values[i - 1])
            {
                colorX = &redX;
                colorValues = &redValues;
            }
            else if (i > 0 && values[i] < values[i - 1])
            {
                colorX = &greenX;
                colorValues = &greenValues;
            }
            colorX->push_back(x[i]);
            colorValues->push_back(values[i]);
        }
    }

    UpdateIndicatorYAxis(x, values);

    const char* indicatorName =
        m_selectedIndicator == IndicatorType::Volume ? "Vol" : "OI";
    PlotAxisState& indicatorYAxis = m_selectedIndicator == IndicatorType::Volume
                                        ? m_volumeYAxis
                                        : m_openInterestYAxis;
    const std::string plotId = std::string(indicatorName) + "##indicator_" + std::to_string(Id());
    if (ImPlot::BeginPlot(plotId.c_str(), ImVec2(-1.0F, height), ImPlotFlags_NoLegend))
    {
        ImPlot::SetupAxis(ImAxis_X1, "时间");
        ImPlot::SetupAxis(ImAxis_Y1, indicatorName);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0.0,
                                           std::numeric_limits<double>::max());
        ImPlot::SetupAxisLinks(ImAxis_X1, &m_xAxis.min, &m_xAxis.max);
        ImPlot::SetupAxisLinks(ImAxis_Y1, &indicatorYAxis.min, &indicatorYAxis.max);
        if (m_selectedIndicator == IndicatorType::OpenInterest)
        {
            if (!x.empty())
                ImPlot::PlotLine("OI", x.data(), values.data(), static_cast<int>(x.size()));
        }
        else
        {
            const double barWidth = GetBarWidth(x);
            if (!redX.empty())
            {
                ImPlot::SetNextFillStyle(ImVec4(0.90F, 0.20F, 0.20F, 1.0F));
                ImPlot::PlotBars("增加", redX.data(), redValues.data(),
                                 static_cast<int>(redX.size()), barWidth);
            }
            if (!greenX.empty())
            {
                ImPlot::SetNextFillStyle(ImVec4(0.20F, 0.75F, 0.30F, 1.0F));
                ImPlot::PlotBars("减少", greenX.data(), greenValues.data(),
                                 static_cast<int>(greenX.size()), barWidth);
            }
            if (!flatX.empty())
            {
                ImPlot::SetNextFillStyle(ImVec4(0.55F, 0.55F, 0.55F, 1.0F));
                ImPlot::PlotBars("持平", flatX.data(), flatValues.data(),
                                 static_cast<int>(flatX.size()), barWidth);
            }
        }
        HandlePlotAxisInteraction(m_xAxis, indicatorYAxis);
        ImPlot::EndPlot();
    }
}

void MarketPanel::UpdateAutoXAxis(const std::vector<double>& timestamps)
{
    if (!m_xAxis.autoScale || timestamps.empty())
        return;

    const auto [minimum, maximum] = std::minmax_element(timestamps.begin(), timestamps.end());
    if (!std::isfinite(*minimum) || !std::isfinite(*maximum))
        return;

    const double span = std::max(*maximum - *minimum, 1.0);
    const double padding = span * 0.02;
    SmoothPlotAxisRange(m_xAxis, std::max(0.0, *minimum - padding), *maximum + padding);
}

void MarketPanel::UpdatePriceYAxis(const std::vector<double>& x,
                                   const std::vector<double>& prices,
                                   const std::vector<double>& spreads)
{
    if (!m_priceYAxis.autoScale || x.size() != prices.size() || x.size() != spreads.size())
        return;

    std::vector<double> visiblePrices;
    std::vector<double> visibleSpreads;
    visiblePrices.reserve(prices.size());
    visibleSpreads.reserve(spreads.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        if (x[i] >= m_xAxis.min && x[i] <= m_xAxis.max &&
            std::isfinite(prices[i]) && std::isfinite(spreads[i]))
        {
            visiblePrices.push_back(prices[i]);
            visibleSpreads.push_back(std::max(spreads[i], 0.0));
        }
    }
    if (visiblePrices.empty())
        return;

    const auto [minimum, maximum] =
        std::minmax_element(visiblePrices.begin(), visiblePrices.end());
    const double center = (*minimum + *maximum) * 0.5;
    const std::size_t middle = visibleSpreads.size() / 2;
    std::nth_element(visibleSpreads.begin(), visibleSpreads.begin() + middle,
                     visibleSpreads.end());
    const double minimumSpan = std::max({visibleSpreads[middle] * 4.0,
                                         std::abs(center) * 0.0001,
                                         1.0e-6});
    const double span = std::max(*maximum - *minimum, minimumSpan);
    SmoothPlotAxisRange(m_priceYAxis, center - span * 0.58, center + span * 0.58);
}

void MarketPanel::UpdateIndicatorYAxis(const std::vector<double>& x,
                                       const std::vector<double>& values)
{
    PlotAxisState& state = m_selectedIndicator == IndicatorType::Volume
                               ? m_volumeYAxis
                               : m_openInterestYAxis;
    if (!state.autoScale || x.size() != values.size())
        return;

    std::vector<double> visible;
    visible.reserve(values.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        if (x[i] >= m_xAxis.min && x[i] <= m_xAxis.max && std::isfinite(values[i]))
            visible.push_back(values[i]);
    }
    if (visible.empty())
        return;

    if (m_selectedIndicator == IndicatorType::Volume)
    {
        for (double& value : visible)
            value = std::max(value, 0.0);
        const std::size_t percentileIndex =
            std::min(visible.size() - 1,
                     static_cast<std::size_t>(std::ceil(visible.size() * 0.99)) - 1);
        std::nth_element(visible.begin(), visible.begin() + percentileIndex, visible.end());
        SmoothPlotAxisRange(state, 0.0, std::max(visible[percentileIndex] * 1.15, 1.0));
        return;
    }

    const auto [minimum, maximum] = std::minmax_element(visible.begin(), visible.end());
    const double center = (*minimum + *maximum) * 0.5;
    const double minimumSpan = std::max(1.0, std::abs(center) * 0.001);
    const double span = std::max(*maximum - *minimum, minimumSpan);
    SmoothPlotAxisRange(state, center - span * 0.55, center + span * 0.55);
}

void MarketPanel::ResetAxisStates()
{
    m_xAxis = {};
    m_priceYAxis = {};
    m_volumeYAxis = {};
    m_openInterestYAxis = {};
}
