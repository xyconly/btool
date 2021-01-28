#pragma once

//////////////////////////////////////////////////////////////////////////
// Ԥ���� HP_SOCKET_STATIC_LIB   ��HPSOCKETΪ��̬�����ӣ���ֻ��Ҫһ��HpWebsocketServer.dll
// ���û�ж��� HP_SOCKET_STATIC_LIB ����Ҫ������̬��(HpWebsocketServer.dll��HPSocket4C.dll)
#ifdef HP_SOCKET_STATIC_LIB

# if defined( _DEBUG ) || defined (__DEBUG__)
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Debug_static/HpWebsocketServer.lib")
#     else
#  	     pragma comment(lib,"x64Debug_static/HpWebsocketServer.lib")
#     endif
#else
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Release_static/HpWebsocketServer.lib")
#     else
#  	     pragma comment(lib,"x64Release_static/HpWebsocketServer.lib")
#     endif
# endif

#else

# if defined( _DEBUG ) || defined (__DEBUG__)
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Debug/HpWebsocketServer.lib")
#     else
#  	     pragma comment(lib,"x64Debug/HpWebsocketServer.lib")
#     endif
#else
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Release/HpWebsocketServer.lib")
#     else
#  	     pragma comment(lib,"x64Release/HpWebsocketServer.lib")
#     endif
# endif

#endif // HP_SOCKET_STATIC_LIB

#include "HPSocket/HPSocket4C.h"
#include "hp_websocket/HXNetWebsocket.h"






