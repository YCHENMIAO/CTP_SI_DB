#ifndef GUI_MARKET_PANEL_H
#define GUI_MARKET_PANEL_H

#include "gui/BasePanel.h"
#include "gui/PlotAxis.h"
#include "ThostFtdcUserApiStruct.h"

#include <string>
#include <vector>

class CDataBuffer;

// 单合约实时行情面板：负责合约选择、最新盘口数值、MidPrice 和 Tick 指标绘制。
// 不拥有 CDataBuffer 和合约列表，只在 GUI 主线程读取它们。
class MarketPanel final : public BasePanel
{
public:
    MarketPanel(std::uint64_t id, CDataBuffer& dataBuffer,
                const std::vector<std::string>& contracts);

private:
    enum class IndicatorType
    {
        Volume,
        OpenInterest
    };

    void RenderContent() override;
    void RenderToolbar();
    void RenderQuoteTable(const CThostFtdcDepthMarketDataField* tick);
    void RenderMidPricePlot(const std::vector<CThostFtdcDepthMarketDataField>& ticks,
                            float height);
    void RenderIndicatorButtons();
    void RenderIndicatorPlot(const std::vector<CThostFtdcDepthMarketDataField>& ticks,
                             float height);
    void UpdateAutoXAxis(const std::vector<double>& timestamps);
    void UpdatePriceYAxis(const std::vector<double>& x,
                          const std::vector<double>& prices,
                          const std::vector<double>& spreads);
    void UpdateIndicatorYAxis(const std::vector<double>& x,
                              const std::vector<double>& values);
    void ResetAxisStates();

    CDataBuffer& m_dataBuffer;                  // 非拥有引用；GetRecentTicks 内部有锁。
    const std::vector<std::string>& m_contracts; // 非拥有引用；生命周期由 PanelManager 保证。
    std::string m_selectedContract;             // 每个面板独立保存选择，互不影响。
    IndicatorType m_selectedIndicator = IndicatorType::Volume;
    PlotAxisState m_xAxis;
    PlotAxisState m_priceYAxis;
    PlotAxisState m_volumeYAxis;
    PlotAxisState m_openInterestYAxis;
};

#endif
