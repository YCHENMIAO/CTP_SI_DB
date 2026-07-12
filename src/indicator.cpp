#include "indicator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

CIndicator::CIndicator(std::size_t maxSpreadCount)
	: m_maxSpreadCount(std::max<std::size_t>(maxSpreadCount, 2))
{
}

CIndicator::~CIndicator()
{
}

uint64_t CIndicator::GetDateTime(const CThostFtdcDepthMarketDataField& stTick)
{
	std::string tradingDay = stTick.TradingDay;
	std::string updateTime = stTick.UpdateTime;

	if (tradingDay.empty() || updateTime.size() < 8)
	{
		return 0;
	}

	std::string compactTime = updateTime;
	compactTime.erase(2, 1);
	compactTime.erase(4, 1);

	const std::string datetime = tradingDay + compactTime + std::to_string(stTick.UpdateMillisec);
	return static_cast<uint64_t>(std::strtoull(datetime.c_str(), nullptr, 10));
}

bool CIndicator::GetMidprice(const CThostFtdcDepthMarketDataField& stTick, double& dMidPrice)
{
	if (stTick.BidPrice1 <= 0 || stTick.AskPrice1 <= 0 || stTick.AskPrice1 < stTick.BidPrice1)
	{
		return false;
	}

	dMidPrice = (stTick.BidPrice1 + stTick.AskPrice1) / 2.0;
	return true;
}

bool CIndicator::Getmeanprice(const std::vector<double>& values, double& dMean)
{
	if (values.empty())
	{
		return false;
	}

	double sum = 0.0;
	for (double value : values)
	{
		sum += value;
	}
	dMean = sum / static_cast<double>(values.size());
	return true;
}

bool CIndicator::Getstd(const std::vector<double>& values, double& dStd)
{
	if (values.size() < 2)
	{
		return false;
	}

	double mean = 0.0;
	if (!Getmeanprice(values, mean))
	{
		return false;
	}

	double squareSum = 0.0;
	for (double value : values)
	{
		const double diff = value - mean;
		squareSum += diff * diff;
	}

	dStd = std::sqrt(squareSum / static_cast<double>(values.size() - 1));
	return true;
}

bool CIndicator::Getspread(const CThostFtdcDepthMarketDataField& stTick, double& dSpread)
{
	if (stTick.BidPrice1 <= 0 || stTick.AskPrice1 <= 0 || stTick.AskPrice1 < stTick.BidPrice1)
	{
		return false;
	}

	dSpread = stTick.AskPrice1 - stTick.BidPrice1;
	return true;
}

bool CIndicator::Get_Mon_spread(const CThostFtdcDepthMarketDataField& stTickA, const CThostFtdcDepthMarketDataField& stTickB, double& dMonSpread)
{
	double midA = 0.0;
	double midB = 0.0;
	if (!GetMidprice(stTickA, midA) || !GetMidprice(stTickB, midB))
	{
		return false;
	}

	dMonSpread = midA - midB;
	return true;
}

bool CIndicator::GetSingleVol(const CDataBuffer& dataBuffer, const std::string& instrumentID, std::size_t window, double& dVol)
{
	std::vector<CThostFtdcDepthMarketDataField> ticks;
	if (!dataBuffer.GetRecentTicks(instrumentID, window, ticks))
	{
		return false;
	}

	std::vector<double> midPrices;
	midPrices.reserve(ticks.size());
	for (const CThostFtdcDepthMarketDataField& tick : ticks)
	{
		double midPrice = 0.0;
		if (GetMidprice(tick, midPrice))
		{
			midPrices.push_back(midPrice);
		}
	}

	return Getstd(midPrices, dVol);
}

bool CIndicator::UpdateMonSpread(const CDataBuffer& dataBuffer, const std::string& codeA, const std::string& codeB, std::size_t window, double& dSpread, double& dVol)
{
	CThostFtdcDepthMarketDataField tickA;
	CThostFtdcDepthMarketDataField tickB;
	if (!dataBuffer.GetTick(codeA, tickA) || !dataBuffer.GetTick(codeB, tickB))
	{
		return false;
	}
	if (!Get_Mon_spread(tickA, tickB, dSpread))
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(m_lockIndicator);
	std::vector<double>& spreadSeries = m_mapMonSpreadSeries[SpreadKey(codeA, codeB)];
	spreadSeries.push_back(dSpread);
	if (spreadSeries.size() > m_maxSpreadCount)
	{
		spreadSeries.erase(spreadSeries.begin());
	}

	const std::size_t count = std::min(window, spreadSeries.size());
	std::vector<double> recentSpreads(spreadSeries.end() - count, spreadSeries.end());
	return Getstd(recentSpreads, dVol);
}
