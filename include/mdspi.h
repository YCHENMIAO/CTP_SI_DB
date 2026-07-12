#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"
#include <string>
#include <vector>
#include <sstream>




using namespace std;
class MdSpi :public CThostFtdcMdSpi {
public:
	///错误应答
	MdSpi(CThostFtdcMdApi* mdapi);
	~MdSpi();
	virtual void OnRspError(CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);
	virtual void OnHeartBeatWarning(int nTimeLapse);
	///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
	///@param nReason 错误原因
///        0x1001 网络读失败
///        0x1002 网络写失败
///        0x2001 接收心跳超时
///        0x2002 发送心跳失败
///        0x2003 收到错误报文
	void OnFrontConnected();
	virtual void OnFrontDisconnected(int nReason);
	///登录请求响应
	void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo,
		int nRequestID, bool bIsLast);
	///订阅行情应答
	void OnRspSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo,
		int nRequestID, bool bIsLast);
	///取消订阅行情应答
	void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo,
		int nRequestID, bool bIsLast);
	///深度行情通知
	void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData);
	///@param nTimeLapse 距离上次接收报文的时间

	void ReqUserLogin();
	void SubscribeMarketData(const std::vector<std::string>& instruments);
	bool IsErrorRspInfo(CThostFtdcRspInfoField* pRspInfo);
private:
	CThostFtdcMdApi* mdapi;
	CThostFtdcTraderApi* tdapi = nullptr;
	CThostFtdcReqUserLoginField* loginField = nullptr;
};
