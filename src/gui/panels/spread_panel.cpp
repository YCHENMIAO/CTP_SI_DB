#include "gui/panels/SpreadPanel.h"

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
constexpr double kPairToleranceSeconds = 1.0;

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

bool HasValidBook(const CThostFtdcDepthMarketDataField& tick)
{
    double midPrice = 0.0;
    return CIndicator::GetMidprice(tick, midPrice) && std::isfinite(midPrice);
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

SpreadPanel::SpreadPanel(std::uint64_t id, CDataBuffer& dataBuffer,
                         const std::vector<std::string>& contracts,
                         std::size_t statsWindow)
    : BasePanel(id, "价差面板 " + std::to_string(id), "spread", 900.0F, 650.0F),
      m_dataBuffer(dataBuffer),
      m_contracts(contracts),
      m_statsWindow(std::max<std::size_t>(statsWindow, 2))
{
}

void SpreadPanel::RenderContent()
{
    RenderContractSelector();
    RenderPriceModeButtons();

    std::vector<double> timestamps;
    std::vector<double> spreads;
    BuildSpreadSeries(timestamps, spreads);

    double mean = 0.0;
    double volatility = 0.0;
    bool meanValid = false;
    bool volatilityValid = false;
    if (!spreads.empty())
    {
        const std::size_t count = std::min(m_statsWindow, spreads.size());
        const std::vector<double> recentSpreads(spreads.end() - count, spreads.end());
        meanValid = CIndicator::Getmeanprice(recentSpreads, mean);
        volatilityValid = CIndicator::Getstd(recentSpreads, volatility);
    }
    RenderStatisticsButtons(meanValid, mean, volatilityValid, volatility);

    if (m_confirmedLegA.empty() || m_confirmedLegB.empty())
        ImGui::TextUnformatted("请选择两个不同合约并点击确定");
    else
    {
        ImGui::Text("当前组合：%s - %s", m_confirmedLegA.c_str(), m_confirmedLegB.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("口径：%s", PriceModeLabel());
        if (spreads.empty())
            ImGui::TextUnformatted("等待两腿同步行情……");
        else
            ImGui::Text("最新月差：%.4f", spreads.back());
    }

    UpdateAxes(timestamps, spreads);
    const std::string plotId = "月差##spread_plot_" + std::to_string(Id());
    const float plotHeight = std::max(ImGui::GetContentRegionAvail().y, 220.0F);
    if (ImPlot::BeginPlot(plotId.c_str(), ImVec2(-1.0F, plotHeight)))
    {
        ImPlot::SetupAxis(ImAxis_X1, "时间");
        ImPlot::SetupAxis(ImAxis_Y1, "月差");
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0.0,
                                           std::numeric_limits<double>::max());
        ImPlot::SetupAxisLinks(ImAxis_X1, &m_xAxis.min, &m_xAxis.max);
        ImPlot::SetupAxisLinks(ImAxis_Y1, &m_yAxis.min, &m_yAxis.max);
        if (!timestamps.empty())
            ImPlot::PlotLine("月差", timestamps.data(), spreads.data(),
                             static_cast<int>(timestamps.size()));
        RenderStatisticLines(meanValid, mean, volatilityValid, volatility);
        HandlePlotAxisInteraction(m_xAxis, m_yAxis);
        ImPlot::EndPlot();
    }
}

void SpreadPanel::RenderContractSelector()
{
    const auto renderCombo = [this](const char* label, std::string& selected) {
        ImGui::SetNextItemWidth(220.0F);
        const char* preview = selected.empty() ? "请选择合约" : selected.c_str();
        const std::string id = std::string(label) + "##spread_" + std::to_string(Id());
        if (ImGui::BeginCombo(id.c_str(), preview))
        {
            for (const std::string& contract : m_contracts)
            {
                const bool isSelected = selected == contract;
                if (ImGui::Selectable(contract.c_str(), isSelected))
                    selected = contract;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    };

    renderCombo("Leg A", m_candidateLegA);
    ImGui::SameLine();
    renderCombo("Leg B", m_candidateLegB);
    ImGui::SameLine();

    const bool canConfirm = !m_candidateLegA.empty() && !m_candidateLegB.empty() &&
                            m_candidateLegA != m_candidateLegB;
    ImGui::BeginDisabled(!canConfirm);
    if (ImGui::Button("确定"))
    {
        m_confirmedLegA = m_candidateLegA;
        m_confirmedLegB = m_candidateLegB;
        ResetAxes();
    }
    ImGui::EndDisabled();

    if (!m_candidateLegA.empty() && m_candidateLegA == m_candidateLegB)
        ImGui::TextDisabled("Leg A 与 Leg B 不能相同");
}

void SpreadPanel::RenderPriceModeButtons()
{
    const auto selectMode = [this](const char* label, SpreadPriceMode mode) {
        if (RenderSelectedButton(label, m_priceMode == mode) && m_priceMode != mode)
        {
            m_priceMode = mode;
            m_yAxis = {};
        }
    };

    selectMode("MidA - MidB", SpreadPriceMode::MidPrice);
    ImGui::SameLine();
    selectMode("卖1 - 买2", SpreadPriceMode::SellA_BuyB);
    ImGui::SameLine();
    selectMode("买1 - 卖2", SpreadPriceMode::BuyA_SellB);
}

void SpreadPanel::RenderStatisticsButtons(bool meanValid, double mean,
                                          bool volatilityValid, double volatility)
{
    char meanLabel[64]{};
    char volatilityLabel[64]{};
    if (meanValid)
        std::snprintf(meanLabel, sizeof(meanLabel), "均值: %.4f", mean);
    else
        std::snprintf(meanLabel, sizeof(meanLabel), "均值: --");
    if (volatilityValid)
        std::snprintf(volatilityLabel, sizeof(volatilityLabel), "波动率: %.4f", volatility);
    else
        std::snprintf(volatilityLabel, sizeof(volatilityLabel), "波动率: --");

    ImGui::BeginDisabled(!meanValid);
    if (RenderSelectedButton(meanLabel, m_showMeanLine))
        m_showMeanLine = !m_showMeanLine;
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!volatilityValid);
    if (RenderSelectedButton(volatilityLabel, m_showVolatilityLines))
        m_showVolatilityLines = !m_showVolatilityLines;
    ImGui::EndDisabled();
}

void SpreadPanel::RenderStatisticLines(bool meanValid, double mean,
                                       bool volatilityValid, double volatility) const
{
    if (m_showMeanLine && meanValid)
    {
        const double value = mean;
        ImPlot::SetNextLineStyle(ImVec4(1.0F, 0.75F, 0.15F, 1.0F), 1.5F);
        ImPlot::PlotInfLines("##spread_mean", &value, 1, ImPlotInfLinesFlags_Horizontal);
    }

    if (!m_showVolatilityLines || !meanValid || !volatilityValid)
        return;

    // ImPlot 0.17 没有虚线样式；用成对线段生成两条水平虚线，并用 ## 隐藏图例项。
    constexpr int kDashCount = 24;
    double xValues[kDashCount * 2]{};
    double upperValues[kDashCount * 2]{};
    double lowerValues[kDashCount * 2]{};
    const double span = std::max(m_xAxis.max - m_xAxis.min, 1.0e-6);
    for (int index = 0; index < kDashCount; ++index)
    {
        const double start = m_xAxis.min + span * static_cast<double>(index) / kDashCount;
        const double end = start + span / (kDashCount * 2.0);
        xValues[index * 2] = start;
        xValues[index * 2 + 1] = end;
        upperValues[index * 2] = upperValues[index * 2 + 1] = mean + volatility;
        lowerValues[index * 2] = lowerValues[index * 2 + 1] = mean - volatility;
    }

    ImPlot::SetNextLineStyle(ImVec4(1.0F, 0.75F, 0.15F, 0.55F), 1.0F);
    ImPlot::PlotLine("##spread_upper_sigma", xValues, upperValues, kDashCount * 2,
                     ImPlotLineFlags_Segments);
    ImPlot::SetNextLineStyle(ImVec4(1.0F, 0.75F, 0.15F, 0.55F), 1.0F);
    ImPlot::PlotLine("##spread_lower_sigma", xValues, lowerValues, kDashCount * 2,
                     ImPlotLineFlags_Segments);
}

void SpreadPanel::BuildSpreadSeries(std::vector<double>& timestamps,
                                    std::vector<double>& spreads) const
{
    if (m_confirmedLegA.empty() || m_confirmedLegB.empty())
        return;

    std::vector<CThostFtdcDepthMarketDataField> rawA;
    std::vector<CThostFtdcDepthMarketDataField> rawB;
    if (!m_dataBuffer.GetRecentTicks(m_confirmedLegA, kMaxVisibleTicks, rawA) ||
        !m_dataBuffer.GetRecentTicks(m_confirmedLegB, kMaxVisibleTicks, rawB))
        return;

    const auto prepare = [](const std::vector<CThostFtdcDepthMarketDataField>& raw) {
        std::vector<TimedTick> result;
        result.reserve(raw.size());
        for (const CThostFtdcDepthMarketDataField& tick : raw)
        {
            double timestamp = 0.0;
            if (GetTickTimestamp(tick, timestamp) && HasValidBook(tick))
                result.push_back({timestamp, tick});
        }
        std::stable_sort(result.begin(), result.end(),
                         [](const TimedTick& left, const TimedTick& right) {
                             return left.timestamp < right.timestamp;
                         });
        return result;
    };

    const std::vector<TimedTick> ticksA = prepare(rawA);
    const std::vector<TimedTick> ticksB = prepare(rawB);
    std::size_t indexA = 0;
    std::size_t indexB = 0;
    while (indexA < ticksA.size() && indexB < ticksB.size())
    {
        const double timeA = ticksA[indexA].timestamp;
        // 对当前 A 比较尚未消费的两个相邻 B；相同距离保留较早的 B。
        if (indexB + 1 < ticksB.size() &&
            std::abs(ticksB[indexB + 1].timestamp - timeA) <
                std::abs(ticksB[indexB].timestamp - timeA))
        {
            ++indexB;
            continue;
        }

        const double difference = timeA - ticksB[indexB].timestamp;
        if (std::abs(difference) <= kPairToleranceSeconds)
        {
            double spread = 0.0;
            if (CalculateSpread(ticksA[indexA].tick, ticksB[indexB].tick, spread))
            {
                timestamps.push_back(std::max(timeA, ticksB[indexB].timestamp));
                spreads.push_back(spread);
            }
            ++indexA;
            ++indexB;
        }
        else if (difference < 0.0)
            ++indexA;
        else
            ++indexB;
    }
}

bool SpreadPanel::CalculateSpread(const CThostFtdcDepthMarketDataField& tickA,
                                  const CThostFtdcDepthMarketDataField& tickB,
                                  double& spread) const
{
    switch (m_priceMode)
    {
    case SpreadPriceMode::MidPrice:
        return CIndicator::Get_Mon_spread(tickA, tickB, spread);
    case SpreadPriceMode::SellA_BuyB:
        return CIndicator::GetSellABuyBSpread(tickA, tickB, spread);
    case SpreadPriceMode::BuyA_SellB:
        return CIndicator::GetBuyASellBSpread(tickA, tickB, spread);
    }
    return false;
}

void SpreadPanel::UpdateAxes(const std::vector<double>& timestamps,
                             const std::vector<double>& spreads)
{
    if (m_xAxis.autoScale && !timestamps.empty())
    {
        const auto [minimum, maximum] =
            std::minmax_element(timestamps.begin(), timestamps.end());
        const double span = std::max(*maximum - *minimum, 1.0);
        SmoothPlotAxisRange(m_xAxis, std::max(0.0, *minimum - span * 0.02),
                            *maximum + span * 0.02);
    }

    if (!m_yAxis.autoScale || timestamps.size() != spreads.size())
        return;
    std::vector<double> visible;
    visible.reserve(spreads.size());
    for (std::size_t i = 0; i < spreads.size(); ++i)
    {
        if (timestamps[i] >= m_xAxis.min && timestamps[i] <= m_xAxis.max &&
            std::isfinite(spreads[i]))
            visible.push_back(spreads[i]);
    }
    if (visible.empty())
        return;

    const auto [minimum, maximum] = std::minmax_element(visible.begin(), visible.end());
    const double center = (*minimum + *maximum) * 0.5;
    const double minimumSpan = std::max(std::abs(center) * 0.0001, 1.0e-6);
    const double span = std::max(*maximum - *minimum, minimumSpan);
    SmoothPlotAxisRange(m_yAxis, center - span * 0.58, center + span * 0.58);
}

void SpreadPanel::ResetAxes()
{
    m_xAxis = {};
    m_yAxis = {};
}

const char* SpreadPanel::PriceModeLabel() const
{
    switch (m_priceMode)
    {
    case SpreadPriceMode::MidPrice:
        return "MidA - MidB";
    case SpreadPriceMode::SellA_BuyB:
        return "卖1 - 买2";
    case SpreadPriceMode::BuyA_SellB:
        return "买1 - 卖2";
    }
    return "";
}
