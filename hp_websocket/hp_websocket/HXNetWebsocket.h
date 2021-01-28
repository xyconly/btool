#pragma once
#include <memory>

#ifdef HXWEBSOCKETAPI_EXPORTS
#define HXWEBSOCKET_API __declspec(dllexport)
#else
#define HXWEBSOCKET_API __declspec(dllimport)
#endif

namespace HXWebsocket
{
	class CHXWebsocket;
	class CHXWebsocketCallBack;

	HXWEBSOCKET_API CHXWebsocket* createServer();
	HXWEBSOCKET_API void delServer(CHXWebsocket * pServer);

	class CHXWebsocketCallBack
	{
	public:
		virtual int			OnWSMessage(unsigned int uiConnID, char* pData, int nLen) { return 0; };
		virtual int			OnWSError(unsigned int uiConnID, int nErrorCode, char* szErrorDesc) { return 0; };
	};

	class CHXWebsocket
	{
	public:
		virtual ~CHXWebsocket() {};

		//************************************
		// 备注:      
		// 函数名:    SendMessage
		// 函数全名:  CHXNetWebsocket::SendMessage
		// 访问权限:  virtual public 
		// 返回值:    int
		// 说明:      发送送消息
		// 参数: 	  unsigned int uiConnID   
		// 参数: 	  char * pData   
		// 参数: 	  int nLen   
		//************************************
		virtual int			sendMessage(unsigned int uiConnID, char* pData, int nLen) = 0;



		//////////////////////////////////////////////////////////////////////////
		///服务端
	public:
		//************************************
		// 备注:      
		// 函数名:    StartNetService
		// 函数全名:  HXNet::CHXNetServerImpl::StartNetService
		// 访问权限:  virtual public 
		// 返回值:    int
		// 说明:      启动服务
		// 参数: 	  int nPort   
		// 参数: 	  char * strIp   
		//************************************
		virtual int					startNetService(int nPort, wchar_t* strIp = nullptr) = 0;

		//************************************
		// 备注:      
		// 函数名:    EndNetService
		// 函数全名:  HXNet::CHXNetServerImpl::EndNetService
		// 访问权限:  virtual public 
		// 返回值:    bool
		// 说明:      终止网络服务
		//************************************
		virtual bool				endNetService() = 0;

		//************************************
		// 备注:      
		// 函数名:    RegisterCallBack
		// 函数全名:  HXNet::CHXNetServerImpl::RegisterCallBack
		// 访问权限:  virtual public 
		// 返回值:    int
		// 说明:      设置回调函数
		// 参数: 	  CHXNetServerCallBack * pCallBackFun   
		//************************************
		virtual void					registerCallBack(CHXWebsocketCallBack* pCallBackFun) = 0;
	};

}