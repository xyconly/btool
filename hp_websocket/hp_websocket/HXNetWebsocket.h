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
		// ��ע:      
		// ������:    SendMessage
		// ����ȫ��:  CHXNetWebsocket::SendMessage
		// ����Ȩ��:  virtual public 
		// ����ֵ:    int
		// ˵��:      ��������Ϣ
		// ����: 	  unsigned int uiConnID   
		// ����: 	  char * pData   
		// ����: 	  int nLen   
		//************************************
		virtual int			sendMessage(unsigned int uiConnID, char* pData, int nLen) = 0;



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
		virtual int					startNetService(int nPort, wchar_t* strIp = nullptr) = 0;

		//************************************
		// ��ע:      
		// ������:    EndNetService
		// ����ȫ��:  HXNet::CHXNetServerImpl::EndNetService
		// ����Ȩ��:  virtual public 
		// ����ֵ:    bool
		// ˵��:      ��ֹ�������
		//************************************
		virtual bool				endNetService() = 0;

		//************************************
		// ��ע:      
		// ������:    RegisterCallBack
		// ����ȫ��:  HXNet::CHXNetServerImpl::RegisterCallBack
		// ����Ȩ��:  virtual public 
		// ����ֵ:    int
		// ˵��:      ���ûص�����
		// ����: 	  CHXNetServerCallBack * pCallBackFun   
		//************************************
		virtual void					registerCallBack(CHXWebsocketCallBack* pCallBackFun) = 0;
	};

}