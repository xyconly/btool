#pragma once

#ifdef HXWEBSOCKETCLIENT_EXPORTS
#define HXWEBSOCKETCLIENT_API __declspec(dllexport)
#else
#define HXWEBSOCKETCLIENT_API __declspec(dllimport)
#endif

namespace HXWebsocket
{
	class CHXWebsocketClient;
	class CHXWebsocketClientCallBack;

    HXWEBSOCKETCLIENT_API CHXWebsocketClient* createClient();
    HXWEBSOCKETCLIENT_API void delClient(CHXWebsocketClient * pServer);

	class CHXWebsocketClientCallBack
	{
	public:
		virtual int			OnWSMessage(char* pData, int nLen) = 0;
		virtual int			OnWSError(int nErrorCode, char* szErrorDesc) = 0;
	};

	class CHXWebsocketClient
	{
	public:
		virtual ~CHXWebsocketClient() {};

		//************************************
		// 备注:      
		// 函数名:    SendMessage
		// 函数全名:  CHXNetWebsocket::SendMessage
		// 访问权限:  virtual public 
		// 返回值:    int
		// 说明:      发送送消息
		// 参数: 	  char * pData   
		// 参数: 	  int nLen   
		//************************************
		virtual int			sendMessage(char* pData, int nLen) = 0;



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
		virtual int					connect(char* strIp, int nPort) = 0;

		//************************************
		// 备注:      
		// 函数名:    EndNetService
		// 函数全名:  HXNet::CHXNetServerImpl::EndNetService
		// 访问权限:  virtual public 
		// 返回值:    bool
		// 说明:      终止网络服务
		//************************************
		virtual bool				disconnect() = 0;

		//************************************
		// 备注:      
		// 函数名:    RegisterCallBack
		// 函数全名:  HXNet::CHXNetServerImpl::RegisterCallBack
		// 访问权限:  virtual public 
		// 返回值:    int
		// 说明:      设置回调函数
		// 参数: 	  CHXNetServerCallBack * pCallBackFun   
		//************************************
		virtual void					registerCallBack(CHXWebsocketClientCallBack* pCallBackFun) = 0;
	};

}


