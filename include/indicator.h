#ifndef INDICATOR_H
#define INDICATOR_H

#include "DataBuffer.h"
#include "ThostFtdcUserApiStruct.h"

#include <cstdint>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

struct Values
{
	uint64_t qwDataTime;
	double dValue;
};

class CIndicator
{
public:
	explicit CIndicator(std::size_t maxSpreadCount = 1000);
	~CIndicator();

	static uint64_t GetDateTime(const CThostFtdcDepthMarketDataField& stTick);

	// 获取单个 tick 的盘口中间价：(BidPrice1 + AskPrice1) / 2。
	static bool GetMidprice(const CThostFtdcDepthMarketDataField& stTick, double& dMidPrice);

	// 获取数值序列的平均值。
	static bool Getmeanprice(const std::vector<double>& values, double& dMean);

	// 获取数值序列的样本标准差，分母为 n - 1。
	static bool Getstd(const std::vector<double>& values, double& dStd);

	// 获取单个 tick 的盘口价差：AskPrice1 - BidPrice1。
	static bool Getspread(const CThostFtdcDepthMarketDataField& stTick, double& dSpread);

	// 获取一个时点的月差，方向为 stTickA - stTickB，价格口径使用 MidPrice。
	static bool Get_Mon_spread(const CThostFtdcDepthMarketDataField& stTickA, const CThostFtdcDepthMarketDataField& stTickB, double& dMonSpread);

	// 盘口月差：A 的卖一价（Ask1）减 B 的买一价（Bid1）。
	static bool GetSellABuyBSpread(const CThostFtdcDepthMarketDataField& stTickA,
		const CThostFtdcDepthMarketDataField& stTickB, double& dSpread);

	// 盘口月差：A 的买一价（Bid1）减 B 的卖一价（Ask1）。
	static bool GetBuyASellBSpread(const CThostFtdcDepthMarketDataField& stTickA,
		const CThostFtdcDepthMarketDataField& stTickB, double& dSpread);

	// 获取单品种最近 window 个 MidPrice 的实时波动率。
	bool GetSingleVol(const CDataBuffer& dataBuffer, const std::string& instrumentID, std::size_t window, double& dVol);

	// 更新 codeA-codeB 的月差序列，并输出最新月差和最近 window 个月差的实时波动率。
	bool UpdateMonSpread(const CDataBuffer& dataBuffer, const std::string& codeA, const std::string& codeB, std::size_t window, double& dSpread, double& dVol);

private:
	using SpreadKey = std::pair<std::string, std::string>;

	std::map<SpreadKey, std::vector<double>> m_mapMonSpreadSeries;
	std::mutex m_lockIndicator;
	std::size_t m_maxSpreadCount;
};

#endif
