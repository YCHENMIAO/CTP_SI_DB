#ifndef GUI_SPREAD_PANEL_H
#define GUI_SPREAD_PANEL_H

#include "gui/BasePanel.h"
#include "gui/PlotAxis.h"
#include "ThostFtdcUserApiStruct.h"

#include <string>
#include <vector>

class CDataBuffer;

// 双合约月差面板。候选选择必须经“确定”后才替换当前绘图组合。
class SpreadPanel final : public BasePanel
{
public:
    SpreadPanel(std::uint64_t id, CDataBuffer& dataBuffer,
                const std::vector<std::string>& contracts,
                std::size_t statsWindow);

private:
    enum class SpreadPriceMode
    {
        MidPrice,
        SellA_BuyB,
        BuyA_SellB
    };

    struct TimedTick
    {
        double timestamp = 0.0;
        CThostFtdcDepthMarketDataField tick{};
    };

    void RenderContent() override;
    void RenderContractSelector();
    void RenderPriceModeButtons();
    void RenderStatisticsButtons(bool meanValid, double mean,
                                 bool volatilityValid, double volatility);
    void RenderStatisticLines(bool meanValid, double mean,
                              bool volatilityValid, double volatility) const;
    void BuildSpreadSeries(std::vector<double>& timestamps,
                           std::vector<double>& spreads) const;
    bool CalculateSpread(const CThostFtdcDepthMarketDataField& tickA,
                         const CThostFtdcDepthMarketDataField& tickB,
                         double& spread) const;
    void UpdateAxes(const std::vector<double>& timestamps,
                    const std::vector<double>& spreads);
    void ResetAxes();
    const char* PriceModeLabel() const;

    CDataBuffer& m_dataBuffer;
    const std::vector<std::string>& m_contracts;
    std::string m_candidateLegA;
    std::string m_candidateLegB;
    std::string m_confirmedLegA;
    std::string m_confirmedLegB;
    SpreadPriceMode m_priceMode = SpreadPriceMode::MidPrice;
    std::size_t m_statsWindow;
    bool m_showMeanLine = false;
    bool m_showVolatilityLines = false;
    PlotAxisState m_xAxis;
    PlotAxisState m_yAxis;
};

#endif
