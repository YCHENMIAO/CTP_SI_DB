#include "DataBuffer.h"

#include <algorithm>
#include <cstring>

CDataBuffer::CDataBuffer(std::size_t maxTickCount)
	: m_maxTickCount(std::max<std::size_t>(maxTickCount, 1))
{
}

CDataBuffer::~CDataBuffer()
{
}

void CDataBuffer::PushNewTick(const CThostFtdcDepthMarketDataField& stTick)
{
	const std::string strInstrumentID = stTick.InstrumentID;

	std::lock_guard<std::mutex> lock(m_lockBuffer);
	BufferInfo& stBufferInfo = m_mapBuffer[strInstrumentID];
	std::memcpy(&stBufferInfo.stLastTick, &stTick, sizeof(CThostFtdcDepthMarketDataField));
	stBufferInfo.vctTickSeries.push_back(stTick);

	// 只保留最近 m_maxTickCount 条，避免实盘长时间运行时缓存无限增长。
	if (stBufferInfo.vctTickSeries.size() > m_maxTickCount)
	{
		stBufferInfo.vctTickSeries.erase(stBufferInfo.vctTickSeries.begin());
	}
}

bool CDataBuffer::GetTick(const std::string& strCode, CThostFtdcDepthMarketDataField& stTick) const
{
	std::lock_guard<std::mutex> lock(m_lockBuffer);
	const auto iter = m_mapBuffer.find(strCode);
	if (iter == m_mapBuffer.end())
	{
		return false;
	}

	std::memcpy(&stTick, &iter->second.stLastTick, sizeof(CThostFtdcDepthMarketDataField));
	return true;
}

bool CDataBuffer::GetSeries(const std::string& strCode, std::vector<CThostFtdcDepthMarketDataField>& vctSeries) const
{
	std::lock_guard<std::mutex> lock(m_lockBuffer);
	const auto iter = m_mapBuffer.find(strCode);
	if (iter == m_mapBuffer.end())
	{
		return false;
	}

	vctSeries = iter->second.vctTickSeries;
	return true;
}

bool CDataBuffer::GetRecentTicks(const std::string& strCode, std::size_t nTickCount, std::vector<CThostFtdcDepthMarketDataField>& vctSeries) const
{
	std::lock_guard<std::mutex> lock(m_lockBuffer);
	const auto iter = m_mapBuffer.find(strCode);
	if (iter == m_mapBuffer.end())
	{
		return false;
	}

	const std::vector<CThostFtdcDepthMarketDataField>& series = iter->second.vctTickSeries;
	const std::size_t count = std::min(nTickCount, series.size());
	// 从尾部截取最近 count 条；count 可能小于请求数量。
	vctSeries.assign(series.end() - count, series.end());
	return true;
}
