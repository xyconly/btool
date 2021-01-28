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
		// ��ע:      
		// ������:    SendMessage
		// ����ȫ��:  CHXNetWebsocket::SendMessage
		// ����Ȩ��:  virtual public 
		// ����ֵ:    int
		// ˵��:      ��������Ϣ
		// ����: 	  char * pData   
		// ����: 	  int nLen   
		//************************************
		virtual int			sendMessage(char* pData, int nLen) = 0;



		//////////////////////////////////////////////////////////////////////////
		///�����
	public:
		//************************************
		// ��ע:      
		// ������:    StartNetService
		// ����ȫ��:  HXNet::CHXNetServerImpl::StartNetService
		// ����Ȩ��:  virtual public 
		// ����ֵ:    int
		// ˵��:      ��������
		// ����: 	  int nPort   
		// ����: 	  char * strIp   
		//************************************
		virtual int					connect(char* strIp, int nPort) = 0;

		//************************************
		// ��ע:      
		// ������:    EndNetService
		// ����ȫ��:  HXNet::CHXNetServerImpl::EndNetService
		// ����Ȩ��:  virtual public 
		// ����ֵ:    bool
		// ˵��:      ��ֹ�������
		//************************************
		virtual bool				disconnect() = 0;

		//************************************
		// ��ע:      
		// ������:    RegisterCallBack
		// ����ȫ��:  HXNet::CHXNetServerImpl::RegisterCallBack
		// ����Ȩ��:  virtual public 
		// ����ֵ:    int
		// ˵��:      ���ûص�����
		// ����: 	  CHXNetServerCallBack * pCallBackFun   
		//************************************
		virtual void					registerCallBack(CHXWebsocketClientCallBack* pCallBackFun) = 0;
	};

}


