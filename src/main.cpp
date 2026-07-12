#include <vector>
#include <ThostFtdcTraderApi.h>
#include <ThostFtdcMdApi.h>
#include <ThostFtdcUserApiDataType.h>
#include <ThostFtdcUserApiStruct.h>
#include <iostream>
#include <fstream>
#include <mutex>
#include <map>
#include <cstring>
#include <ThostFtdcMdApi.h>
#include "mdspi.h"
#include "tdspi.h"
#include "DataBuffer.h"
#include "indicator.h"
#include <algorithm>
#include <deque>
#include <cstdio>
#include <string>

using namespace std;

// 渲染每个窗口

map<string, string> accountConfig_map;//保存账户信息的map
std::map<std::string, std::string> contracts_map;//保存合约信息的表
map<string, CThostFtdcInstrumentField> g_mapInstruments; //保存所订阅的合约信息
vector<pair<string, string>> g_vctIFSpreads; //做套利用到两个合约的名称
//char** ppInstrumentID; //保存需要订阅的合约

//string g_strLeg1 = "sc2608";
//string g_strLeg2 = "sc2610";

void ReadConfigMap(map<string, string>& accountmap);
void ReadContracts(map<std::string, std::string>& contractmap);

CThostFtdcTraderApi* pTDUserApi;
CThostFtdcMdApi* pMDUserApi;
CDataBuffer* g_pDataBuffer;
CIndicator* g_pCIndicator;
//CThostFtdcTraderApi* pTDUserApi = nullptr;


char AUTHCODE[] = "0000000000000000";
char APPID[] = "simnow_client_test";
TThostFtdcBrokerIDType	BROKER_ID;
TThostFtdcInvestorIDType INVESTOR_ID;
TThostFtdcPasswordType	PASSWORD;

//const char *ppInstrumentID[]={"IF2412","IF2501"};
int iInstrumentID;
int iRequestID; // 请求编号
vector<string> g_vctIFCodes;

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

static std::string ProjectPath(const char* relativePath)
{
	return std::string(PROJECT_SOURCE_DIR) + "/" + relativePath;
}

int main()
{
	cerr << "---------------------------------------------" << endl;
	cerr << "---------------------------------------------" << endl;
	cerr << "-------------CTP高频交易系统启动-------------" << endl;
	cerr << "---------------------------------------------" << endl;
	cerr << "---------------------------------------------" << endl;
	//-----------------1、读取账户信息和订阅的合约信息-------------------
	ReadConfigMap(accountConfig_map);
	ReadContracts(contracts_map);
	std::snprintf(BROKER_ID, sizeof(BROKER_ID), "%s", accountConfig_map["brokerId"].c_str());
	std::snprintf(INVESTOR_ID, sizeof(INVESTOR_ID), "%s", accountConfig_map["userId"].c_str());
	std::snprintf(PASSWORD, sizeof(PASSWORD), "%s", accountConfig_map["passwd"].c_str());
	std::snprintf(AUTHCODE, sizeof(AUTHCODE), "%s", accountConfig_map["authcode"].c_str());
	std::snprintf(APPID, sizeof(APPID), "%s", accountConfig_map["appid"].c_str());

	//-----------------2、创建行情Api和回调类实例------------------------
	//-----------------参考ctp函数手册中的行情/交易接口API---------------
	std::string marketFlowPath = ProjectPath("flow/marketflow/");
	std::string tradeFlowPath = ProjectPath("flow/tradeflow/");
	CThostFtdcMdApi* pMDUserApi = CThostFtdcMdApi::CreateFtdcMdApi(marketFlowPath.c_str());
	const char* ver_Md = CThostFtdcMdApi::GetApiVersion();
	printf("行情API版本：%s\n", ver_Md);
	MdSpi* pMDUserSpi = new MdSpi(pMDUserApi);
	pMDUserApi->RegisterSpi(pMDUserSpi);

	//-----------------3、创建交易Api和回调类实例------------------------
	pTDUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi(tradeFlowPath.c_str());
	TdSpi* pTDUserSpi = new TdSpi();
	pTDUserApi->RegisterSpi(pTDUserSpi);//api注册回调类

	pTDUserApi->SubscribePublicTopic(THOST_TERT_RESTART);//订阅公有流
	pTDUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);//订阅私有流

	//恢复策略类指针初始化，并设置策略的初始状态
    g_pDataBuffer = new CDataBuffer();
    g_pCIndicator = new CIndicator();

	//--------------启动行情线程，行情线程引导交易线程-
	char mdFront[50];
	std::snprintf(mdFront, sizeof(mdFront), "%s", accountConfig_map["MarketFront"].c_str());
	pMDUserApi->RegisterFront(mdFront);
	pMDUserApi->Init();

	
	//--------------阻塞行情与交易线程-----------------
	pMDUserApi->Join();
	pTDUserApi->Join();
	pMDUserApi->Release();
	pTDUserApi->Release();
	return 0;
}


void ReadContracts(map<std::string, std::string>& contractmap)
{
	std::ifstream file2(ProjectPath("config/contracts.txt"), ios::in);
	string fieldKey;
	string fieldValue;
	char dataLine[256];
	if (!file2)
	{
		cout << "配置文件不存在" << endl;
		return;
	}
	else
	{
		while (file2.getline(dataLine, sizeof(dataLine), '\n'))
		{
			int length = strlen(dataLine);
			char tmp[128];
			for (int i = 0, j = 0, count = 0; i < length + 1; i++)
			{
				if (dataLine[i] != ',' && dataLine[i] != '\0')
					tmp[j++] = dataLine[i];
				else
				{

					tmp[j] = '\0';
					count++;
					j = 0;
					switch (count)
					{
					case 1:
						fieldKey = tmp;
						break;
					case 2:
						fieldValue = tmp;
					default:
						break;
					}
				}
			}
			contractmap.insert(make_pair(fieldKey, fieldValue));
		}
	}
	file2.close();
}

void ReadConfigMap(map<std::string, std::string>& accountmap)
{
	std::ifstream file1(ProjectPath("config/account.local.txt"), ios::in);
	string fieldKey;
	string fieldValue;
	char dataLine[256];
	if (!file1)
	{
		cout << "配置文件不存在" << endl;
		return;
	}
	else
	{
		while (file1.getline(dataLine, sizeof(dataLine), '\n'))
		{
			int length = strlen(dataLine);
			char tmp[128];
			for (int i = 0, j = 0, count = 0; i < length + 1; i++)
			{
				if (dataLine[i] != ',' && dataLine[i] != '\0')
					tmp[j++] = dataLine[i];
				else
				{
					tmp[j] = '\0';
					count++;
					j = 0;
					switch (count)
					{
					case 1:
						fieldKey = tmp;
						break;
					case 2:
						fieldValue = tmp;
					default:
						break;
					}
				}
			}
			accountmap.insert(make_pair(fieldKey, fieldValue));
		}
	}
	file1.close();
} 
