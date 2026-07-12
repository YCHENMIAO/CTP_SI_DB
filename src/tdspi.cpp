#include "tdspi.h"
#include <iostream>
#include "ThostFtdcTraderApi.h"
#include "ThostFtdcUserApiStruct.h"
#include "mdspi.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <vector>
#include <map>

using namespace std;

//指针使用
extern std::map<std::string, std::string> accountConfig_map;//保存账户信息的map
extern CThostFtdcTraderApi* pTDUserApi;
extern CThostFtdcMdApi* pMDUserApi;


// 配置参数
extern char AUTHCODE[];
extern char APPID[];
extern char BROKER_ID[];		// 经纪公司代码
extern char INVESTOR_ID[];		// 投资者代码
extern char PASSWORD[];			// 用户密码


// 会话参数
TThostFtdcFrontIDType	FRONT_ID;	//前置编号
TThostFtdcSessionIDType	SESSION_ID;	//会话编号
TThostFtdcOrderRefType	ORDER_REF;	//报单引用
extern int iRequestID;

//当天订阅的合约集合，保存所有保单信息和状态
extern map<string, CThostFtdcInstrumentField> g_mapInstruments;
extern vector<string> g_vctIFCodes;
extern vector<pair<string, string>> g_vctIFSpreads;

void TdSpi::OnFrontConnected()
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "TD已连接" << endl;
	///用户登录请求
	ReqAuthenticate();
}

void TdSpi::ReqAuthenticate()
{
	CThostFtdcReqAuthenticateField req;
	memset(&req, 0, sizeof(req));
	std::snprintf(req.BrokerID, sizeof(req.BrokerID), "%s", BROKER_ID);
	std::snprintf(req.UserID, sizeof(req.UserID), "%s", INVESTOR_ID);
	std::snprintf(req.UserProductInfo, sizeof(req.UserProductInfo), "%s", accountConfig_map["product"].c_str());
	std::snprintf(req.AuthCode, sizeof(req.AuthCode), "%s", AUTHCODE);
	std::snprintf(req.AppID, sizeof(req.AppID), "%s", APPID);
	int iResult = pTDUserApi->ReqAuthenticate(&req, ++iRequestID);
	cerr << "--->>> 发送客户端认证请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void TdSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField* pRspAuthenticateField, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///用户登录请求
		cerr << "TD认证成功" << endl;
		ReqUserLogin();
	}
	else if (bIsLast && IsErrorRspInfo(pRspInfo))
	{
		string strLog;
		strLog = string("TD认证失败：") + pRspInfo->ErrorMsg;
		cerr << strLog << endl;
	}
}

void TdSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	std::snprintf(req.BrokerID, sizeof(req.BrokerID), "%s", BROKER_ID);
	std::snprintf(req.UserID, sizeof(req.UserID), "%s", INVESTOR_ID);
	std::snprintf(req.Password, sizeof(req.Password), "%s", PASSWORD);
	int iResult = pTDUserApi->ReqUserLogin(&req, ++iRequestID);
	cerr << "--->>> 发送用户登录请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void TdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		cerr << "TD登录成功" << endl;
		// 保存会话参数
		FRONT_ID = pRspUserLogin->FrontID;
		SESSION_ID = pRspUserLogin->SessionID;
		int iNextOrderRef = atoi(pRspUserLogin->MaxOrderRef);
		iNextOrderRef++;
		std::snprintf(ORDER_REF, sizeof(ORDER_REF), "%d", iNextOrderRef);
		///获取当前交易日
		cerr << "--->>> 获取当前交易日 = " << pTDUserApi->GetTradingDay() << endl;
		cout << "前置编号：" << pRspUserLogin->FrontID << endl
			<< "会话编号：" << pRspUserLogin->SessionID << endl
			<< "报单引用：" << pRspUserLogin->MaxOrderRef << endl
			<< "上期所时间：" << pRspUserLogin->SHFETime << endl
			<< "大商所时间：" << pRspUserLogin->DCETime << endl
			<< "郑商所时间：" << pRspUserLogin->CZCETime << endl
			<< "中金所时间：" << pRspUserLogin->FFEXTime << endl
			<< "能源所时间：" << pRspUserLogin->INETime << endl
			<< "交易柜台版本：" << pRspUserLogin->SysVersion << endl;
		///投资者结算结果确认
		ReqSettlementInfoConfirm();
		
	}
	else if (bIsLast && IsErrorRspInfo(pRspInfo))
	{
		string strLog;
		strLog = string("TD登录失败：") + pRspInfo->ErrorMsg;
		cerr << strLog << endl;
	}
}

void TdSpi::ReqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField req;
	memset(&req, 0, sizeof(req));
	std::snprintf(req.BrokerID, sizeof(req.BrokerID), "%s", BROKER_ID);
	std::snprintf(req.InvestorID, sizeof(req.InvestorID), "%s", INVESTOR_ID);
	int iResult = pTDUserApi->ReqSettlementInfoConfirm(&req, ++iRequestID);
	cerr << "--->>> 投资者结算结果确认: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void TdSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField* pSettlementInfoConfirm, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		ReqQryInstrument();
	}
}

