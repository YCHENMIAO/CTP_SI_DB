#include"mdspi.h"
#include"tdspi.h"
#include"DataBuffer.h"
#include"indicator.h"
#include<iostream>
#include"ThostFtdcUserApiStruct.h"
#include<map>
#include<vector>
#include<mutex>
#include<string>
#include<cstring>
#include<cstdio>
#include<sstream>

extern CThostFtdcTraderApi* pTDUserApi;
extern CThostFtdcMdApi* pMDUserApi;

extern TThostFtdcBrokerIDType	BROKER_ID;
extern TThostFtdcInvestorIDType INVESTOR_ID;
extern TThostFtdcPasswordType	PASSWORD;
extern TThostFtdcFrontIDType	FRONT_ID;	//前置编号
extern TThostFtdcSessionIDType	SESSION_ID;	//会话编号
extern TThostFtdcOrderRefType	ORDER_REF;	//报单引用

//extern const char* ppInstrumentID[];
extern int iInstrumentID;

extern int iRequestID; // 请求编号

extern vector<pair<string, string>> g_vctIFSpreads;
extern map<string, CThostFtdcInstrumentField> g_mapInstruments;
extern map<string, string> accountConfig_map;
extern std::map<std::string, std::string> contracts_map;
extern CDataBuffer* g_pDataBuffer;
extern CIndicator* g_pCIndicator;

constexpr std::size_t INDICATOR_WINDOW = 100;
constexpr const char* LEG_A = "si2609";
constexpr const char* LEG_B = "si2611";

using namespace std;

static std::vector<std::string> SplitInstrumentList(const std::string& text)
{
	std::vector<std::string> instruments;
	std::string normalized = text;
	for (char& ch : normalized)
	{
		if (ch == ',' || ch == ';' || ch == '|')
			ch = ' ';
	}

	std::istringstream stream(normalized);
	std::string instrument;
	while (stream >> instrument)
	{
		instruments.push_back(instrument);
	}
	return instruments;
}

MdSpi::MdSpi(CThostFtdcMdApi* mdapi) :mdapi(mdapi)
{

}

MdSpi::~MdSpi()
{
	// 释放所有对象的内存空间
	if (loginField)
		delete loginField;

}

void MdSpi::OnFrontConnected()
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "MD已连接" << endl;
	///用户登录请求
	ReqUserLogin();
}

void MdSpi::OnFrontDisconnected(int nReason)
{
	cerr << "MD已断开" << endl;
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "--->>> Reason = " << nReason << endl;
}

void MdSpi::OnRspError(CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	IsErrorRspInfo(pRspInfo);
}

void MdSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
	pMDUserApi = mdapi;
	memset(&req, 0, sizeof(req));
	std::snprintf(req.BrokerID, sizeof(req.BrokerID), "%s", BROKER_ID);
	std::snprintf(req.UserID, sizeof(req.UserID), "%s", INVESTOR_ID);
	std::snprintf(req.Password, sizeof(req.Password), "%s", PASSWORD);
	int iResult = pMDUserApi->ReqUserLogin(&req, ++iRequestID);
	cerr << "--->>> 发送用户登录请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}


void MdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		cerr << "MD已登录" << endl;
		///获取当前交易日
		cerr << "--->>> 获取当前交易日 = " << pMDUserApi->GetTradingDay() << endl;
		char tdFront[50];
		std::snprintf(tdFront, sizeof(tdFront), "%s", accountConfig_map["TradeFront"].c_str());
		std::vector<std::string> instruments;
		for (const auto& entry : contracts_map)
		{
			std::vector<std::string> parsed = SplitInstrumentList(entry.second);
			instruments.insert(instruments.end(), parsed.begin(), parsed.end());
		}
		SubscribeMarketData(instruments);
		if (pTDUserApi != nullptr) {
			pTDUserApi->RegisterFront(tdFront);
			pTDUserApi->Init();//初始化交易线程
		}
		else {
			cerr << "警告：交易API尚未初始化" << endl;
		}
	}
	else if (bIsLast && IsErrorRspInfo(pRspInfo))
	{
		string strLog;
		strLog = string("MD登录失败：") + pRspInfo->ErrorMsg;
		cerr << strLog << endl;
	}
}

void MdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << __FUNCTION__ << endl;
	const char* instrumentID = pSpecificInstrument ? pSpecificInstrument->InstrumentID : "<unknown>";
	if (!IsErrorRspInfo(pRspInfo))
	{
		cerr << "MD订阅成功: " << instrumentID << endl;
	}
	else
	{
		cerr << "MD订阅失败: " << instrumentID << endl;
	}
}

void MdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << __FUNCTION__ << endl;
}

void MdSpi::SubscribeMarketData(const std::vector<std::string>& instruments)
{
	if (instruments.empty())
	{
		cerr << "没有可订阅的期货合约，请检查 config/contracts.txt" << endl;
		return;
	}

	std::vector<char*> ppInstrument;
	ppInstrument.reserve(instruments.size());
	for (const std::string& instrument : instruments)
	{
		cerr << "准备订阅合约: " << instrument << endl;
		ppInstrument.push_back(const_cast<char*>(instrument.c_str()));
	}

	int nRet = mdapi->SubscribeMarketData(ppInstrument.data(), static_cast<int>(ppInstrument.size()));
	cerr << "订阅期货合约行情：" << ((nRet == 0) ? "成功" : "失败") << endl;
}

void MdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData)
{
	if (pDepthMarketData == nullptr)
	{
		return;
	}

	cout << pDepthMarketData->InstrumentID
		<< " " << pDepthMarketData->UpdateTime
		<< "." << pDepthMarketData->UpdateMillisec
		<< " Last=" << pDepthMarketData->LastPrice
		<< " Bid1=" << pDepthMarketData->BidPrice1 << "x" << pDepthMarketData->BidVolume1
		<< " Ask1=" << pDepthMarketData->AskPrice1 << "x" << pDepthMarketData->AskVolume1
		<< " Ask2=" << pDepthMarketData->AskPrice1 << "x" << pDepthMarketData->AskVolume2
		<< " Volume=" << pDepthMarketData->Volume
		<< " Turnover=" << pDepthMarketData->Turnover
		<< " OpenInterest=" << pDepthMarketData->OpenInterest
		<< " TradingDay=" << pDepthMarketData->TradingDay
		<< "PreDelta=" << pDepthMarketData->PreDelta
		<< " CurrDelta=" << pDepthMarketData->CurrDelta
		<< endl;
	if (g_pDataBuffer == nullptr)
	{
		return;
	}

	g_pDataBuffer->PushNewTick(*pDepthMarketData);

	if (g_pCIndicator == nullptr)
	{
		return;
	}

	const std::string currentInstrument = pDepthMarketData->InstrumentID;

	if (currentInstrument == LEG_A || currentInstrument == LEG_B)
	{
		double volA = 0.0;
		if (g_pCIndicator->GetSingleVol(*g_pDataBuffer, LEG_A, INDICATOR_WINDOW, volA))
		{
			cout << "Indicator " << LEG_A
				<< " SingleVol" << INDICATOR_WINDOW << "=" << volA
				<< endl;
		}

		double volB = 0.0;
		if (g_pCIndicator->GetSingleVol(*g_pDataBuffer, LEG_B, INDICATOR_WINDOW, volB))
		{
			cout << "Indicator " << LEG_B
				<< " SingleVol" << INDICATOR_WINDOW << "=" << volB
				<< endl;
		}

		double monSpread = 0.0;
		double monSpreadVol = 0.0;
		if (g_pCIndicator->UpdateMonSpread(*g_pDataBuffer, LEG_A, LEG_B, INDICATOR_WINDOW, monSpread, monSpreadVol))
		{
			cout << "Indicator " << LEG_A << "-" << LEG_B
				<< " MonSpread=" << monSpread
				<< " MonSpreadVol" << INDICATOR_WINDOW << "=" << monSpreadVol
				<< endl;
		}
	}
}

void MdSpi::OnHeartBeatWarning(int nTimeLapse)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "--->>> nTimerLapse = " << nTimeLapse << endl;
}

bool MdSpi::IsErrorRspInfo(CThostFtdcRspInfoField* pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
		cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
	return bResult;
}
