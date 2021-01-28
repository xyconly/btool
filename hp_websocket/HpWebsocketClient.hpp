#pragma once

//////////////////////////////////////////////////////////////////////////
// 预定义 HP_SOCKET_STATIC_LIB   则HPSOCKET为静态库连接，即只需要一个HpWebsocketClient.dll
// 如果没有定义 HP_SOCKET_STATIC_LIB 则需要两个动态库(HpWebsocketClient.dll和HPSocket4C.dll)
#ifdef HP_SOCKET_STATIC_LIB

# if defined( _DEBUG ) || defined (__DEBUG__)
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Debug_static/HpWebsocketClient.lib")
#     else
#  	     pragma comment(lib,"x64Debug_static/HpWebsocketClient.lib")
#     endif
#else
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Release_static/HpWebsocketClient.lib")
#     else
#  	     pragma comment(lib,"x64Release_static/HpWebsocketClient.lib")
#     endif
# endif

#else

# if defined( _DEBUG ) || defined (__DEBUG__)
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Debug/HpWebsocketClient.lib")
#     else
#  	     pragma comment(lib,"x64Debug/HpWebsocketClient.lib")
#     endif
#else
#     ifndef _M_X64
#  	     pragma comment(lib,"Win32Release/HpWebsocketClient.lib")
#     else
#  	     pragma comment(lib,"x64Release/HpWebsocketClient.lib")
#     endif
# endif

#endif // HP_SOCKET_STATIC_LIB

#include "HPSocket/HPSocket4C.h"
#include "hp_websocket/HXWebsocketClient.h"