void TdSpi::ReqQryInstrument()
{
	g_mapInstruments.clear();
	CThostFtdcQryInstrumentField req;
	memset(&req, 0, sizeof(req));
	//strcpy_s(req.InstrumentID, INSTRUMENT_ID);
	int iResult = pTDUserApi->ReqQryInstrument(&req, ++iRequestID);
	cerr << "--->>> 请求查询合约: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void TdSpi::OnRspQryInstrument(CThostFtdcInstrumentField* pInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo) && pInstrument)
	{
		//printf("%s.%s %s\n", pInstrument->ExchangeID, pInstrument->InstrumentID, pInstrument->InstrumentName);
		//cerr << "所有合约已经查询完毕" << endl;
		g_mapInstruments[pInstrument->InstrumentID] = *pInstrument;
	
	}
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		g_vctIFCodes.clear();
		map<string, CThostFtdcInstrumentField>::iterator iter;
		for (iter = g_mapInstruments.begin(); iter != g_mapInstruments.end(); iter++)
		{
			string strProductID = iter->second.ProductID;
			transform(strProductID.begin(), strProductID.end(), strProductID.begin(), [](unsigned char ch) {
				return static_cast<char>(std::toupper(ch));
			});	//转大写
			if (strProductID == "SI")
			{
				g_vctIFCodes.push_back(iter->second.InstrumentID);
			}
		}
		g_vctIFSpreads.clear();
		for (int i = 0; i < g_vctIFCodes.size() - 1; i++)
		{
			for (int j = i + 1; j < g_vctIFCodes.size(); j++)
			{
				g_vctIFSpreads.push_back(make_pair(g_vctIFCodes[j], g_vctIFCodes[i]));
			}
		}
		for (int i = 0; i < g_vctIFSpreads.size(); i++)
		{
			printf("%d %s-%s\n", i + 1, g_vctIFSpreads[i].first.c_str(), g_vctIFSpreads[i].second.c_str());
		}
	}
}

void TdSpi::ReqQryTradingAccount()
{
	CThostFtdcQryTradingAccountField req;
	memset(&req, 0, sizeof(req));
	std::snprintf(req.BrokerID, sizeof(req.BrokerID), "%s", BROKER_ID);
	std::snprintf(req.InvestorID, sizeof(req.InvestorID), "%s", INVESTOR_ID);
	int iResult = pTDUserApi->ReqQryTradingAccount(&req, ++iRequestID);
	cerr << "--->>> 请求查询资金账户: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void TdSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pTradingAccount, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	if (!IsErrorRspInfo(pRspInfo) && pTradingAccount)
		printf("静态权益=%.2f, 平仓盈亏=%.2f, 持仓盈亏=%.2f, 手续费=%.2f\n", pTradingAccount->CurrMargin + pTradingAccount->Available, pTradingAccount->CloseProfit, pTradingAccount->PositionProfit, pTradingAccount->Commission);
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		ReqQryInvestorPosition();
	}
}

void TdSpi::ReqQryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField req;
	memset(&req, 0, sizeof(req));
	std::snprintf(req.BrokerID, sizeof(req.BrokerID), "%s", BROKER_ID);
	std::snprintf(req.InvestorID, sizeof(req.InvestorID), "%s", INVESTOR_ID);
	//strcpy_s(req.InstrumentID, INSTRUMENT_ID);
	int iResult = pTDUserApi->ReqQryInvestorPosition(&req, ++iRequestID);
	cerr << "--->>> 请求查询投资者持仓: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void TdSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pInvestorPosition, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	if (!IsErrorRspInfo(pRspInfo) && pInvestorPosition)
		printf("%s.%s %d\n", pInvestorPosition->ExchangeID, pInvestorPosition->InstrumentID, pInvestorPosition->Position);
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		//ReqOrderInsert();
	}
}

void TdSpi::OnRtnOrder(CThostFtdcOrderField* pOrder)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	char szLog[1024];
	memset(szLog, 0, 1024);
	std::snprintf(szLog, sizeof(szLog), "订单状态变化：订单号%s，状态%c %s", pOrder->OrderSysID, pOrder->OrderStatus, pOrder->StatusMsg);
	cerr << szLog << endl;
	
}

void TdSpi::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	char szLog[1024];
	memset(szLog, 0, 1024);
	std::snprintf(szLog, sizeof(szLog), "成交：订单号%s，成交号%s，合约%s, 成交量%d, 成交价%.2f", pTrade->OrderSysID, pTrade->TradeID, pTrade->InstrumentID, pTrade->Volume, pTrade->Price);
	cerr << szLog << endl;
	
}

bool TdSpi::IsErrorRspInfo(CThostFtdcRspInfoField* pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
		cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
	return bResult;
}
//什么样的订单可撤，除了部分成交不在队列中、已成交的、已经撤单了的，都可以撤单
//其实可以无脑撤单，不等任何状态，顶多就是撤单错误，控制撤单总量就行，没啥问题
bool TdSpi::IsMyOrder(CThostFtdcOrderField* pOrder)
{
	return ((pOrder->FrontID == FRONT_ID) &&
		(pOrder->SessionID == SESSION_ID) &&
		(strcmp(pOrder->OrderRef, ORDER_REF) == 0));
}

bool TdSpi::IsTradingOrder(CThostFtdcOrderField* pOrder)
{
	return ((pOrder->OrderStatus != THOST_FTDC_OST_PartTradedNotQueueing) &&
		(pOrder->OrderStatus != THOST_FTDC_OST_Canceled) &&
		(pOrder->OrderStatus != THOST_FTDC_OST_AllTraded));
}

void TdSpi::OnRspError(CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	IsErrorRspInfo(pRspInfo);
}

void TdSpi::OnFrontDisconnected(int nReason)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "--->>> Reason = " << nReason << endl;
}

void TdSpi::OnHeartBeatWarning(int nTimeLapse)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "--->>> nTimerLapse = " << nTimeLapse << endl;
}

void TdSpi::OnRspOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	IsErrorRspInfo(pRspInfo);
}

void TdSpi::OnRspOrderAction(CThostFtdcInputOrderActionField* pInputOrderAction, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	IsErrorRspInfo(pRspInfo);
}
