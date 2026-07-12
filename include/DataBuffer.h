#ifndef DATA_BUFFER_H
#define DATA_BUFFER_H

#include "ThostFtdcUserApiStruct.h"

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct BufferInfo
{
	CThostFtdcDepthMarketDataField stLastTick;
	std::vector<CThostFtdcDepthMarketDataField> vctTickSeries;
};

class CDataBuffer
{
public:
	// 创建行情缓存；maxTickCount 表示每个合约最多保留的最近 tick 数，默认 1000。
	explicit CDataBuffer(std::size_t maxTickCount = 1000);
	~CDataBuffer();

	// 写入一条新 tick，并更新该合约的最新 tick。
	// 如果该合约缓存数量超过 maxTickCount，会删除最老的一条 tick。
	void PushNewTick(const CThostFtdcDepthMarketDataField& stTick);

	// 获取指定合约的最新 tick。
	// 返回 true 表示找到该合约缓存；返回 false 表示该合约尚无 tick。
	bool GetTick(const std::string& strCode, CThostFtdcDepthMarketDataField& stTick) const;

	// 获取指定合约当前缓存的全部 tick 序列。
	// 序列长度最多为构造函数指定的 maxTickCount。
	bool GetSeries(const std::string& strCode, std::vector<CThostFtdcDepthMarketDataField>& vctSeries) const;

	// 获取指定合约最近 nTickCount 条 tick。
	// 如果实际缓存不足 nTickCount 条，则返回现有全部 tick。
	bool GetRecentTicks(const std::string& strCode, std::size_t nTickCount, std::vector<CThostFtdcDepthMarketDataField>& vctSeries) const;

private:
	std::map<std::string, BufferInfo> m_mapBuffer;
	mutable std::mutex m_lockBuffer;
	std::size_t m_maxTickCount;
};

#endif
